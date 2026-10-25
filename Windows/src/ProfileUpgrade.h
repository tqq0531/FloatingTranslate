#pragma once
#include "Maintenance.h"
#include <msctf.h>
#include <iostream>
namespace fuyi {
inline bool installUserProfile(REFGUID id,bool enabled) {
    auto input=LoadLibraryExW(L"input.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!input)return false;
    using Install=BOOL(WINAPI*)(LPCWSTR,DWORD);
    auto install=(Install)GetProcAddress(input,"InstallLayoutOrTip");
    auto tip=L"0x0804:"+guidText(id)+guidText(profileId)+L";";
    bool result=install && install(tip.c_str(),enabled?0:1);
    FreeLibrary(input);return result;
}
inline std::vector<GUID> olderRegisteredServices() {
    std::vector<GUID> result;
    HKEY classes=nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Classes\\CLSID",0,KEY_READ|KEY_WOW64_64KEY,&classes)!=ERROR_SUCCESS)return result;
    auto root=knownFolder(FOLDERID_ProgramFiles)/L"FloatingTranslate";
    for(DWORD index=0;;++index) {
        wchar_t name[64]{};DWORD count=64;
        auto status=RegEnumKeyExW(classes,index,name,&count,nullptr,nullptr,nullptr,nullptr);
        if(status==ERROR_NO_MORE_ITEMS)break;
        if(status!=ERROR_SUCCESS || _wcsnicmp(name,L"{92AE21FD-4226-4F22-9DE8-",25)!=0)continue;
        GUID id{};
        if(FAILED(CLSIDFromString(name,&id)) || !isFuyiService(id) || IsEqualGUID(id,serviceId))continue;
        HKEY server=nullptr;
        auto subkey=std::wstring(name)+L"\\InprocServer32";
        if(RegOpenKeyExW(classes,subkey.c_str(),0,KEY_READ,&server)!=ERROR_SUCCESS)continue;
        wchar_t path[32768]{};DWORD bytes=sizeof(path),type=0;
        auto read=RegQueryValueExW(server,nullptr,nullptr,&type,(BYTE*)path,&bytes);RegCloseKey(server);
        if(read!=ERROR_SUCCESS || type!=REG_SZ || bytes>sizeof(path)-sizeof(wchar_t))continue;
        auto dll=fs::path(path),folder=dll.parent_path();
        if(_wcsicmp(dll.filename().c_str(),L"FuyiIME.dll") || _wcsicmp(folder.parent_path().c_str(),root.c_str()) ||
            !olderRuntimeVersion(folder.filename().wstring(),runtimeVersion))continue;
        result.push_back(id);
    }
    RegCloseKey(classes);return result;
}
inline HRESULT activateReleaseForSession() {
    ITfInputProcessorProfiles *profiles=nullptr;
    auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,(void**)&profiles);
    ITfInputProcessorProfileMgr *manager=nullptr;
    if(SUCCEEDED(hr))hr=profiles->QueryInterface(IID_ITfInputProcessorProfileMgr,(void**)&manager);
    LANGID currentLanguage=0;
    if(SUCCEEDED(hr))hr=profiles->GetCurrentLanguage(&currentLanguage);
    if(SUCCEEDED(hr) && currentLanguage!=0x0804)hr=profiles->ChangeCurrentLanguage(0x0804);
    // Public TSF flags: ENABLEPROFILE | DONTCARECURRENTINPUTLANGUAGE | FORSESSION.
    // Other-language threads pick up the new TIP when switching to Chinese.
    // This asks Windows to switch
    // the input service; it does not send keystrokes or close any application.
    if(SUCCEEDED(hr)) {
        // Newly registered profiles can briefly be absent from TSF's cached
        // profile list. Retry the request after servicing its notifications.
        auto deadline=GetTickCount64()+2000;
        do {
            hr=manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,serviceId,profileId,nullptr,0x20000005);
            if(SUCCEEDED(hr) || hr!=E_FAIL || GetTickCount64()>=deadline)break;
            MsgWaitForMultipleObjectsEx(0,nullptr,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT){PostQuitMessage((int)message.wParam);break;}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }while(true);
    }
    if(SUCCEEDED(hr)) {
        // Activation is delivered asynchronously even after S_OK. Service the
        // caller's TSF notifications before judging the first upgrade a failure.
        auto deadline=GetTickCount64()+1500;
        for(;;) {
            TF_INPUTPROCESSORPROFILE active{};
            auto query=manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active);
            if(SUCCEEDED(query) && IsEqualGUID(active.clsid,serviceId) && IsEqualGUID(active.guidProfile,profileId)){hr=S_OK;break;}
            if(GetTickCount64()>=deadline){hr=FAILED(query)?query:E_PENDING;break;}
            MsgWaitForMultipleObjectsEx(0,nullptr,40,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT){PostQuitMessage((int)message.wParam);break;}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }
    }
    if(SUCCEEDED(hr))for(auto &id:olderRegisteredServices()) {
        // Remove only this user's obsolete FloatingTranslate entries after the
        // replacement is active. Keep machine registration for other users and
        // keep all still-mapped modules intact.
        installUserProfile(id,false);
        profiles->EnableLanguageProfile(id,0x0804,profileId,FALSE);
    }
    if(manager)manager->Release();if(profiles)profiles->Release();return hr;
}
inline bool registerStartupMaintenance() {
    auto runtime=registeredRuntime();
    auto expected=knownFolder(FOLDERID_ProgramFiles)/L"FloatingTranslate";
    if(runtime.filename()!=runtimeVersion || _wcsicmp(runtime.parent_path().c_str(),expected.c_str()) || !noLinks(runtime))return false;
    auto command=L"\""+(runtime/L"FuyiSettings.exe").wstring()+L"\" --maintain";
    HKEY key=nullptr;
    auto created=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(created!=ERROR_SUCCESS)return false;
    auto saved=RegSetValueExW(key,L"FloatingTranslateMaintenance",0,REG_SZ,(const BYTE*)command.c_str(),(DWORD)((command.size()+1)*sizeof(wchar_t)));RegCloseKey(key);
    return saved==ERROR_SUCCESS;
}
inline bool startupProfileUpgrade() {
    if(!installUserProfile(serviceId,true))return false;
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\FloatingTranslate",0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    wchar_t previous[64]{};DWORD bytes=sizeof(previous),type=0;
    bool current=RegQueryValueExW(key,L"SessionActivatedVersion",nullptr,&type,(BYTE*)previous,&bytes)==ERROR_SUCCESS &&
        type==REG_SZ && bytes<sizeof(previous) && std::wstring(previous)==runtimeVersion;
    HRESULT hr=current?S_OK:activateReleaseForSession();
    if(!current && hr==E_FAIL)hr=activateReleaseForSession();
    if(SUCCEEDED(hr) && !current)hr=HRESULT_FROM_WIN32(RegSetValueExW(key,L"SessionActivatedVersion",0,REG_SZ,(const BYTE*)runtimeVersion,(DWORD)((wcslen(runtimeVersion)+1)*sizeof(wchar_t))));
    RegCloseKey(key);
    if(FAILED(hr)){std::cerr<<"Session profile activation failed: 0x"<<std::hex<<(unsigned)hr<<"\n";return false;}
    // The user explicitly requested automatic startup maintenance. The signed-
    // in user's Run entry points to the verified Program Files copy, not an
    // arbitrary extraction directory. No interactive UI or forced restart.
    return registerStartupMaintenance();
}
}
