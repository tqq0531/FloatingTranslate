// Render fixed sample data with the production painter, without capturing any
// desktop windows, registering TSF, or reading user input.
#define FUYI_PANEL_PREVIEW
#include "../src/Panel.h"
#include <cassert>
namespace fuyi {
HINSTANCE module = nullptr;
struct PanelPreview {
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
    if (argc < 3) return 2;
    fuyi::PanelDpiScope dpi;
    fuyi::PanelPreview::save(argv[1], _wtoi(argv[2]), argc > 3);
}
