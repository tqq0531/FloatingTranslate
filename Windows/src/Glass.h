#pragma once
#include "WinUtil.h"
namespace fuyi {
// Apply a backdrop only to our own IME HWND; never change its owner, activation,
// shell cloaking, or the host application's composition attributes.
inline bool glassAllowed() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SYSTEM_POWER_STATUS power{};
    DWORD transparent = 1, bytes = sizeof(transparent);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &transparent, &bytes);
    return transparent && !GetSystemMetrics(SM_REMOTESESSION) &&
        !(SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
          (contrast.dwFlags & HCF_HIGHCONTRASTON)) &&
        !(GetSystemPowerStatus(&power) && power.SystemStatusFlag == 1);
}
} // namespace fuyi
