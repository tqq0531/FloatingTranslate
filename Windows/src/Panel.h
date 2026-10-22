#pragma once
#include "Core.h"
#include "PanelLayout.h"
#include "WinUtil.h"
#include <windowsx.h>
namespace fuyi {
class Panel {
    HWND window_ = nullptr;
    HFONT font_ = nullptr, small_ = nullptr, candidateFont_ = nullptr;
    int scale_ = 96, width_ = 160, height_ = 44, scroll_ = 0;
    PanelLayout layout_;
    std::wstring english_, status_, raw_;
    std::vector<Candidate> candidates_;
    size_t selected_ = 0, selA_ = 0, selB_ = 0;
    bool dragging_ = false;
    std::vector<TextLine> lines_;
    RECT copy_{};
    int d(int value) const {
        return MulDiv(value, scale_, 96);
    }
    static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
        auto *self = (Panel *)GetWindowLongPtrW(w, GWLP_USERDATA);
        if (m == WM_NCCREATE) {
            self = (Panel *)((CREATESTRUCTW *)b)->lpCreateParams;
            self->window_ = w;
            SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)self);
        }
        return self ? self->message(m, a, b) : DefWindowProcW(w, m, a, b);
    }
    void fonts() {
        if (font_)
            DeleteObject(font_);
        if (small_)
            DeleteObject(small_);
        font_ =
            CreateFontW(-d(16), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (candidateFont_) DeleteObject(candidateFont_);
        candidateFont_ = CreateFontW(-d(21), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        small_ =
            CreateFontW(-d(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    void layout(int maxWidth, int maxHeight) {
        HDC dc = GetDC(window_);
        auto old = SelectObject(dc, font_);
        auto measure = [&](const std::wstring &text, size_t start, size_t count, PanelFont font) {
            SelectObject(dc, font == PanelFont::Candidate ? candidateFont_ : font_);
            SIZE size{};
            GetTextExtentPoint32W(dc, text.data() + start, (int)count, &size);
            return (int)size.cx;
        };
        layout_ = layoutPanel(english_.empty() ? status_ : english_, candidates_, selected_,
                              scale_, maxWidth, maxHeight, measure);
        width_ = layout_.width; height_ = layout_.height; lines_ = layout_.english;
        copy_ = {layout_.copy.left, layout_.copy.top, layout_.copy.right, layout_.copy.bottom};
        scroll_ = std::clamp(scroll_, 0, std::max(0, layout_.contentHeight - height_));
        SelectObject(dc, old);
        ReleaseDC(window_, dc);
    }
    size_t indexAt(POINT point) {
        point.y += scroll_;
        if (english_.empty() || lines_.empty())
            return 0;
        const TextLine *line = &lines_.front();
        for (auto &current : lines_)
            if (point.y >= current.rect.top)
                line = &current;
        HDC dc = GetDC(window_);
        auto old = SelectObject(dc, font_);
        size_t index = line->start;
        for (size_t i = 0; i <= line->length; i++) {
            SIZE size;
            GetTextExtentPoint32W(dc, english_.data() + line->start, (int)i, &size);
            if (line->rect.left + size.cx > point.x)
                break;
            index = line->start + i;
        }
        SelectObject(dc, old);
        ReleaseDC(window_, dc);
        return index;
    }
    void paint() {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window_, &ps);
        RECT client; GetClientRect(window_, &client);
        HDC back = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width_, height_);
        auto oldBitmap = SelectObject(back, bitmap);
        HBRUSH transparent = CreateSolidBrush(RGB(1, 2, 3));
        FillRect(back, &client, transparent); DeleteObject(transparent);
        auto brush = CreateSolidBrush(RGB(239, 239, 237));
        auto pen = CreatePen(PS_SOLID, 1, RGB(182, 182, 180));
        auto ob = SelectObject(back, brush), op = SelectObject(back, pen);
        RoundRect(back, 0, 0, width_, height_, d(22), d(22));
        SelectObject(back, ob); SelectObject(back, op); DeleteObject(brush); DeleteObject(pen);
        // Keep both text and selection inside the rounded, transparent window.
        HRGN clip = CreateRoundRectRgn(1, 1, width_ - 1, height_ - 1, d(22), d(22));
        SelectClipRgn(back, clip); DeleteObject(clip);
        SetBkMode(back, TRANSPARENT);
        SelectObject(back, small_); SetTextColor(back, RGB(105, 105, 103));
        TextOutW(back, d(15), d(16) - scroll_, L"EN", 2);
        SelectObject(back, font_);
        const auto &text = english_.empty() ? status_ : english_;
        size_t lo = std::min(selA_, selB_), hi = std::max(selA_, selB_);
        for (auto &line : lines_) {
            int y = line.rect.top - scroll_;
            if (line.rect.bottom - scroll_ < 0 || y >= height_) continue;
            SetTextColor(back, english_.empty() ? RGB(118, 118, 116) : RGB(65, 65, 63));
            TextOutW(back, line.rect.left, y, text.data() + line.start, (int)line.length);
            if (!english_.empty() && hi > line.start && lo < line.start + line.length) {
                size_t a = std::max(lo, line.start), b = std::min(hi, line.start + line.length);
                SIZE prefix{}, part{};
                GetTextExtentPoint32W(back, text.data() + line.start, (int)(a - line.start), &prefix);
                GetTextExtentPoint32W(back, text.data() + a, (int)(b - a), &part);
                RECT box = {line.rect.left + prefix.cx, y, line.rect.left + prefix.cx + part.cx,
                            line.rect.bottom - scroll_};
                SetBkColor(back, RGB(0, 122, 255)); SetTextColor(back, RGB(255, 255, 255));
                ExtTextOutW(back, box.left, box.top, ETO_OPAQUE, &box, text.data() + a, (UINT)(b - a), nullptr);
            }
        }
        if (layout_.divider) {
            auto divider = CreatePen(PS_SOLID, 1, RGB(207, 207, 204));
            auto prev = SelectObject(back, divider);
            MoveToEx(back, d(10), layout_.divider - scroll_, nullptr);
            LineTo(back, width_ - d(10), layout_.divider - scroll_);
            SelectObject(back, prev); DeleteObject(divider);
        }
        for (const auto &choice : layout_.choices) {
            RECT r = {choice.rect.left, choice.rect.top - scroll_, choice.rect.right, choice.rect.bottom - scroll_};
            if (r.bottom < 0 || r.top >= height_) continue;
            bool active = choice.index == selected_;
            if (active) {
                auto b = CreateSolidBrush(RGB(0, 122, 255));
                auto old = SelectObject(back, b); auto previous = SelectObject(back, GetStockObject(NULL_PEN));
                RoundRect(back, r.left, r.top, r.right, r.bottom, d(30), d(30));
                SelectObject(back, old); SelectObject(back, previous); DeleteObject(b);
            }
            SetTextColor(back, active ? RGB(255, 255, 255) : RGB(74, 74, 72));
            SelectObject(back, small_);
            auto number = std::to_wstring(choice.index % 5 + 1);
            TextOutW(back, r.left + d(9), r.top + d(14), number.c_str(), (int)number.size());
            SelectObject(back, candidateFont_);
            const auto &value = candidates_[choice.index].text;
            for (const auto &line : choice.lines)
                TextOutW(back, line.rect.left, line.rect.top - scroll_, value.data() + line.start, (int)line.length);
        }
        // A compact two-paper copy glyph, like the Mac panel; no wide text button.
        auto paper = CreatePen(PS_SOLID, std::max(1, d(1)), RGB(120, 120, 117));
        auto oldPen = SelectObject(back, paper);
        auto oldBrush = SelectObject(back, GetStockObject(NULL_BRUSH));
        RoundRect(back, copy_.left + d(8), copy_.top + d(3), copy_.right - d(3), copy_.bottom - d(7), d(3), d(3));
        RoundRect(back, copy_.left + d(3), copy_.top + d(8), copy_.right - d(8), copy_.bottom - d(2), d(3), d(3));
        SelectObject(back, oldBrush); SelectObject(back, oldPen); DeleteObject(paper);
        if (layout_.contentHeight > height_) {
            int track = height_ - d(20), thumb = std::max(d(18), track * height_ / layout_.contentHeight);
            int top = d(10) + (track - thumb) * scroll_ / (layout_.contentHeight - height_);
            RECT bar = {width_ - d(5), top, width_ - d(3), top + thumb};
            auto b = CreateSolidBrush(RGB(162, 162, 158)); FillRect(back, &bar, b); DeleteObject(b);
        }
        SelectClipRgn(back, nullptr);
        BitBlt(dc, 0, 0, width_, height_, back, 0, 0, SRCCOPY);
        SelectObject(back, oldBitmap); DeleteObject(bitmap); DeleteDC(back); EndPaint(window_, &ps);
    }
    LRESULT message(UINT m, WPARAM a, LPARAM b) {
        POINT point = {GET_X_LPARAM(b), GET_Y_LPARAM(b)};
        switch (m) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint();
            return 0;
        case WM_LBUTTONDOWN:
            if (PtInRect(&copy_, point)) {
                copy();
                return 0;
            }
            if (layout_.contentHeight > height_ && point.x >= width_ - d(9)) {
                scroll_ = std::clamp((int)point.y * (layout_.contentHeight - height_) / std::max(1, height_),
                                     0, layout_.contentHeight - height_);
                InvalidateRect(window_, nullptr, FALSE); return 0;
            }
            for (const auto &choice : layout_.choices) {
                RECT r = {choice.rect.left, choice.rect.top - scroll_, choice.rect.right, choice.rect.bottom - scroll_};
                if (PtInRect(&r, point)) {
                    if (choose) choose(choice.index);
                    return 0;
                }
            }
            if (!english_.empty()) {
                selA_ = selB_ = indexAt(point);
                dragging_ = true;
                SetCapture(window_);
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL:
            scroll_ = std::clamp(scroll_ - GET_WHEEL_DELTA_WPARAM(a) * d(36) / WHEEL_DELTA,
                                 0, std::max(0, layout_.contentHeight - height_));
            InvalidateRect(window_, nullptr, FALSE); return 0;
        case WM_MOUSEMOVE:
            if (dragging_) {
                selB_ = indexAt(point);
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                selB_ = indexAt(point);
                dragging_ = false;
                ReleaseCapture();
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        case WM_CAPTURECHANGED:
            dragging_ = false;
            return 0;
        case WM_RBUTTONUP: {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, selectedText().empty() ? L"复制整句英文" : L"复制选中英文");
            AppendMenuW(menu, MF_STRING, 2, L"浮译设置");
            POINT screen = point;
            ClientToScreen(window_, &screen);
            int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screen.x,
                                         screen.y, 0, window_, nullptr);
            DestroyMenu(menu);
            if (command == 1)
                copy();
            if (command == 2)
                settings();
            return 0;
        }
        }
        return DefWindowProcW(window_, m, a, b);
    }

  public:
    std::function<void(size_t)> choose;
    Panel() {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = proc;
        wc.hInstance = module;
        wc.lpszClassName = L"Fuyi.TranslationPanel.1";
        wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
        wc.style = CS_DROPSHADOW;
        RegisterClassW(&wc);
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName,
                        L"浮译", WS_POPUP, 0, 0, width_, height_, nullptr, nullptr, module, this);
        if (window_)
            SetLayeredWindowAttributes(window_, RGB(1, 2, 3), 255, LWA_COLORKEY);
        fonts();
    }
    ~Panel() {
        if (window_)
            DestroyWindow(window_);
        if (font_)
            DeleteObject(font_);
        if (small_)
            DeleteObject(small_);
        if (candidateFont_) DeleteObject(candidateFont_);
    }
    void hide() {
        if (window_)
            ShowWindow(window_, SW_HIDE);
        if (GetCapture() == window_) ReleaseCapture();
        dragging_ = false;
        scroll_ = 0;
        selA_ = selB_ = 0;
    }
    bool selecting() const {
        return dragging_;
    }
    std::wstring selectedText() const {
        size_t a = std::min(selA_, selB_), b = std::min(std::max(selA_, selB_), english_.size());
        return b > a ? english_.substr(a, b - a) : L"";
    }
    void copy() {
        clipboard(window_, selectedText().empty() ? english_ : selectedText());
    }
    void show(RECT anchor, const std::wstring &english, const std::wstring &status, const std::wstring &raw,
              const std::vector<Candidate> &choices, size_t selected, bool above) {
        if (!window_)
            return;
        if (english_ != english) {
            selA_ = selB_ = 0;
            scroll_ = 0;
        }
        bool selectionChanged = selected_ != selected;
        english_ = english;
        status_ = status;
        raw_ = raw;
        candidates_ = choices;
        selected_ = selected;
        MONITORINFO info = {sizeof(info)};
        auto monitor = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(monitor, &info)) {
            hide();
            return;
        }
        UINT dpi = 96;
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        if (shcore) {
            using Dpi = HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *);
            auto get = (Dpi)GetProcAddress(shcore, "GetDpiForMonitor");
            UINT y = 96;
            if (get)
                get(monitor, 0, &dpi, &y);
            FreeLibrary(shcore);
        }
        if ((int)dpi != scale_) {
            scale_ = (int)dpi;
            fonts();
        }
        layout((int)(info.rcWork.right - info.rcWork.left) - d(16),
               (int)(info.rcWork.bottom - info.rcWork.top) - d(16));
        if (selectionChanged)
            for (const auto &choice : layout_.choices)
                if (choice.index == selected_) {
                    if (choice.rect.top < scroll_) scroll_ = choice.rect.top;
                    else if (choice.rect.bottom > scroll_ + height_)
                        scroll_ = std::min(choice.rect.top, choice.rect.bottom - height_);
                }
        auto r = place({anchor.left, anchor.top, anchor.right, anchor.bottom}, width_, height_,
                       {info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom}, above);
        if (!r) {
            hide();
            return;
        }
        SetWindowPos(window_, HWND_TOPMOST, r->left, r->top, width_, height_,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(window_, nullptr, FALSE);
    }
};
} // namespace fuyi
