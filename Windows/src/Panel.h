#pragma once
#include "Core.h"
#include "WinUtil.h"
#include <windowsx.h>
namespace fuyi {
class Panel {
    HWND window_ = nullptr;
    HFONT font_ = nullptr, small_ = nullptr;
    int scale_ = 96, width_ = 580, height_ = 120;
    std::wstring english_, status_, raw_;
    std::vector<Candidate> candidates_;
    size_t selected_ = 0, selA_ = 0, selB_ = 0;
    bool dragging_ = false;
    struct Line {
        size_t start, length;
        RECT rect;
    };
    std::vector<Line> lines_;
    RECT copy_{}, choices_{};
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
            CreateFontW(-d(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        small_ =
            CreateFontW(-d(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    void layout() {
        HDC dc = GetDC(window_);
        auto old = SelectObject(dc, font_);
        lines_.clear();
        const auto &text = english_.empty() ? status_ : english_;
        size_t start = 0;
        int y = d(17), available = width_ - d(112);
        while (start < text.size()) {
            int fit = 0;
            SIZE size;
            GetTextExtentExPointW(dc, text.data() + start, (int)(text.size() - start), available, &fit,
                                  nullptr, &size);
            size_t count = std::max(1, fit);
            auto newline = text.find(L'\n', start);
            if (newline != std::wstring::npos && newline < start + count)
                count = newline - start + 1;
            else if (start + count < text.size()) {
                auto space = text.rfind(L' ', start + count);
                if (space != std::wstring::npos && space > start)
                    count = space - start + 1;
            }
            lines_.push_back({start, count, {d(46), y, width_ - d(58), y + d(27)}});
            y += d(27);
            start += count;
        }
        if (lines_.empty())
            y += d(27);
        copy_ = {width_ - d(49), d(12), width_ - d(9), d(48)};
        if (!raw_.empty())
            y += d(29);
        choices_ = {d(12), y + d(7), width_ - d(12), y + d(52)};
        height_ = candidates_.empty() ? y + d(14) : choices_.bottom + d(12);
        SelectObject(dc, old);
        ReleaseDC(window_, dc);
    }
    size_t indexAt(POINT point) {
        if (english_.empty() || lines_.empty())
            return 0;
        const Line *line = &lines_.front();
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
        RECT client;
        GetClientRect(window_, &client);
        HDC back = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width_, height_);
        auto oldBitmap = SelectObject(back, bitmap);
        HBRUSH transparent = CreateSolidBrush(RGB(1, 2, 3));
        FillRect(back, &client, transparent);
        DeleteObject(transparent);
        auto brush = CreateSolidBrush(RGB(247, 248, 251));
        auto pen = CreatePen(PS_SOLID, 1, RGB(205, 211, 222));
        auto ob = SelectObject(back, brush), op = SelectObject(back, pen);
        RoundRect(back, 0, 0, width_, height_, d(20), d(20));
        SelectObject(back, ob);
        SelectObject(back, op);
        DeleteObject(brush);
        DeleteObject(pen);
        SetBkMode(back, TRANSPARENT);
        SelectObject(back, small_);
        SetTextColor(back, RGB(104, 115, 133));
        TextOutW(back, d(15), d(21), L"EN", 2);
        DrawTextW(back, L"复制", -1, &copy_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(back, font_);
        const auto &text = english_.empty() ? status_ : english_;
        size_t lo = std::min(selA_, selB_), hi = std::max(selA_, selB_);
        for (auto &line : lines_) {
            SetTextColor(back, english_.empty() ? RGB(126, 137, 151) : RGB(32, 43, 60));
            TextOutW(back, line.rect.left, line.rect.top, text.data() + line.start, (int)line.length);
            if (!english_.empty() && hi > line.start && lo < line.start + line.length) {
                size_t a = std::max(lo, line.start), b = std::min(hi, line.start + line.length);
                SIZE prefix, part;
                GetTextExtentPoint32W(back, text.data() + line.start, (int)(a - line.start), &prefix);
                GetTextExtentPoint32W(back, text.data() + a, (int)(b - a), &part);
                RECT box = {line.rect.left + prefix.cx, line.rect.top, line.rect.left + prefix.cx + part.cx,
                            line.rect.bottom};
                SetBkColor(back, RGB(57, 101, 238));
                SetTextColor(back, RGB(255, 255, 255));
                ExtTextOutW(back, box.left, box.top, ETO_OPAQUE, &box, text.data() + a, (UINT)(b - a),
                            nullptr);
            }
        }
        if (!raw_.empty()) {
            SetTextColor(back, RGB(111, 122, 141));
            auto r = RECT{d(18), choices_.top - d(29), width_ - d(18), choices_.top};
            SelectObject(back, small_);
            DrawTextW(back, raw_.c_str(), -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            SelectObject(back, font_);
        }
        if (!candidates_.empty()) {
            size_t page = selected_ / 5;
            int cell = (choices_.right - choices_.left) / 5;
            for (size_t i = page * 5; i < candidates_.size() && i < page * 5 + 5; i++) {
                RECT r = {choices_.left + (int)(i % 5) * cell, choices_.top,
                          choices_.left + (int)(i % 5 + 1) * cell - d(4), choices_.bottom};
                if (i == selected_) {
                    auto b = CreateSolidBrush(RGB(42, 103, 246));
                    auto old = SelectObject(back, b);
                    auto p = SelectObject(back, GetStockObject(NULL_PEN));
                    RoundRect(back, r.left, r.top, r.right, r.bottom, d(18), d(18));
                    SelectObject(back, old);
                    SelectObject(back, p);
                    DeleteObject(b);
                }
                SetTextColor(back, i == selected_ ? RGB(255, 255, 255) : RGB(47, 58, 76));
                std::wstring value = std::to_wstring(i % 5 + 1) + L" " + candidates_[i].text;
                DrawTextW(back, value.c_str(), -1, &r,
                          DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }
        }
        BitBlt(dc, 0, 0, width_, height_, back, 0, 0, SRCCOPY);
        SelectObject(back, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(back);
        EndPaint(window_, &ps);
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
            if (!candidates_.empty() && PtInRect(&choices_, point)) {
                int cell = (choices_.right - choices_.left) / 5;
                size_t index = (selected_ / 5) * 5 + (point.x - choices_.left) / cell;
                if (index < candidates_.size() && choose)
                    choose(index);
                return 0;
            }
            if (!english_.empty()) {
                selA_ = selB_ = indexAt(point);
                dragging_ = true;
                SetCapture(window_);
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
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
    }
    void hide() {
        if (window_)
            ShowWindow(window_, SW_HIDE);
        dragging_ = false;
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
        }
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
        width_ = std::min(d(580), (int)(info.rcWork.right - info.rcWork.left) - d(8));
        layout();
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
