#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#define ORT_API_MANUAL_INIT
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#endif
#include <onnxruntime_cxx_api.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
namespace fuyi {
class TranslationModel {
    Ort::Env env_{nullptr};
    Ort::Session tokenizer_{nullptr}, encoder_{nullptr}, decoder_{nullptr};
    Ort::AllocatorWithDefaultOptions allocator_{nullptr};
    std::vector<int32_t> map_;
    std::vector<std::string> vocab_;
    static uint32_t number(std::ifstream &in) {
        uint32_t n = 0;
        in.read((char *)&n, 4);
        if (!in)
            throw std::runtime_error("Invalid vocabulary");
        return n;
    }

  public:
    void load(const std::filesystem::path &base, const std::filesystem::path &extension) {
        if (encoder_)
            return;
        auto api = &Ort::GetApi();
        env_ = Ort::Env(ORT_LOGGING_LEVEL_ERROR, "FuyiOffline");
        Ort::ThrowOnError(api->DisableTelemetryEvents(env_));
        allocator_ = Ort::AllocatorWithDefaultOptions();
        std::ifstream mappings(base / "sp-map.bin", std::ios::binary);
        uint32_t count = number(mappings);
        if (count > 100000)
            throw std::runtime_error("Invalid mapping size");
        map_.resize(count);
        mappings.read((char *)map_.data(), count * 4);
        if (!mappings)
            throw std::runtime_error("Invalid mapping");
        std::ifstream vocabulary(base / "vocab.bin", std::ios::binary);
        count = number(vocabulary);
        if (count != 65001)
            throw std::runtime_error("Invalid vocabulary size");
        for (uint32_t i = 0; i < count; i++) {
            auto len = number(vocabulary);
            if (len > 1024)
                throw std::runtime_error("Invalid vocabulary token");
            std::string token(len, 0);
            vocabulary.read(token.data(), len);
            if (!vocabulary)
                throw std::runtime_error("Invalid vocabulary token");
            vocab_.push_back(token);
        }
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2);
        options.SetInterOpNumThreads(1);
        options.DisableMemPattern();
        options.DisableCpuMemArena();
        Ort::ThrowOnError(api->RegisterCustomOpsLibrary_V2(options, extension.c_str()));
        tokenizer_ = Ort::Session(env_, (base / "tokenizer.onnx").c_str(), options);
        encoder_ = Ort::Session(env_, (base / "encoder_model_quantized.onnx").c_str(), options);
        decoder_ = Ort::Session(env_, (base / "decoder_model_quantized.onnx").c_str(), options);
    }
    std::string translate(const std::string &source, const std::function<bool()> &cancelled) {
        if (cancelled())
            return {};
        auto input = source;
        const char *value = input.c_str();
        int64_t one = 1;
        auto text = Ort::Value::CreateTensor(allocator_, &one, 1, ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING);
        text.FillStringTensor(&value, 1);
        const char *ti[] = {"text"};
        const char *to[] = {"ids"};
        auto tokenized = tokenizer_.Run(Ort::RunOptions{nullptr}, ti, &text, 1, to, 1);
        auto info = tokenized[0].GetTensorTypeAndShapeInfo();
        size_t n = info.GetElementCount();
        if (n > 256)
            throw std::length_error("Sentence too long");
        const auto *pieces = tokenized[0].GetTensorData<int64_t>();
        std::vector<int64_t> ids;
        for (size_t i = 0; i < n; i++) {
            if (pieces[i] < 0 || (size_t)pieces[i] >= map_.size())
                throw std::runtime_error("Invalid source token");
            ids.push_back(map_[pieces[i]]);
        }
        ids.push_back(0);
        std::vector<int64_t> mask(ids.size(), 1);
        int64_t dims[] = {1, (int64_t)ids.size()};
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> inputs;
        inputs.push_back(Ort::Value::CreateTensor<int64_t>(memory, ids.data(), ids.size(), dims, 2));
        inputs.push_back(Ort::Value::CreateTensor<int64_t>(memory, mask.data(), mask.size(), dims, 2));
        const char *ei[] = {"input_ids", "attention_mask"};
        const char *eo[] = {"last_hidden_state"};
        auto hidden = encoder_.Run(Ort::RunOptions{nullptr}, ei, inputs.data(), 2, eo, 1);
        std::vector<int64_t> output{65000};
        std::string english;
        const char *di[] = {"input_ids", "encoder_attention_mask", "encoder_hidden_states"};
        const char *dout[] = {"logits"};
        for (int step = 0; step < 128; step++) {
            if (cancelled())
                return {};
            int64_t shape[] = {1, (int64_t)output.size()};
            auto tokens = Ort::Value::CreateTensor<int64_t>(memory, output.data(), output.size(), shape, 2);
            const OrtValue *values[] = {tokens, inputs[1], hidden[0]};
            OrtValue *result = nullptr;
            Ort::RunOptions run;
            Ort::ThrowOnError(Ort::GetApi().Run(decoder_, run, di, values, 3, dout, 1, &result));
            Ort::Value logits(result);
            auto *scores = logits.GetTensorData<float>() + (output.size() - 1) * 65001;
            size_t best = (size_t)(std::max_element(scores, scores + 65000) - scores);
            if (best == 0) {
                for (size_t p = english.find("▁"); p != std::string::npos; p = english.find("▁", p + 1))
                    english.replace(p, 3, " ");
                auto start = english.find_first_not_of(" \n\t");
                if (start == std::string::npos)
                    return {};
                auto end = english.find_last_not_of(" \n\t");
                return english.substr(start, end - start + 1);
            }
            english += vocab_[best] == "<unk>" ? "�" : vocab_[best];
            output.push_back((int64_t)best);
        }
        throw std::length_error("Sentence too long");
    }
};
} // namespace fuyi
