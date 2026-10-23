#include "Pinyin.h"
#include "Translator.h"
#include <iostream>
namespace fuyi {
HINSTANCE module = nullptr;
}
int wmain() {
    using namespace fuyi;
    module = GetModuleHandleW(nullptr);
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
    SetConsoleOutputCP(CP_UTF8);
    try {
        verifyModels();
        std::cout << "Bundled model SHA-256 checks passed\n";
        if (!pinyin().start())
            throw std::runtime_error("Rime initialization failed");
        for (auto pair : std::vector<std::pair<std::string, std::wstring>>{{"nihao", L"你好"},
                                                                           {"nh", L"你好"},
                                                                           {"zhogn", L"中"},
                                                                           {"xi'an", L"西安"},
                                                                           {"nihoa", L"你好"}}) {
            auto list = pinyin().query(pair.first);
            auto found = std::find_if(list.begin(), list.end(), [&](auto &candidate) {
                return candidate.text == pair.second && candidate.consumed == pair.first.size();
            });
            if (found == list.end())
                throw std::runtime_error("Rime candidate/correction test failed: " + pair.first);
            std::cout << pair.first << " -> " << utf8(found->text) << "\n";
        }
        Translator engine;
        for (auto text : {L"你好", L"我今天去北京。", L"为什么不一起翻译了？", L"你好今天健身了吗？"}) {
            auto result = engine.translate(text, [] { return false; });
            if (result.empty())
                throw std::runtime_error("Translation missing");
            std::cout << utf8(text) << " -> " << utf8(result) << "\n";
        }
        for (auto text : {L"Hello.", L"Have you worked out today?", L"How are you?"}) {
            auto result = engine.translate(text, [] { return false; }, true);
            if (std::none_of(result.begin(), result.end(), [](wchar_t c) { return c >= 0x3400 && c <= 0x9fff; }))
                throw std::runtime_error("English-to-Chinese translation missing");
            std::cout << utf8(text) << " -> " << utf8(result) << "\n";
        }
        std::cout << "Offline component tests passed. No registration or user input was used.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Component test failed: " << e.what() << "\n";
        return 1;
    }
}
