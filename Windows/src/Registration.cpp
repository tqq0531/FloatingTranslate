#include "Ids.h"
#include "WinUtil.h"
#include "CandidateUI.h"
#include <msctf.h>
namespace fuyi {
// Public TSF capability GUIDs absent from LLVM-MinGW's older msctf.h.
static const GUID immersiveCategory = {0x13a016df, 0x560b, 0x46cd, {0x94,0x7a,0x4c,0x3a,0xf1,0xe0,0xe3,0x5d}};
static const GUID trayCategory = {0x25504fb4, 0x7bab, 0x4bc1, {0x9c,0x69,0xcf,0x81,0x89,0x0f,0x0e,0xf5}};
static const GUID serviceCategories[] = {GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, immersiveCategory, trayCategory, tsf::enabledCategory};
static HRESULT writeKey(const std::wstring &path, const wchar_t *name, const std::wstring &value) {
    HKEY key = nullptr;
    auto result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                                  &key, nullptr);
    if (result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(result);
    result = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value.c_str(),
                            (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(result);
}
HRESULT unregisterService() {
    ITfCategoryMgr *categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   (void **)&categories))) {
        for (const auto &category : serviceCategories)
            categories->UnregisterCategory(serviceId, category, serviceId);
        categories->Release();
    }
    ITfInputProcessorProfiles *profiles = nullptr;
    HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_ITfInputProcessorProfiles, (void **)&profiles);
    if (SUCCEEDED(result)) {
        result = profiles->Unregister(serviceId);
        profiles->Release();
    }
    auto key = L"SOFTWARE\\Classes\\CLSID\\" + guidText(serviceId);
    auto deleted = RegDeleteTreeW(HKEY_LOCAL_MACHINE, key.c_str());
    if (deleted != ERROR_SUCCESS && deleted != ERROR_FILE_NOT_FOUND)
        return HRESULT_FROM_WIN32(deleted);
    return result;
}
HRESULT registerService() {
    auto dll = (nativeDirectory() / L"FuyiIME.dll").wstring();
    auto base = L"SOFTWARE\\Classes\\CLSID\\" + guidText(serviceId);
    auto hr = writeKey(base, nullptr, L"浮译离线输入法");
    if (SUCCEEDED(hr))
        hr = writeKey(base + L"\\InprocServer32", nullptr, dll);
    if (SUCCEEDED(hr))
        hr = writeKey(base + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    if (FAILED(hr))
        return hr;
    ITfInputProcessorProfiles *profiles = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfiles, (void **)&profiles);
    if (SUCCEEDED(hr)) {
        hr = profiles->Register(serviceId);
        if (SUCCEEDED(hr))
            hr = profiles->AddLanguageProfile(serviceId, 0x0804, profileId, L"浮译", 2, dll.c_str(),
                                              (ULONG)dll.size(), 0);
        release(profiles);
    }
    ITfCategoryMgr *categories = nullptr;
    if (SUCCEEDED(hr))
        hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                              (void **)&categories);
    if (SUCCEEDED(hr)) {
        for (const auto &category : serviceCategories) {
            hr = categories->RegisterCategory(serviceId, category, serviceId);
            if (FAILED(hr)) break;
        }
    }
    release(categories);
#ifdef _WIN64
    if(SUCCEEDED(hr))hr=writeKey(L"SOFTWARE\\FloatingTranslate",L"CurrentDirectory",directory().wstring());
#endif
    if (FAILED(hr))
        unregisterService();
    return hr;
}
} // namespace fuyi
extern "C" HRESULT WINAPI DllRegisterServer() {
    return fuyi::registerService();
}
extern "C" HRESULT WINAPI DllUnregisterServer() {
    return fuyi::unregisterService();
}
