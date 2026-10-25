#pragma once
#include "Core.h"
#include "Ids.h"
#include "WinUtil.h"
#include <tlhelp32.h>
#include <set>
namespace fuyi {
inline constexpr const wchar_t *runtimeVersion = L"0.1.22";
inline fs::path registeredRuntime() {
    HKEY key = nullptr;
    auto name = L"SOFTWARE\\Classes\\CLSID\\" + guidText(serviceId) + L"\\InprocServer32";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, name.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return {};
    wchar_t value[32768]{}; DWORD size = sizeof(value), type = 0;
    auto error = RegQueryValueExW(key, nullptr, nullptr, &type, (BYTE *)value, &size);
    RegCloseKey(key);
    return error == ERROR_SUCCESS && type == REG_SZ ? fs::path(value).parent_path() : fs::path{};
}
struct RuntimeUser { DWORD pid, session; std::wstring name; fs::path folder; };
inline std::vector<RuntimeUser> runtimeUsers(bool *complete = nullptr) {
    if (complete) *complete = true;
    std::vector<RuntimeUser> found;
    auto root = knownFolder(FOLDERID_ProgramFiles) / L"FloatingTranslate";
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { if (complete) *complete = false; return found; }
    PROCESSENTRY32W process{sizeof(process)};
    for (BOOL more = Process32FirstW(snapshot, &process); more; more = Process32NextW(snapshot, &process)) {
        // The protected Windows subsystem process cannot host a TSF TIP. Its
        // module snapshot is always denied, including for an administrator.
        if (_wcsicmp(process.szExeFile,L"csrss.exe")==0) continue;
        DWORD session = 0;
        if (!ProcessIdToSessionId(process.th32ProcessID, &session) || !session) continue;
        HANDLE modules = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process.th32ProcessID);
        if (modules == INVALID_HANDLE_VALUE) {
            // Do not remove shared runtime files if another interactive process
            // could not be inspected. A later elevated startup can retry.
            if (complete && GetLastError() != ERROR_INVALID_PARAMETER) *complete = false;
            continue;
        }
        MODULEENTRY32W item{sizeof(item)};
        for (BOOL next = Module32FirstW(modules, &item); next; next = Module32NextW(modules, &item)) {
            auto folder = fs::path(item.szExePath).parent_path();
            if (_wcsicmp(folder.parent_path().c_str(), root.c_str()) == 0 &&
                std::none_of(found.begin(), found.end(), [&](const auto &user) {
                    return user.pid == process.th32ProcessID && _wcsicmp(user.folder.c_str(), folder.c_str()) == 0;
                }))
                found.push_back({process.th32ProcessID, session, process.szExeFile, folder});
        }
        CloseHandle(modules);
    }
    CloseHandle(snapshot);
    return found;
}
inline bool noLinks(const fs::path &path) {
    auto attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        fs::recursive_directory_iterator it(path, ec), end;
        if (ec) return false;
        for (; it != end; it.increment(ec)) {
            if (ec) return false;
            auto entry = GetFileAttributesW(it->path().c_str());
            if (entry == INVALID_FILE_ATTRIBUTES || (entry & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        }
        if (ec) return false;
    }
    return true;
}
inline size_t cleanOldRuntimes() {
    auto root = knownFolder(FOLDERID_ProgramFiles) / L"FloatingTranslate";
    auto registered = registeredRuntime();
    if (registered.empty() || registered.filename() != runtimeVersion || !noLinks(root)) return 0;
    bool complete = false;
    auto users = runtimeUsers(&complete);
    if (!complete) return 0;
    size_t removed = 0;
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        auto path = it->path();
        if (!olderRuntimeVersion(path.filename().wstring(), runtimeVersion) || !noLinks(path)) continue;
        if (!fs::is_regular_file(path/L"FuyiIME.dll", ec) || !fs::is_regular_file(path/L"FuyiSettings.exe", ec) ||
            !fs::is_regular_file(path/L"SHA256SUMS.txt", ec)) { ec.clear(); continue; }
        bool used = std::any_of(users.begin(), users.end(), [&](const auto &user) {
            return _wcsicmp(user.folder.c_str(), path.c_str()) == 0;
        });
        if (used || _wcsicmp(directory().c_str(), path.c_str()) == 0) continue;
        fs::remove_all(path, ec);
        if (!ec) ++removed;
        ec.clear(); // Locked files are retried at a later startup, never forced.
    }
    return removed;
}
inline size_t cleanRimeCaches(const std::vector<fs::path> &testRoots = {}) {
    wchar_t temp[32768]{};
    std::vector<fs::path> roots;
    DWORD n = GetTempPathW(32768, temp);
    if (n && n < 32768) roots.emplace_back(temp);
    auto search = knownFolder(FOLDERID_LocalAppData)/L"Packages"/L"Microsoft.Windows.Search_cw5n1h2txyewy";
    roots.push_back(search/L"TempState"); roots.push_back(search/L"AC"/L"Temp");
    roots.push_back(search/L"AC"/L"TempState");
    if (!testRoots.empty()) roots = testRoots;
    size_t removed = 0;
    for (auto &root : roots) {
        std::error_code ec;
        auto attr = GetFileAttributesW(root.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT) || !fs::is_directory(root, ec)) continue;
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
            auto path = it->path(); auto pid = rimeCachePid(path.filename().wstring());
            if (!pid || !noLinks(path) || !fs::is_directory(path, ec)) continue;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *pid);
            if (process) { CloseHandle(process); continue; }
            if (GetLastError() != ERROR_INVALID_PARAMETER) continue;
            // Old caches have no marker. Accept only the fixed Rime metadata
            // and compiled-data names; unrelated files make the folder ineligible.
            bool owned = true;
            for (auto &entry : fs::recursive_directory_iterator(path, ec)) {
                if (entry.is_directory()) continue;
                auto name = entry.path().filename().wstring();
                if (name != L"user.yaml" && name != L"installation.yaml" && name != L"display-status.txt" &&
                    name != L"fuyi.schema.yaml" && name != L"default.yaml" &&
                    name != L"pinyin_simp.table.bin" && name != L"pinyin_simp.reverse.bin" &&
                    name != L"fuyi.prism.bin") { owned = false; break; }
            }
            if (ec || !owned) { ec.clear(); continue; }
            fs::remove_all(path, ec); if (!ec) ++removed; ec.clear();
        }
    }
    return removed;
}
inline std::wstring maintainUserRuntime() {
    DWORD session = 0; ProcessIdToSessionId(GetCurrentProcessId(), &session);
    auto registered = registeredRuntime();
    std::wstring reopen;
    std::vector<std::wstring> names;
    for (auto &user : runtimeUsers()) {
        if (user.session != session || user.pid == GetCurrentProcessId() ||
            !olderRuntimeVersion(user.folder.filename().wstring(), runtimeVersion)) continue;
        bool search = _wcsicmp(user.name.c_str(), L"SearchApp.exe") == 0 ||
                      _wcsicmp(user.name.c_str(), L"SearchHost.exe") == 0 || _wcsicmp(user.name.c_str(), L"SearchUI.exe") == 0;
        bool refreshed = false;
        if (search && registered.filename() == runtimeVersion) {
            HANDLE process = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, user.pid);
            if (process) {
                // Recheck the executable before terminating a potentially reused PID.
                wchar_t image[32768]{}; DWORD size = 32768;
                wchar_t windows[32768]{}; GetWindowsDirectoryW(windows, 32768);
                auto systemApps = fs::path(windows)/L"SystemApps";
                if (QueryFullProcessImageNameW(process, 0, image, &size) &&
                    _wcsicmp(fs::path(image).parent_path().parent_path().c_str(), systemApps.c_str()) == 0 &&
                    _wcsicmp(fs::path(image).filename().c_str(), user.name.c_str()) == 0)
                    if (TerminateProcess(process, 0)) refreshed = WaitForSingleObject(process, 2000) == WAIT_OBJECT_0;
                CloseHandle(process);
            }
        }
        if (!refreshed) {
            auto name = fs::path(user.name).stem().wstring();
            if (_wcsicmp(user.name.c_str(), L"Weixin.exe") == 0 || _wcsicmp(user.name.c_str(), L"WeChatAppEx.exe") == 0) name=L"微信";
            if (_wcsicmp(user.name.c_str(), L"chrome.exe") == 0) name=L"Chrome";
            if (_wcsicmp(user.name.c_str(), L"explorer.exe") == 0) name=L"文件管理器";
            if (_wcsicmp(user.name.c_str(), L"ApplicationFrameHost.exe") == 0) name=L"Windows 应用";
            if (_wcsicmp(user.name.c_str(), L"msedgewebview2.exe") == 0) name=L"网页应用";
            if (std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
                if (!reopen.empty()) reopen += L"、";
                reopen += name;
            }
        }
    }
    cleanRimeCaches();
    return reopen.empty() ? L"0.1.22 · 已检查更新和过期临时缓存" : L"0.1.22 已安装，等待重启统一生效。\n仍用旧版：" + reopen + L"\n保存后点击“统一更新所有软件”。";
}
} // namespace fuyi
