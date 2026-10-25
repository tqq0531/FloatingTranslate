#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <cmath>
#include <cctype>
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
    int64_t targetLanguage_ = -1;
    static uint32_t number(std::ifstream &in) {
        uint32_t n = 0;
        in.read((char *)&n, 4);
        if (!in)
            throw std::runtime_error("Invalid vocabulary");
        return n;
    }

  public:
    void load(const std::filesystem::path &base, const std::filesystem::path &extension, bool chineseTarget = false) {
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
        if (chineseTarget) {
            auto token = std::find(vocab_.begin(), vocab_.end(), ">>cmn_Hans<<");
            if (token == vocab_.end()) throw std::runtime_error("Simplified Chinese target token missing");
            targetLanguage_ = (int64_t)(token - vocab_.begin());
        }
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2);
        options.SetInterOpNumThreads(1);
        // Reuse decoder allocations across tokens and consecutive sentences.
        options.EnableMemPattern();
        options.EnableCpuMemArena();
        Ort::ThrowOnError(api->RegisterCustomOpsLibrary_V2(options, extension.c_str()));
        tokenizer_ = Ort::Session(env_, (base / "tokenizer.onnx").c_str(), options);
        encoder_ = Ort::Session(env_, (base / "encoder_model_quantized.onnx").c_str(), options);
        decoder_ = Ort::Session(env_, (base / "decoder_model_quantized.onnx").c_str(), options);
    }
    std::string translate(const std::string &source, const std::function<bool()> &cancelled) {
        if (cancelled())
            return {};
        auto input = source;
        // Isolated greetings are ambiguous to the small multilingual model.
        // Exact whole-input matches only; never rewrite words inside a sentence.
        if (targetLanguage_ >= 0) {
            std::string greeting = source;
            std::transform(greeting.begin(), greeting.end(), greeting.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
            auto begin = greeting.find_first_not_of(" \t\r\n");
            auto end = greeting.find_last_not_of(" \t\r\n.!?");
            if (begin != std::string::npos && end != std::string::npos) greeting = greeting.substr(begin,end-begin+1);
            if (greeting == "hi" || greeting == "hello" || greeting == "hey") return "你好";
            for (const auto &prefix : {std::string("hi "), std::string("hello "), std::string("hey "), std::string("hi, "), std::string("hello, "), std::string("hey, ")}) {
                if (begin == 0 && greeting.rfind(prefix, 0) == 0) {
                    auto rest = translate(source.substr(prefix.size()), cancelled);
                    return rest.empty() ? std::string{} : "你好，" + rest;
                }
            }

        }
        // Give fragmentary English the sentence boundary expected by Marian.
        if (targetLanguage_ >= 0 && !input.empty() &&
            ((input.back() >= 'a' && input.back() <= 'z') || (input.back() >= 'A' && input.back() <= 'Z')))
            input += ".";
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
        if (targetLanguage_ >= 0) ids.push_back(targetLanguage_);
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
        // Very short, unpunctuated English utterances can make this subtitle-
        // trained model invent a second sentence. End at its first full stop.
        // Longer/compound input keeps normal EOS decoding.
        size_t words = 0; bool inWord = false;
        for (unsigned char c : source) { bool letter = std::isalnum(c) != 0; if(letter && !inWord)++words; inWord=letter; }
        std::string lower = source;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});
        bool shortUtterance = targetLanguage_ >= 0 && words <= 8 &&
            source.find_first_of(".,;:!?\n") == std::string::npos &&
            lower.find(" and ")==std::string::npos && lower.find(" but ")==std::string::npos &&
            lower.find(" because ")==std::string::npos;
        struct Beam { std::vector<int64_t> tokens; double score; bool done; };
        std::vector<Beam> beams{{{65000}, 0, false}};
        const char *di[] = {"input_ids", "encoder_attention_mask", "encoder_hidden_states"};
        const char *dout[] = {"logits"};
        auto rank = [](const Beam &b) { return b.score; };
        for (int step = 0; step < 128; ++step) {
            std::vector<Beam> next;
            std::vector<size_t> active;
            for (size_t i=0;i<beams.size();++i) {
                if (beams[i].done) next.push_back(beams[i]);
                else active.push_back(i);
            }
            if (cancelled()) return {};
            const size_t length=beams[active.front()].tokens.size();
            std::vector<int64_t> tokenBatch, maskBatch;
            std::vector<float> hiddenBatch;
            auto hiddenInfo=hidden[0].GetTensorTypeAndShapeInfo();
            auto hiddenShape=hiddenInfo.GetShape();
            size_t hiddenCount=hiddenInfo.GetElementCount();
            auto hiddenData=hidden[0].GetTensorData<float>();
            for (auto index:active) {
                tokenBatch.insert(tokenBatch.end(),beams[index].tokens.begin(),beams[index].tokens.end());
                maskBatch.insert(maskBatch.end(),mask.begin(),mask.end());
                hiddenBatch.insert(hiddenBatch.end(),hiddenData,hiddenData+hiddenCount);
            }
            int64_t tokenShape[]={(int64_t)active.size(),(int64_t)length};
            int64_t maskShape[]={(int64_t)active.size(),(int64_t)mask.size()};
            hiddenShape[0]=(int64_t)active.size();
            auto batchTokens=Ort::Value::CreateTensor<int64_t>(memory,tokenBatch.data(),tokenBatch.size(),tokenShape,2);
            auto batchMask=Ort::Value::CreateTensor<int64_t>(memory,maskBatch.data(),maskBatch.size(),maskShape,2);
            auto batchHidden=Ort::Value::CreateTensor<float>(memory,hiddenBatch.data(),hiddenBatch.size(),hiddenShape.data(),hiddenShape.size());
            const OrtValue *values[]={batchTokens,batchMask,batchHidden};
            OrtValue *result=nullptr;
            Ort::RunOptions run;
            Ort::ThrowOnError(Ort::GetApi().Run(decoder_,run,di,values,3,dout,1,&result));
            Ort::Value logits(result);
            for (size_t batch=0;batch<active.size();++batch) {
                if (cancelled()) return {};
                auto &beam=beams[active[batch]];
                auto &output=beam.tokens;
                auto *scores=logits.GetTensorData<float>()+(batch*length+length-1)*65001;
                double maximum = *std::max_element(scores, scores+65000), sum = 0;
                for(size_t i=0;i<65000;++i) sum += std::exp(scores[i]-maximum);
                double normalizer = maximum + std::log(sum);
                std::vector<float> adjusted(scores,scores+65000);
                // Prevent decoder loops without deleting genuine source repetitions.
                if(output.size()>=4) for(size_t i=1;i+3<output.size();++i)
                    if(output[i]==output[output.size()-3] && output[i+1]==output[output.size()-2] && output[i+2]==output.back())
                        adjusted[output[i+3]]=-std::numeric_limits<float>::infinity();
                for(int k=0;k<4;++k) {
                    size_t best=std::max_element(adjusted.begin(),adjusted.end())-adjusted.begin();
                    Beam candidate=beam;
                    candidate.tokens.push_back(best);
                    candidate.score += adjusted[best]-normalizer;
                    candidate.done=best==0 || (shortUtterance && output.size()>2 &&
                        vocab_[best].find("。")!=std::string::npos);
                    next.push_back(std::move(candidate));
                    adjusted[best]=-std::numeric_limits<float>::infinity();
                }
            }
            std::sort(next.begin(),next.end(),[&](const Beam&a,const Beam&b){return rank(a)>rank(b);});
            if(next.size()>4) next.resize(4);
            beams=std::move(next);
            if(beams.front().done) {
                std::string text;
                for(size_t i=1;i<beams.front().tokens.size() && beams.front().tokens[i]!=0;++i) {
                    auto &piece=vocab_[beams.front().tokens[i]];
                    text += piece=="<unk>" ? "�" : piece;
                }
                for(size_t p=text.find("▁");p!=std::string::npos;p=text.find("▁",p+1))text.replace(p,3," ");
                if (targetLanguage_ >= 0) {
                    for (const auto &pattern : {std::string("。 。"), std::string("。。")})
                        for (size_t p=text.find(pattern); p!=std::string::npos; p=text.find(pattern)) text.replace(p,pattern.size(),"。");
                }
                auto start=text.find_first_not_of(" \n\t");
                if(start==std::string::npos)return {};
                return text.substr(start,text.find_last_not_of(" \n\t")-start+1);
            }
        }
        throw std::length_error("Sentence too long");
    }
};
} // namespace fuyi
