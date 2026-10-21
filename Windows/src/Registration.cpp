#include "Ids.h"
#include "WinUtil.h"
#include <msctf.h>
namespace fuyi {
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
        categories->UnregisterCategory(serviceId, GUID_TFCAT_TIP_KEYBOARD, serviceId);
        categories->UnregisterCategory(serviceId, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, serviceId);
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
    auto dll = (directory() / L"FuyiIME.dll").wstring();
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
        hr = categories->RegisterCategory(serviceId, GUID_TFCAT_TIP_KEYBOARD, serviceId);
        if (SUCCEEDED(hr))
            hr = categories->RegisterCategory(serviceId, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, serviceId);
    }
    release(categories);
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
