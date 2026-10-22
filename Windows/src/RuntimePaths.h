#pragma once
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
namespace fuyi {
inline bool appContainer() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        throw std::runtime_error("Process token unavailable");
    DWORD value = 0, size = 0;
    BOOL ok = GetTokenInformation(token, TokenIsAppContainer, &value, sizeof(value), &size);
    CloseHandle(token);
    if (!ok) throw std::runtime_error("Process isolation unavailable");
    return value != 0;
}
inline std::wstring userSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        throw std::runtime_error("User identity unavailable");
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> data(size);
    BOOL ok = size && GetTokenInformation(token, TokenUser, data.data(), size, &size);
    CloseHandle(token);
    if (!ok) throw std::runtime_error("User identity unavailable");
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(((TOKEN_USER *)data.data())->User.Sid, &text))
        throw std::runtime_error("User identity unavailable");
    std::wstring result(text); LocalFree(text);
    return result;
}
inline std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR text = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &text)))
        throw std::runtime_error("Known folder unavailable");
    std::filesystem::path result(text); CoTaskMemFree(text); return result;
}
inline std::filesystem::path sharedPreferencesRoot() {
    return knownFolder(FOLDERID_ProgramData) / L"FloatingTranslate" / L"Preferences";
}
inline std::filesystem::path pinyinRuntime() {
    std::filesystem::path base;
    if (appContainer()) {
        // Use only the hosting app's own writable sandbox. Do not grant the
        // IME access to the user's ordinary AppData or relax host isolation.
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            throw std::runtime_error("Sandbox identity unavailable");
        DWORD size = 0;
        GetTokenInformation(token, TokenAppContainerSid, nullptr, 0, &size);
        std::vector<BYTE> data(size);
        BOOL ok = size && GetTokenInformation(token, TokenAppContainerSid, data.data(), size, &size);
        LPWSTR sid = nullptr;
        PSID value = ok ? ((TOKEN_APPCONTAINER_INFORMATION *)data.data())->TokenAppContainer : nullptr;
        if (ok) ok = value && ConvertSidToStringSidW(value, &sid);
        CloseHandle(token);
        if (!ok) throw std::runtime_error("Sandbox identity unavailable");
        auto library = LoadLibraryExW(L"userenv.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Folder = HRESULT(WINAPI *)(PCWSTR, PWSTR *);
        auto folder = library ? (Folder)GetProcAddress(library, "GetAppContainerFolderPath") : nullptr;
        PWSTR path = nullptr;
        HRESULT hr = folder ? folder(sid, &path) : E_FAIL;
        LocalFree(sid);
        if (SUCCEEDED(hr) && path) { base = path; CoTaskMemFree(path); }
        if (library) FreeLibrary(library);
        if (base.empty()) throw std::runtime_error("Sandbox storage unavailable");
        base /= L"TempState";
    } else {
        std::vector<wchar_t> temp(32768);
        DWORD n = GetTempPathW((DWORD)temp.size(), temp.data());
        if (!n || n >= temp.size()) throw std::runtime_error("Temporary storage unavailable");
        base = std::wstring(temp.data(), n);
    }
    auto result = base / (L"FuyiRime-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(result);
    return result;
}
} // namespace fuyi
