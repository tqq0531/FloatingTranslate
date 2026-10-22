#pragma once
#include "WinUtil.h"
#include "Translator.h"
namespace fuyi {
inline void directoryPermissions(const fs::path &path, const std::wstring &sddl) {
    // Refuse junctions/reparse points; never redirect an elevated copy or ACL
    // operation through a user-controlled link.
    auto attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Runtime path is a reparse point");
    fs::create_directory(path);
    PSECURITY_DESCRIPTOR security = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                               &security, nullptr))
        throw std::runtime_error("Cannot prepare preference permissions");
    BOOL ok = SetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION |
                               PROTECTED_DACL_SECURITY_INFORMATION, security);
    LocalFree(security);
    if (!ok) throw std::runtime_error("Cannot prepare preference permissions");
}
inline void preparePreferences(const std::wstring &sid) {
    PSID parsed = nullptr;
    if (!ConvertStringSidToSidW(sid.c_str(), &parsed))
        throw std::runtime_error("Invalid user identity");
    bool valid = IsValidSid(parsed); LocalFree(parsed);
    if (!valid || sid.find_first_not_of(L"S-0123456789") != std::wstring::npos)
        throw std::runtime_error("Invalid user identity");
    // Only four non-secret input preferences live here. Apps get read access
    // to this dedicated directory, never to ordinary user files/input text.
    const std::wstring common = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)(A;OICI;GRGX;;;AC)";
    auto root = knownFolder(FOLDERID_ProgramData) / L"FloatingTranslate";
    directoryPermissions(root, common);
    directoryPermissions(root / L"Preferences", common);
    directoryPermissions(root / L"Preferences" / sid,
        L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGWGX;;;" + sid + L")(A;OICI;GRGX;;;AC)");
}
inline fs::path installRuntime() {
    // Program Files already permits read/execute from AppContainers. Keep
    // dictionaries and models read-only, with no broad ACL on Desktop/AppData.
    auto root = knownFolder(FOLDERID_ProgramFiles) / L"FloatingTranslate";
    for (auto p : {root, root / L"0.1.2"}) {
        if (fs::exists(p) && (GetFileAttributesW(p.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Runtime path is a reparse point");
        fs::create_directory(p);
    }
    auto destination = root / L"0.1.2";
    auto source = directory();
    if (fs::equivalent(source, destination)) return destination;
    auto nested = destination.lexically_relative(source);
    if (!nested.empty() && *nested.begin() != L"..")
        throw std::runtime_error("Extract the package outside its installed runtime directory");
    // Copy only files listed in the delivered checksum manifest. Unrelated
    // files a user puts beside the package must never become sandbox-readable.
    std::ifstream manifest(source / L"SHA256SUMS.txt");
    if (!manifest) throw std::runtime_error("Package manifest missing");
    std::string line;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 67 || line.substr(64, 2) != "  " ||
            line.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("Invalid package manifest");
        fs::path relative(wide(line.substr(66)));
        if (relative.empty() || relative.has_root_path()) throw std::runtime_error("Invalid package path");
        for (const auto &part : relative)
            if (part == L".." || part == L".") throw std::runtime_error("Invalid package path");
        auto input = source, target = destination;
        for (const auto &part : relative) {
            input /= part; target /= part;
            for (const auto &path : {input, target}) {
                DWORD attr = GetFileAttributesW(path.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
                    throw std::runtime_error("Linked package paths are unsupported");
            }
        }
        if (!fs::is_regular_file(input) || fileSha(input) != line.substr(0, 64))
            throw std::runtime_error("Package integrity check failed");
        fs::create_directories(target.parent_path());
        if (fs::is_regular_file(target) && fs::file_size(input) == fs::file_size(target) &&
            fileSha(target) == line.substr(0, 64)) continue;
        fs::copy_file(input, target, fs::copy_options::overwrite_existing);
    }
    if (!manifest.eof()) throw std::runtime_error("Cannot read package manifest");
    fs::copy_file(source / L"SHA256SUMS.txt", destination / L"SHA256SUMS.txt", fs::copy_options::overwrite_existing);
    return destination;
}
} // namespace fuyi
