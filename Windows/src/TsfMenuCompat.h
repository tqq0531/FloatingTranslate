#pragma once
#include <ctfutb.h>
// These public COM interfaces are absent from LLVM-MinGW's ctfutb.h.
// ABI follows Microsoft's Windows SDK ctfutb.idl / win32metadata declarations.
#ifndef __ITfMenu_INTERFACE_DEFINED__
#define __ITfMenu_INTERFACE_DEFINED__
struct ITfMenu : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE AddMenuItem(UINT, DWORD, HBITMAP, HBITMAP, const WCHAR *, ULONG,
                                                  ITfMenu **) = 0;
};
#endif
#ifndef __ITfLangBarItemButton_INTERFACE_DEFINED__
#define __ITfLangBarItemButton_INTERFACE_DEFINED__
enum TfLBIClick { TF_LBI_CLK_RIGHT = 1, TF_LBI_CLK_LEFT = 2 };
inline const IID IID_ITfLangBarItemButton = {
    0x28c7f1d0, 0xde25, 0x11d2, {0xaf, 0xdd, 0, 0x10, 0x5a, 0x27, 0x99, 0xb5}};
struct ITfLangBarItemButton : public ITfLangBarItem {
    virtual HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick, POINT, const RECT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu *) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIcon(HICON *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetText(BSTR *) = 0;
};
#endif
#ifndef TF_LBI_STYLE_BTN_MENU
#define TF_LBI_STYLE_BTN_MENU 0x00020000
#define TF_LBI_STYLE_SHOWNINTRAY 0x00000002
#define TF_LBMENUF_CHECKED 0x1
#endif

#ifndef TF_LBI_TEXT
#define TF_LBI_ICON 0x1
#define TF_LBI_TEXT 0x2
#endif
