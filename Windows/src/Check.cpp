#include "Pinyin.h"
#include "Translator.h"
#include <iostream>
#include <aclapi.h>
namespace fuyi {
HINSTANCE module = nullptr;
}
static bool sandboxReadAcl(const std::filesystem::path &path,const wchar_t *sidText) {
    PSECURITY_DESCRIPTOR security=nullptr;PACL acl=nullptr;PSID sid=nullptr;
    if(!ConvertStringSidToSidW(sidText,&sid))return false;
    DWORD error=GetNamedSecurityInfoW((LPWSTR)path.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&security);
    TRUSTEEW trustee{};BuildTrusteeWithSidW(&trustee,sid);ACCESS_MASK rights=0;
    bool ok=error==ERROR_SUCCESS && acl && GetEffectiveRightsFromAclW(acl,&trustee,&rights)==ERROR_SUCCESS &&
        (rights&(GENERIC_READ|FILE_READ_DATA))!=0 && (rights&(GENERIC_WRITE|FILE_WRITE_DATA))==0;
    if(security)LocalFree(security);LocalFree(sid);return ok;
}
int wmain(int argc,wchar_t** argv) {
    using namespace fuyi;
    module = GetModuleHandleW(nullptr);
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
    SetConsoleOutputCP(CP_UTF8);
    try {
        if(argc>1 && std::wstring(argv[1])==L"--runtime-access") {
            auto root=knownFolder(FOLDERID_ProgramFiles)/L"FloatingTranslate"/L"0.1.10";
            bool passed=true;
            for(const auto &part:{L"FuyiIME.dll",L"librime.dll",L"RimeData/build/fuyi.schema.yaml",L"models/encoder_model_quantized.onnx",L"models/en-zh/encoder_model_quantized.onnx",L"Proofreading/en_50k.txt"}) {
                bool ok=sandboxReadAcl(root/part,L"S-1-15-2-1")&&sandboxReadAcl(root/part,L"S-1-15-2-2");
                std::cout<<(ok?"PASS ":"FAIL ")<<utf8(part)<<" sandbox read-only permissions\n";passed=passed&&ok;
            }
            auto preferences=sharedPreferencesRoot()/userSid()/L"settings.ini";
            bool ok=sandboxReadAcl(preferences,L"S-1-15-2-1")&&sandboxReadAcl(preferences,L"S-1-15-2-2");
            std::cout<<(ok?"PASS ":"FAIL ")<<"shared settings sandbox read-only permissions\n";
            std::cout<<"ACL inspection only; this does not prove Search host input works.\n";
            return passed&&ok?0:1;
        }
        EnglishProofreader proofreading;
        if(!proofreading.load(directory()/L"Proofreading"/L"en_50k.txt"))throw std::runtime_error("Proofreading dictionary unavailable");
        if(proofreading.check(L"I wnat go to teh gym now ").corrected!=L"I want to go to the gym now ")
            throw std::runtime_error("Offline proofreading check failed");
        std::cout<<"Offline common grammar/spelling check passed\n";
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
