#include "../src/Pinyin.h"
#include "../src/Translator.h"
#include <cassert>
#include <chrono>
#include <iostream>
namespace fuyi { HINSTANCE module = nullptr; }
using namespace fuyi;
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    // Locate the existing bundle without registering an input service.
    module = LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    assert(module);
    SetConsoleOutputCP(CP_UTF8);
    assert(pinyin().start());
    for (const auto &raw : {"nihao", "nh", "zhogn", "xi'an", "nihoa", "wojintianqubeijing", "nihaojintianjianshenlema"}) {
        auto eager = pinyin().query(raw);
        auto lazy = pinyin().query(raw, true);
        assert(eager.size() == lazy.size() && !eager.empty());
        for (size_t i = 0; i < eager.size(); ++i) {
            pinyin().resolve(raw, lazy[i]);
            assert(eager[i].text == lazy[i].text);
            assert(eager[i].consumed == lazy[i].consumed && lazy[i].consumed);
        }
        assert(pinyin().preview(raw).has_value());
    }
    auto elapsed = [](auto action) {
        auto start = std::chrono::steady_clock::now(); action();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
    };
    auto stream = [&](bool lazy) {
        for (std::string raw : {"nihaojintianjianshenlema", "wojintianqubeijing"})
            for (size_t length = 1; length <= raw.size(); ++length) {
                auto input = raw.substr(0, length);
                auto list = pinyin().query(input, lazy);
                if (!list.empty()) pinyin().resolve(input, list[0]);
            }
    };
    auto oldMs = elapsed([&] { stream(false); });
    auto newMs = elapsed([&] { stream(true); });
    std::cout << "Pinyin typing stream, eager: " << oldMs << " ms; lazy: " << newMs << " ms\n";
    Translator model;
    for (auto text : {L"我今天去北京。", L"我明天去北京。", L"我今天坐火车去北京。"}) {
        std::wstring translated;
        auto ms = elapsed([&] { translated = model.translate(text, [] { return false; }); });
        assert(!translated.empty());
        std::cout << utf8(text) << " -> " << utf8(translated) << " (" << ms << " ms)\n";
    }
    assert(model.translate(L"already cancelled", [] { return true; }).empty());
    auto compound = model.translate(L"I like music. I also like swimming.", [] { return false; }, true);
    assert(compound.find(L"音乐") != std::wstring::npos && compound.find(L"游泳") != std::wstring::npos);
    auto name = model.translate(L"Hello, my name is Jack", [] { return false; }, true);
    assert(name.find(L"你好") != std::wstring::npos && name.find(L"杰克") != std::wstring::npos);
    int checks = 0;
    assert(model.translate(L"Please send me the document tomorrow.", [&] { return ++checks > 3; }, true).empty());
    bool rejected = false;
    try { model.translate(std::wstring(4000, L'我'), [] { return false; }); }
    catch (const std::length_error &) { rejected = true; }
    assert(rejected);
    std::cout << "All candidate pages/consumed lengths, preview and real inference passed\n";
}
