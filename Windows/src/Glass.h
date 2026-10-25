#pragma once
#include "WinUtil.h"
namespace fuyi {
// Apply a backdrop only to our own IME HWND; never change its owner, activation,
// shell cloaking, or the host application's composition attributes.
inline bool enableGlass(HWND window) {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SYSTEM_POWER_STATUS power{};
    DWORD transparent = 1, bytes = sizeof(transparent);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &transparent, &bytes);
    bool allowed = transparent && !GetSystemMetrics(SM_REMOTESESSION) &&
        !(SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
          (contrast.dwFlags & HCF_HIGHCONTRASTON)) &&
        !(GetSystemPowerStatus(&power) && power.SystemStatusFlag == 1);
    struct Accent { int state, flags; DWORD tint; int animation; };
    struct Attribute { int attribute; void *data; SIZE_T size; };
    using SetComposition = BOOL(WINAPI *)(HWND, Attribute *);
    auto setComposition = (SetComposition)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    auto dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    using Extend = HRESULT(WINAPI *)(HWND, const void *);
    auto set = dwm ? (SetAttribute)GetProcAddress(dwm, "DwmSetWindowAttribute") : nullptr;
    auto extend = dwm ? (Extend)GetProcAddress(dwm, "DwmExtendFrameIntoClientArea") : nullptr;
    bool enabled = false;
    if (allowed && set && extend) {
        // Windows 11 22H2: transient-window (Desktop Acrylic) backdrop.
        int backdrop = 3;
        enabled = SUCCEEDED(set(window, 38, &backdrop, sizeof(backdrop)));
    }
    if (!enabled && setComposition && extend) {
        // Windows 10 has no DWMWA_SYSTEMBACKDROP_TYPE. Resolve the native
        // compatibility API at runtime; failure retains the opaque design.
        Accent accent{allowed ? 4 : 0, 2, 0xA62B2725, 0};
        Attribute attribute{19, &accent, sizeof(accent)};
        enabled = setComposition(window, &attribute) && allowed;
    }
    if (!allowed && set) { int none = 1; set(window, 38, &none, sizeof(none)); }
    if (extend) {
        struct Margins { int left, right, top, bottom; } margins{enabled ? -1 : 0, 0, 0, 0};
        if (FAILED(extend(window, &margins))) enabled = false;
    }
    if (dwm) FreeLibrary(dwm);
    return enabled;
}
} // namespace fuyi
