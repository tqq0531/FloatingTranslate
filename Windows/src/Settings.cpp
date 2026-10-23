#include "Ids.h"
#include "InstallRuntime.h"
#include "PersonalDictionary.h"
#include "Translator.h"
#include "WinUtil.h"
#include <commctrl.h>
#include <msctf.h>
#include <shellapi.h>
#include <thread>
namespace fuyi {
HINSTANCE module = nullptr;
static HFONT font = nullptr, heading = nullptr;
static HWND window = nullptr, status = nullptr;
static Options options;
static std::atomic_bool busy{false};
static void explain(const std::wstring &text, UINT flags = MB_OK | MB_ICONINFORMATION) {
    MessageBoxW(window, text.c_str(), L"浮译 Windows", flags);
}
static bool run(const fs::path &exe, const std::wstring &args, bool elevated = false, DWORD *code = nullptr,
                bool hidden = false) {
    SHELLEXECUTEINFOW info = {sizeof(info)};
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.hwnd = window;
    info.lpVerb = elevated ? L"runas" : L"open";
    info.lpFile = exe.c_str();
    info.lpParameters = args.c_str();
    auto cwd = directory().wstring();
    info.lpDirectory = cwd.c_str();
    info.nShow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info))
        return false;
    if (info.hProcess) {
        WaitForSingleObject(info.hProcess, INFINITE);
        DWORD result = 1;
        GetExitCodeProcess(info.hProcess, &result);
        CloseHandle(info.hProcess);
        if (code)
            *code = result;
        return result == 0 || result == 3010;
    }
    return false;
}
static bool profile(bool enable) {
    HMODULE input = LoadLibraryExW(L"input.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!input)
        return false;
    using Install = BOOL(WINAPI *)(LPCWSTR, DWORD);
    auto install = (Install)GetProcAddress(input, "InstallLayoutOrTip");
    auto tip = L"0x0804:" + guidText(serviceId) + guidText(profileId) + L";";
    BOOL ok = install ? install(tip.c_str(), enable ? 0 : 1) : FALSE;
    FreeLibrary(input);
    return ok != FALSE;
}
static bool elevatedRegistration(bool enable, const std::wstring &sid = L"") {
    try {
        if (enable) {
            verifyModels();
            auto runtime = loadLocal(L"onnxruntime.dll");
            if (!runtime) {
                DWORD code = 1;
                if (!run(directory() / L"vc_redist.x64.exe", L"/install /quiet /norestart", false, &code) ||
                    code == 3010)
                    return false;
                runtime = loadLocal(L"onnxruntime.dll");
                if (!runtime)
                    return false;
            }
            FreeLibrary(runtime);
            // Validate Rime and actual inference in a separate process before loading
            // the new input service into any user's application.
            if (!run(directory() / L"FuyiCheck.exe", L"", false, nullptr, true))
                return false;
        }
        auto runtimeDirectory = directory();
        if (enable) {
            preparePreferences(sid.empty() ? userSid() : sid);
            runtimeDirectory = installRuntime();
        }
        auto dll = LoadLibraryExW((runtimeDirectory / L"FuyiIME.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!dll)
            return false;
        using Register = HRESULT(WINAPI *)();
        auto function = (Register)GetProcAddress(dll, enable ? "DllRegisterServer" : "DllUnregisterServer");
        HRESULT hr = function ? function() : E_FAIL;
        FreeLibrary(dll);
        return SUCCEEDED(hr);
    } catch (...) {
        return false;
    }
}
static bool enable(bool value) {
    if (busy.exchange(true))
        return false;
    auto exe = directory() / L"FuyiSettings.exe";
    DWORD code = 1;
    // Capture old user settings before elevation/provisioning creates the new
    // shared preference directory. No input text is copied into that directory.
    auto previous = Options::read();
    bool ok = run(exe, value ? L"--register-elevated " + userSid() : L"--unregister-elevated", true, &code);
    if (ok && value) {
        try { previous.save(); } catch (...) { ok = false; }
    }
    if (ok) {
        ok = profile(value);
        if (value && ok)
            explain(L"浮译已启用。\n\n按 Win + 空格，选择“浮译”。\n轻按 Shift 切换中文 / "
                    L"English。\n\n系统搜索兼容组件已复制到 Program Files；语言包仍来自压缩包，无需下载。"
                    L"更新前请先停用旧版；个别已打开的软件需保存草稿后重新打开。");
        else if (!value)
            explain(L"浮译已停用并注销。\n\n已打开的软件可能仍占用输入法文件。请先切换其他输入法并关闭使用过"
                    L"浮译的软件，再删除解压目录。无需删除其他输入法。");
    }
    if (!ok)
        explain(value ? L"启用未完成。\n\n请先运行“检查离线组件."
                        L"cmd”，确认文件解压完整、没有被安全软件隔离。启用需要系统管理员授权。\n\n如果刚安装 "
                        L"Microsoft 运行库并提示重启，请保存工作后重启再试。没有修改其他输入法。"
                      : L"停用未完成，请确认已授权管理员操作，并先切换到其他输入法。",
                MB_OK | MB_ICONERROR);
    busy = false;
    return ok;
}
static HWND control(const wchar_t *type, const wchar_t *text, DWORD style, int x, int y, int w, int h,
                    int id) {
    auto c = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
                             (HMENU)(INT_PTR)id, module, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}
static void save() {
    options.english = SendDlgItemMessageW(window, 101, BM_GETCHECK, 0, 0) == BST_CHECKED;
    options.above = SendDlgItemMessageW(window, 102, BM_GETCHECK, 0, 0) != BST_CHECKED;
    options.chinese = SendDlgItemMessageW(window, 106, BM_GETCHECK, 0, 0) == BST_CHECKED;
    options.remember = SendDlgItemMessageW(window, 105, BM_GETCHECK, 0, 0) == BST_CHECKED;
    options.privacy = SendDlgItemMessageW(window, 103, BM_GETCHECK, 0, 0) == BST_CHECKED;
    try {
        options.save();
        SetWindowTextW(status, L"设置已保存，输入法会自动应用。");
    } catch (...) {
        SetWindowTextW(status, L"设置保存失败，请检查用户目录权限。");
    }
}
static void test() {
    if (busy.exchange(true))
        return;
    SetWindowTextW(status, L"正在校验内置模型并测试本机翻译…");
    std::thread([] {
        bool ok = false;
        try {
            verifyModels();
            Translator engine;
            auto text = engine.translate(L"你好，今天我去北京。", [] { return false; });
            auto chinese = engine.translate(L"Have you worked out today?", [] { return false; }, true);
            ok = !text.empty() && std::any_of(chinese.begin(), chinese.end(), [](wchar_t c) {
                return c >= 0x3400 && c <= 0x9fff;
            });
        } catch (...) {
        }
        PostMessageW(window, WM_APP + 1, ok, 0);
    }).detach();
}
static LRESULT CALLBACK proc(HWND w, UINT message, WPARAM a, LPARAM b) {
    switch (message) {
    case WM_CREATE: {
        window = w;
        options = Options::read();
        font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        heading =
            CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        auto title = control(L"STATIC", L"浮译 · Windows", 0, 32, 24, 500, 46, 0);
        SendMessageW(title, WM_SETFONT, (WPARAM)heading, TRUE);
        control(L"STATIC", L"中英双向翻译，离线模型已在解压目录内。", 0, 34, 76, 540, 32, 0);
        control(L"BUTTON", L"启用浮译", BS_PUSHBUTTON, 34, 123, 160, 44, 1);
        control(L"BUTTON", L"停用并注销", BS_PUSHBUTTON, 208, 123, 160, 44, 2);
        control(L"BUTTON", L"检查离线组件", BS_PUSHBUTTON, 382, 123, 180, 44, 3);
        control(L"STATIC", L"输入偏好", 0, 34, 199, 500, 26, 0);
        control(L"BUTTON", L"拼音转换后默认输出英文", BS_AUTOCHECKBOX | WS_TABSTOP, 34, 230, 520, 28, 101);
        control(L"BUTTON", L"英文转换后默认输出中文", BS_AUTOCHECKBOX | WS_TABSTOP, 34, 264, 520, 28, 106);
        control(L"BUTTON", L"翻译浮窗优先显示在光标下方", BS_AUTOCHECKBOX | WS_TABSTOP, 34, 298, 520, 28,
                102);
        control(L"BUTTON", L"隐私模式：直接输入，暂停拼音与翻译", BS_AUTOCHECKBOX | WS_TABSTOP, 34, 332, 530,
                28, 103);
        control(L"BUTTON", L"记住常选短词，优先显示（本机加密）", BS_AUTOCHECKBOX | WS_TABSTOP,
                34, 366, 390, 28, 105);
        control(L"BUTTON", L"清除选词记忆", BS_PUSHBUTTON | WS_TABSTOP, 430, 366, 132, 28, 6);
        CheckDlgButton(w, 106, options.chinese ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(w, 105, options.remember ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(w, 101, options.english ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(w, 102, options.above ? BST_UNCHECKED : BST_CHECKED);
        CheckDlgButton(w, 103, options.privacy ? BST_CHECKED : BST_UNCHECKED);
        control(L"STATIC", L"Shift 切换拼音/英文；回车默认语言，Tab 另一语言。", 0, 34, 410, 540, 26, 0);
        auto editor = control(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL, 34, 444,
                              528, 70, 104);
        SendMessageW(editor, EM_SETLIMITTEXT, 1000, 0);
        control(L"STATIC", L"本机离线 · 短词记忆可关闭或清除", 0, 34, 540, 540, 28, 0);
        control(L"STATIC",
                L"密码框会尽量跳过处理；自定义密码框可能无法识别。\n输入密码、验证码或密钥前请切换系统键盘。",
                0, 34, 575, 540, 48, 0);
        control(L"BUTTON", L"使用说明", BS_PUSHBUTTON, 34, 644, 160, 36, 4);
        control(L"BUTTON", L"隐私与开源许可", BS_PUSHBUTTON, 208, 644, 180, 36, 5);
        status = control(L"STATIC", L"0.1.7 · Windows x64 开发试用版", 0, 34, 700, 540, 40, 0);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        SetBkMode((HDC)a, TRANSPARENT);
        SetTextColor((HDC)a, RGB(43, 58, 82));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(w, &r);
        FillRect((HDC)a, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
        return 1;
    }
    case WM_COMMAND: {
        int id = LOWORD(a);
        if (((id >= 101 && id <= 103) || id == 105 || id == 106) && HIWORD(a) == BN_CLICKED) {
            save();
            return 0;
        }
        if (HIWORD(a) == BN_CLICKED) {
            if (id == 1)
                std::thread([] {
                    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    enable(true);
                    CoUninitialize();
                }).detach();
            if (id == 2)
                std::thread([] {
                    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    enable(false);
                    CoUninitialize();
                }).detach();
            if (id == 3)
                test();
            if (id == 4) {
                auto p = directory() / L"使用说明.txt";
                ShellExecuteW(w, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            if (id == 6) {
                try {
                    PersonalDictionary::clear();
                    explain(L"本机已保存的选词记忆已清除。\n系统搜索等沙箱宿主仅有进程内记忆，关闭对应软件后清空。");
                } catch (...) {
                    explain(L"暂时无法清除，请稍后重试。", MB_OK | MB_ICONERROR);
                }
            }
            if (id == 5) {
                auto p = directory() / L"licenses" / L"TRANSLATION-NOTICE.txt";
                ShellExecuteW(w, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        return 0;
    }
    case WM_APP + 1:
        busy = false;
        SetWindowTextW(status, a ? L"中英双向模型校验和离线翻译通过，无需网络。"
                                 : L"检查未通过，请运行“检查离线组件.cmd”查看结果。");
        return 0;
    case WM_CLOSE:
        if (busy) {
            explain(L"正在处理，请等待检查或注册完成。");
            return 0;
        }
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        if (font)
            DeleteObject(font);
        if (heading)
            DeleteObject(heading);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, message, a, b);
}
} // namespace fuyi
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int show) {
    using namespace fuyi;
    module = instance;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    InitCommonControls();
    std::wstring command = args ? args : L"";
    if (command == L"--register-elevated" || command.rfind(L"--register-elevated ", 0) == 0 ||
        command == L"--unregister-elevated") {
        bool registering = command != L"--unregister-elevated";
        auto sid = command.rfind(L"--register-elevated ", 0) == 0 ? command.substr(20) : L"";
        bool result = elevatedRegistration(registering, sid);
        CoUninitialize();
        return result ? 0 : 1;
    }
    if (command == L"--enable" || command == L"--disable") {
        bool result = enable(command == L"--enable");
        CoUninitialize();
        return result ? 0 : 1;
    }
    WNDCLASSW wc = {};
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.lpszClassName = L"Fuyi.Settings.1";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    RegisterClassW(&wc);
    auto w = CreateWindowExW(0, wc.lpszClassName, L"浮译设置",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                             CW_USEDEFAULT, 612, 800, nullptr, nullptr, instance, nullptr);
    ShowWindow(w, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}
