#pragma once
#include "CandidateRanking.h"
#include "WinUtil.h"
#include <wincrypt.h>
namespace fuyi {
// Personal selections are encrypted for the current Windows user. This file
// is deliberately separate from sandbox-readable non-secret preferences.
class PersonalDictionary {
    CandidateRanking session_, cached_;
    bool cacheLoaded_ = false;
    std::optional<fs::file_time_type> cacheTime_;
    static fs::path path() {
        if (appContainer()) throw std::runtime_error("Sandbox uses session-only ranking");
        auto folder = knownFolder(FOLDERID_LocalAppData) / L"FloatingTranslate";
        fs::create_directories(folder);
        return folder / L"candidate-use.dat";
    }
    struct Lock {
        HANDLE handle = INVALID_HANDLE_VALUE;
        explicit Lock(const fs::path &file) {
            handle = CreateFileW((file.wstring() + L".lock").c_str(), GENERIC_READ | GENERIC_WRITE,
                                 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
        }
        ~Lock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        explicit operator bool() const { return handle != INVALID_HANDLE_VALUE; }
    };
    static CandidateRanking load(const fs::path &file) {
        CandidateRanking result;
        if (!fs::exists(file) || fs::file_size(file) > 1024 * 1024) return result;
        std::ifstream in(file, std::ios::binary);
        std::vector<BYTE> encrypted((std::istreambuf_iterator<char>(in)), {});
        if (encrypted.empty()) return result;
        DATA_BLOB source{(DWORD)encrypted.size(), encrypted.data()}, plain{};
        if (!CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plain))
            return result;
        std::string data((char *)plain.pbData, plain.cbData);
        SecureZeroMemory(plain.pbData, plain.cbData); LocalFree(plain.pbData);
        std::istringstream stream(data);
        std::string line; size_t entries = 0;
        while (entries++ < CandidateRanking::limit && std::getline(stream, line)) {
            auto a = line.find('\t'), b = line.find('\t', a == std::string::npos ? 0 : a + 1);
            if (a == std::string::npos || b == std::string::npos) continue;
            auto count = std::stoul(line.substr(0, a));
            if (!count || count > 1000000) continue;
            auto raw = line.substr(a + 1, b - a - 1);
            result.add(raw, {wide(line.substr(b + 1)), raw.size()}, (uint32_t)count);
        }
        SecureZeroMemory(data.data(), data.size());
        return result;
    }
    static void save(const fs::path &file, const CandidateRanking &ranking) {
        std::string data;
        for (const auto &e : ranking.entries())
            data += std::to_string(e.count) + "\t" + e.pinyin + "\t" + utf8(e.text) + "\n";
        DATA_BLOB plain{(DWORD)data.size(), (BYTE *)data.data()}, encrypted{};
        if (!CryptProtectData(&plain, L"FloatingTranslate candidate counts", nullptr, nullptr,
                              nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
            throw std::runtime_error("Cannot encrypt candidate counts");
        SecureZeroMemory(data.data(), data.size());
        auto temporary = fs::path(file.wstring() + L".tmp");
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write((char *)encrypted.pbData, encrypted.cbData); out.close();
        LocalFree(encrypted.pbData);
        if (!out || !MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot save candidate counts");
    }
  public:
    void rank(const std::string &raw, std::vector<Candidate> &candidates,
              const std::function<void(Candidate &)> &resolve = {}) {
        auto options = Options::read();
        if (!options.remember || options.privacy) return;
        try {
            auto file = path(); Lock lock(file);
            if (lock) {
                auto time = fs::exists(file) ? std::optional<fs::file_time_type>(fs::last_write_time(file)) : std::nullopt;
                if (!cacheLoaded_ || time != cacheTime_) {
                    cached_ = load(file); cacheTime_ = time; cacheLoaded_ = true;
                }
                cached_.rank(raw, candidates, resolve); return;
            }
        } catch (...) {}
        session_.rank(raw, candidates, resolve);
    }
    void remember(const std::string &raw, const Candidate &candidate) {
        if (!candidate.consumed || candidate.consumed > raw.size() ||
            !memorableCandidate(raw.substr(0, candidate.consumed), candidate.text)) return;
        auto options = Options::read();
        if (!options.remember || options.privacy) return;
        session_.add(raw, candidate);
        try {
            auto file = path(); Lock lock(file); if (!lock) return;
            auto ranking = load(file); ranking.add(raw, candidate); save(file, ranking);
            cached_ = std::move(ranking); cacheTime_ = fs::last_write_time(file); cacheLoaded_ = true;
        } catch (...) {} // Failure must never block committing the selected word.
    }
    static void clear() {
        auto file = path(); Lock lock(file);
        if (!lock) throw std::runtime_error("Candidate counts are busy");
        // Replace with a valid encrypted empty dictionary so active processes
        // immediately see the reset on their next query.
        save(file, CandidateRanking{});
    }
};
} // namespace fuyi
