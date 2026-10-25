// Render fixed sample data with the production painter, without capturing any
// desktop windows, registering TSF, or reading user input.
#define FUYI_PANEL_PREVIEW
#include "../src/Panel.h"
#include <cassert>
#include <iostream>
namespace fuyi {
HINSTANCE module = nullptr;
struct PanelPreview {
    static void nativeCheck() {
        // Multiple input contexts can share a UI thread. Releasing one panel
        // must not shut down another panel's compositor dispatcher.
        auto earlier = std::make_unique<Panel>();
        earlier->english_=L"Earlier input context";
        earlier->createWindow(); earlier->layout(500,300);
        HDC earlierDC=GetDC(earlier->window_);
        earlier->renderGlass(earlierDC); ReleaseDC(earlier->window_,earlierDC);
        Panel panel;
        panel.english_=L"Native alpha edge check";
        panel.createWindow(); panel.layout(500,300);
        SetWindowPos(panel.window_,nullptr,0,0,panel.width_,panel.height_,SWP_NOACTIVATE|SWP_NOZORDER);
        HDC dc=GetDC(panel.window_); panel.renderGlass(dc); ReleaseDC(panel.window_,dc);
        assert(panel.composition_ || panel.layered_);
        if(panel.composition_)assert(panel.compositionAtCreation_);
        bool composed=panel.composition_!=nullptr;
        earlier.reset();
        for(int i=0;i<12;++i) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
            HDC redraw=GetDC(panel.window_);panel.renderGlass(redraw);ReleaseDC(panel.window_,redraw);
            if(composed)assert(panel.composition_);
        }
        std::cout<<"Native surface accepted; unified composition="<<(panel.composition_!=nullptr)<<" backdrop="<<panel.glass_<<"\n";
    }
    static void save(const wchar_t *file, int dpi, bool glass) {
        Panel panel;
        panel.scale_ = dpi; panel.fonts();
        panel.glass_ = glass;
        panel.english_ = L"I went to Beijing today.";
        panel.candidates_ = {{L"我今天", 9}, {L"我", 2}, {L"今天", 7}, {L"北京", 7}, {L"去", 2}};
        panel.layout(MulDiv(600, dpi, 96), MulDiv(500, dpi, 96));
        std::unique_ptr<Gdiplus::Bitmap> image;
        if (glass) {
            HDC dc = GetDC(nullptr); image = panel.glassImage(dc); ReleaseDC(nullptr, dc);
        } else {
            image = std::make_unique<Gdiplus::Bitmap>(panel.width_, panel.height_, PixelFormat32bppPARGB);
            Gdiplus::Graphics graphics(image.get());
            HDC dc = graphics.GetHDC(); panel.render(dc); graphics.ReleaseHDC(dc);
        }
        UINT count = 0, bytes = 0;
        Gdiplus::GetImageEncodersSize(&count, &bytes);
        std::vector<unsigned char> data(bytes);
        auto encoders = (Gdiplus::ImageCodecInfo *)data.data();
        Gdiplus::GetImageEncoders(count, bytes, encoders);
        bool saved = false;
        for (UINT i = 0; i < count; ++i)
            if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
                saved = image->Save(file, &encoders[i].Clsid, nullptr) == Gdiplus::Ok;
        assert(saved);
    }
};
}
int wmain(int argc, wchar_t **argv) {
    if(argc==2 && wcscmp(argv[1],L"--native-check")==0){fuyi::PanelPreview::nativeCheck();return 0;}
    if (argc < 3) return 2;
    fuyi::PanelDpiScope dpi;
    fuyi::PanelPreview::save(argv[1], _wtoi(argv[2]), argc > 3);
}
