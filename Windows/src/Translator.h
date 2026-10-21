#pragma once
#include "ModelChecks.h"
#include "TranslationModel.h"
#include "WinUtil.h"
#include <bcrypt.h>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace fuyi {
inline std::string fileSha(const fs::path &path) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD size = 0, got = 0;
    std::vector<unsigned char> object, digest(32);
    auto fail = [&] {
        if (hash)
            BCryptDestroyHash(hash);
        if (alg)
            BCryptCloseAlgorithmProvider(alg, 0);
    };
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Hash engine unavailable");
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&size, sizeof(size), &got, 0) < 0) {
        fail();
        throw std::runtime_error("Hash property unavailable");
    }
    object.resize(size);
    if (BCryptCreateHash(alg, &hash, object.data(), size, nullptr, 0, 0) < 0) {
        fail();
        throw std::runtime_error("Hash creation failed");
    }
    std::ifstream in(path, std::ios::binary);
    char buffer[65536];
    if (!in) {
        fail();
        throw std::runtime_error("Missing bundled model");
    }
    while (in) {
        in.read(buffer, sizeof(buffer));
        if (in.gcount() && BCryptHashData(hash, (PUCHAR)buffer, (ULONG)in.gcount(), 0) < 0) {
            fail();
            throw std::runtime_error("Hash failed");
        }
    }
    if (!in.eof() || BCryptFinishHash(hash, digest.data(), 32, 0) < 0) {
        fail();
        throw std::runtime_error("Model read failed");
    }
    fail();
    std::string out;
    const char *digits = "0123456789abcdef";
    for (auto b : digest) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}
inline void verifyModels() {
    for (auto &item : modelChecks)
        if (fileSha(directory() / L"models" / item.file) != item.sha)
            throw std::runtime_error("Bundled model integrity check failed");
}
class Translator {
    std::unique_ptr<TranslationModel> model_;

  public:
    void load() {
        if (model_)
            return;
        verifyModels();
        auto dll = loadLocal(L"onnxruntime.dll");
        if (!dll)
            throw std::runtime_error("ONNX Runtime unavailable");
        auto get = (const OrtApiBase *(ORT_API_CALL *)())GetProcAddress(dll, "OrtGetApiBase");
        if (!get)
            throw std::runtime_error("Invalid ONNX Runtime");
        auto api = get()->GetApi(ORT_API_VERSION);
        if (!api)
            throw std::runtime_error("Incompatible ONNX Runtime");
        Ort::InitApi(api);
        auto model = std::make_unique<TranslationModel>();
        model->load(directory() / L"models", directory() / L"ortextensions.dll");
        model_ = std::move(model);
    }
    std::wstring translate(const std::wstring &source, const std::function<bool()> &cancelled) {
        load();
        return wide(model_->translate(utf8(source), cancelled));
    }
};
struct TranslationResult {
    uint64_t token = 0, ticket = 0;
    std::wstring source, text, status;
};
constexpr UINT WM_TRANSLATED = WM_APP + 101;
struct TranslationTarget {
    std::atomic<uint64_t> generation{0};
    uint64_t token = 0;
    std::atomic<HWND> window{nullptr};
};
class TranslationWorker {
    struct Job {
        std::weak_ptr<TranslationTarget> target;
        uint64_t ticket;
        std::wstring source;
    };
    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<Job> pending_;
    void run() {
        Translator engine;
        for (;;) {
            Job job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [&] { return pending_.has_value(); });
                job = std::move(*pending_);
                pending_.reset();
            }
            auto target = job.target.lock();
            if (!target || target->generation != job.ticket)
                continue;
            auto cancelled = [&] {
                return !target->window.load() || target->generation.load() != job.ticket;
            };
            // Debounce without keeping a queue of old input snapshots.
            {
                std::unique_lock lock(mutex_);
                if (wake_.wait_for(lock, std::chrono::milliseconds(280),
                                   [&] { return pending_.has_value() || cancelled(); }))
                    continue;
            }
            auto result = std::make_unique<TranslationResult>();
            result->token = target->token;
            result->ticket = job.ticket;
            result->source = job.source;
            try {
                result->text = engine.translate(job.source, cancelled);
            } catch (const std::length_error &) {
                result->status = L"句子较长，请分句输入";
            } catch (...) {
                result->status = L"本机翻译不可用，请打开设置检查完整文件";
            }
            if (cancelled())
                continue;
            auto window = target->window.load();
            if (window && PostMessageW(window, WM_TRANSLATED, 0, (LPARAM)result.get()))
                result.release();
        }
    }

  public:
    TranslationWorker() {
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           (LPCWSTR)&verifyModels, &pinned);
        std::thread([this] { run(); }).detach();
    }
    void submit(std::shared_ptr<TranslationTarget> target, uint64_t ticket, std::wstring source) {
        {
            std::lock_guard lock(mutex_);
            pending_ = Job{target, ticket, std::move(source)};
        }
        wake_.notify_all();
    }
};
inline TranslationWorker &worker() {
    static auto *value = new TranslationWorker;
    return *value;
}
} // namespace fuyi
