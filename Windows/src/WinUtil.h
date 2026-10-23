#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include "RuntimePaths.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <shlobj.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
namespace fs = std::filesystem;
namespace fuyi {
extern HINSTANCE module;
inline void pinModule() {
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       (LPCWSTR)&pinModule, &pinned);
}

inline std::wstring wide(const std::string &value) {
    if (value.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
    if (!n)
        throw std::runtime_error("Invalid UTF-8");
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), out.data(), n);
    return out;
}
inline std::string utf8(const std::wstring &value) {
    if (value.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0,
                                nullptr, nullptr);
    if (!n)
        throw std::runtime_error("Invalid Unicode");
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), (int)value.size(), out.data(), n,
                        nullptr, nullptr);
    return out;
}
inline fs::path directory() {
    std::vector<wchar_t> path(32768);
    DWORD n = GetModuleFileNameW(module, path.data(), (DWORD)path.size());
    if (!n || n >= path.size())
        throw std::runtime_error("Module path unavailable");
    return fs::path(std::wstring(path.data(), n)).parent_path();
}
inline fs::path configPath() {
    auto shared = sharedPreferencesRoot() / userSid();
    if (fs::is_directory(shared)) {
        auto file = shared / L"settings.ini";
        if (appContainer() && !fs::is_regular_file(file))
            throw std::runtime_error("Shared preferences unavailable");
        return file;
    }
    // Before activation, keep the old preferences available for migration.
    // A sandbox must never silently discard the user's privacy preference.
    if (appContainer()) throw std::runtime_error("Shared preferences unavailable");
    auto path = knownFolder(FOLDERID_LocalAppData) / L"FloatingTranslate";
    fs::create_directories(path);
    return path / L"settings.ini";
}
struct Options {
    bool english = false, above = false, privacy = false, direct = false, remember = true, chinese = false;
    static Options read() {
        Options o;
        try {
            auto p = configPath();
            if (fs::exists(p)) {
                std::ifstream readable(p);
                if (!readable) throw std::runtime_error("Cannot read settings");
            }
            o.english = GetPrivateProfileIntW(L"input", L"englishDefault", 0, p.c_str()) != 0;
            // Older versions shipped an above-caret default. Migrate it once;
            // subsequent explicit placement choices remain configurable.
            o.above = GetPrivateProfileIntW(L"input", L"placementVersion", 0, p.c_str()) >= 2 &&
                      GetPrivateProfileIntW(L"input", L"above", 0, p.c_str()) != 0;
            o.privacy = GetPrivateProfileIntW(L"input", L"privacy", 0, p.c_str()) != 0;
            o.chinese = GetPrivateProfileIntW(L"input", L"chineseDefault", 0, p.c_str()) != 0;
            o.remember = GetPrivateProfileIntW(L"input", L"remember", 1, p.c_str()) != 0;
            o.direct = GetPrivateProfileIntW(L"input", L"directEnglish", 0, p.c_str()) != 0;
        } catch (...) {
            o.privacy = true;
        }
        return o;
    }
    void save() const {
        auto p = configPath();
        for (auto entry : std::vector<std::pair<const wchar_t *, bool>>{{L"englishDefault", english},
                                                                        {L"above", above},
                                                                        {L"privacy", privacy},
                                                                        {L"directEnglish", direct}, {L"remember", remember}, {L"chineseDefault", chinese}})
            if (!WritePrivateProfileStringW(L"input", entry.first, entry.second ? L"1" : L"0", p.c_str()))
                throw std::runtime_error("Cannot save settings");
        if (!WritePrivateProfileStringW(L"input", L"placementVersion", L"2", p.c_str()))
            throw std::runtime_error("Cannot save placement version");
    }
};
inline DWORD clipboard(HWND owner, const std::wstring &text) {
    if (text.empty()) return ERROR_NO_DATA;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!mem) return ERROR_NOT_ENOUGH_MEMORY;
    void *p = GlobalLock(mem);
    if (!p) { GlobalFree(mem); return ERROR_NOT_ENOUGH_MEMORY; }
    memcpy(p, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(mem);
    // Prepare Unicode data before opening the clipboard and hold it only for
    // the actual write. The panel retries transient lock failures asynchronously.
    if (!OpenClipboard(owner)) {
        DWORD error = GetLastError(); GlobalFree(mem);
        return error ? error : ERROR_BUSY;
    }
    DWORD error = ERROR_SUCCESS;
    if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, mem)) {
        error = GetLastError();
        if (!error) error = ERROR_WRITE_FAULT;
        GlobalFree(mem);
    }
    CloseClipboard();
    return error;
}
inline HMODULE loadLocal(const wchar_t *name) {
    auto p = directory() / name;
    return LoadLibraryExW(p.c_str(), nullptr,
                          LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
}
inline void settings() {
    auto exe = directory() / L"FuyiSettings.exe";
    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, directory().c_str(), SW_SHOWNORMAL);
}
template <class T> void release(T *&p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}
} // namespace fuyi
