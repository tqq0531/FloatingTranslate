#include "../src/Maintenance.h"
#include <cassert>
#include <iostream>
namespace fuyi { HINSTANCE module = nullptr; }
int main() {
    using namespace fuyi;
    module = GetModuleHandleW(nullptr);
    wchar_t temp[32768]{};
    assert(GetTempPathW(32768, temp));
    auto root = fs::path(temp)/(L"FuyiMaintenanceTest-"+std::to_wstring(GetCurrentProcessId()));
    assert(fs::create_directory(root));
    auto dead = root/L"FuyiRime-4294967292";
    auto unknown = root/L"FuyiRime-4294967288";
    auto live = root/(L"FuyiRime-"+std::to_wstring(GetCurrentProcessId()));
    auto unrelated = root/L"FuyiRime-old-version";
    for (auto &path : {dead, unknown, live, unrelated}) fs::create_directory(path);
    std::ofstream(dead/L"user.yaml") << "fixed fixture";
    std::ofstream(unknown/L"personal.txt") << "must remain";
    std::ofstream(live/L"user.yaml") << "active process";
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 4294967292u);
    assert(!process && GetLastError() == ERROR_INVALID_PARAMETER);
    assert(cleanRimeCaches({root}) == 1);
    assert(!fs::exists(dead));
    assert(fs::exists(unknown/L"personal.txt"));
    assert(fs::exists(live/L"user.yaml"));
    assert(fs::exists(unrelated));
    assert(cleanRimeCaches({root}) == 0); // Repeat startup is safe.
    assert(noLinks(root));
    fs::remove_all(root); // Only this newly created test fixture.
    std::cout << "Stale-cache cleanup, live PID protection, unrelated files and repeat startup passed\n";
}
