#pragma once
#include <objbase.h>
#include <string>
#include <windows.h>
#include "Version.h"
namespace fuyi {
inline constexpr GUID legacyServiceId = {0x92ae21fd, 0x4226, 0x4f22, {0x9d, 0xe8, 0x20, 0x58, 0x3b, 0xca, 0xc2, 0x10}};
// A distinct COM identity per release bypasses each host's old in-process
// class-factory cache. Loaded old DLLs can remain resident without being used
// for new activations; never unload a module out of another application.
constexpr GUID serviceForVersion(const wchar_t *version) {
    unsigned parts[4]{}, index=0;
    for(;*version && index<4;++version) {
        if(*version==L'.'){++index;continue;}
        if(*version>=L'0' && *version<=L'9')parts[index]=parts[index]*10+(*version-L'0');
    }
    return {0x92ae21fd,0x4226,0x4f22,{0x9d,0xe8,(BYTE)parts[0],(BYTE)parts[1],
        (BYTE)(parts[2]>>8),(BYTE)parts[2],(BYTE)(parts[3]>>8),(BYTE)parts[3]}};
}
inline constexpr GUID serviceId = serviceForVersion(runtimeVersion);
inline bool isFuyiService(REFGUID id) {
    return IsEqualGUID(id,legacyServiceId) || (id.Data1==serviceId.Data1 &&
        id.Data2==serviceId.Data2 && id.Data3==serviceId.Data3 && id.Data4[0]==0x9d && id.Data4[1]==0xe8);
}
inline const GUID profileId = {0x3162c944, 0x3fac, 0x46eb, {0xa6, 0xbc, 0xc6, 0x52, 0xa4, 0xc7, 0x3f, 0x84}};
inline const GUID barId = {0xa2619899, 0x7e45, 0x4d7c, {0x93, 0x70, 0x81, 0xd3, 0xb1, 0x98, 0x9e, 0x60}};
inline const GUID attributeId = {
    0xc8863ade, 0x8ef1, 0x44ab, {0x90, 0x24, 0x5d, 0x48, 0x55, 0x9c, 0x25, 0xdd}};
inline std::wstring guidText(REFGUID guid) {
    wchar_t buffer[40];
    StringFromGUID2(guid, buffer, 40);
    return buffer;
}
} // namespace fuyi
