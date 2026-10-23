#pragma once
#include "Core.h"
#include "PanelLayout.h"
#include "WinUtil.h"
#include <windowsx.h>
#include <commctrl.h>
namespace fuyi {
class Panel {
    HWND window_ = nullptr, tooltip_ = nullptr;
    std::wstring pendingCopy_, copyTip_ = L"复制译文";
    int copyState_ = 0, copyAttempts_ = 0;
    DWORD copyError_ = ERROR_SUCCESS;
    static constexpr UINT_PTR copyRetryTimer = 2, copyFeedbackTimer = 3;
    RECT copyHit() const {
        return {copy_.left - d(2), copy_.top - d(4), copy_.right + d(5), copy_.bottom + d(4)};
    }
    bool onCopy(POINT p) const { auto r = copyHit(); return PtInRect(&r, p) != FALSE; }
    void copyFeedback(int state, const std::wstring &tip) {
        copyState_ = state; copyTip_ = tip;
        if (tooltip_) {
            TOOLINFOW tool = {sizeof(tool)};
            tool.hwnd = window_; tool.uId = 1; tool.lpszText = copyTip_.data();
            SendMessageW(tooltip_, TTM_UPDATETIPTEXTW, 0, (LPARAM)&tool);
        }
        InvalidateRect(window_, nullptr, FALSE);
    }
    void attemptCopy() {
        copyError_ = clipboard(window_, pendingCopy_);
        if (copyError_ == ERROR_SUCCESS) {
            KillTimer(window_, copyRetryTimer); pendingCopy_.clear();
            copyFeedback(1, L"已复制译文，可粘贴到目标输入框");
            SetTimer(window_, copyFeedbackTimer, 1600, nullptr);
        } else if (++copyAttempts_ >= 12 || copyError_ == ERROR_NO_DATA) {
            KillTimer(window_, copyRetryTimer); pendingCopy_.clear();
            copyFeedback(2, copyError_ == ERROR_NO_DATA ? L"译文翻译尚未就绪" :
                copyError_ == ERROR_ACCESS_DENIED ? L"系统暂不允许访问剪贴板，请重试或检查云电脑剪贴板设置" :
                                                  L"剪贴板暂时被占用，点击重新复制");
            SetTimer(window_, copyFeedbackTimer, 2400, nullptr);
        } else {
            copyFeedback(3, L"正在重试复制译文…");
            SetTimer(window_, copyRetryTimer, 80, nullptr);
        }
    }
    HFONT font_ = nullptr, small_ = nullptr, candidateFont_ = nullptr;
    int scale_ = 96, width_ = 136, height_ = 34, scroll_ = 0;
    PanelLayout layout_;
    std::wstring english_, status_, raw_;
    std::vector<Candidate> candidates_;
    size_t selected_ = 0, selA_ = 0, selB_ = 0;
    bool dragging_ = false, copyHover_ = false, toChinese_ = false;
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
            CreateFontW(-d(PanelStyle::englishSize), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (candidateFont_) DeleteObject(candidateFont_);
        candidateFont_ = CreateFontW(-d(PanelStyle::candidateSize), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        small_ =
            CreateFontW(-d(PanelStyle::labelSize), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
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
        if (tooltip_) {
            TOOLINFOW tool = {sizeof(tool)};
            tool.hwnd = window_; tool.uId = 1; tool.rect = copyHit();
            SendMessageW(tooltip_, TTM_NEWTOOLRECTW, 0, (LPARAM)&tool);
        }
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
    void surface(HDC dc, const RECT &rect, int diameter, COLORREF top, COLORREF bottom,
                 COLORREF outline = CLR_INVALID) {
        int saved = SaveDC(dc);
        HRGN shape = CreateRoundRectRgn(rect.left, rect.top, rect.right, rect.bottom, diameter, diameter);
        ExtSelectClipRgn(dc, shape, RGN_AND); DeleteObject(shape);
        int height = std::max(1L, rect.bottom - rect.top - 1);
        auto blend = [height](int a, int b, int y) { return a + (b - a) * y / height; };
        for (int y = rect.top; y < rect.bottom; ++y) {
            int t = y - rect.top;
            SetDCBrushColor(dc, RGB(blend(GetRValue(top), GetRValue(bottom), t),
                                   blend(GetGValue(top), GetGValue(bottom), t),
                                   blend(GetBValue(top), GetBValue(bottom), t)));
            RECT band = {rect.left, y, rect.right, y + 1};
            FillRect(dc, &band, (HBRUSH)GetStockObject(DC_BRUSH));
        }
        RestoreDC(dc, saved);
        if (outline != CLR_INVALID) {
            auto pen = CreatePen(PS_SOLID, 1, outline);
            auto previousPen = SelectObject(dc, pen);
            auto previousBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, diameter, diameter);
            SelectObject(dc, previousBrush); SelectObject(dc, previousPen); DeleteObject(pen);
        }
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
        RECT card = {0, 0, width_, height_};
        surface(back, card, d(PanelStyle::cornerDiameter), RGB(248, 249, 251), RGB(238, 240, 244),
                RGB(202, 207, 214));
        // Keep both text and selection inside the rounded, transparent window.
        HRGN clip = CreateRoundRectRgn(1, 1, width_ - 1, height_ - 1, d(PanelStyle::cornerDiameter), d(PanelStyle::cornerDiameter));
        SelectClipRgn(back, clip); DeleteObject(clip);
        SetBkMode(back, TRANSPARENT);
        SelectObject(back, small_); SetTextColor(back, RGB(133, 140, 150));
        TextOutW(back, d(11), d(12) - scroll_, toChinese_ ? L"中" : L"EN", toChinese_ ? 1 : 2);
        SelectObject(back, font_);
        const auto &text = english_.empty() ? status_ : english_;
        size_t lo = std::min(selA_, selB_), hi = std::max(selA_, selB_);
        for (auto &line : lines_) {
            int y = line.rect.top - scroll_;
            if (line.rect.bottom - scroll_ < 0 || y >= height_) continue;
            SetTextColor(back, english_.empty() ? RGB(130, 137, 147) : RGB(49, 54, 62));
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
            auto divider = CreatePen(PS_SOLID, 1, RGB(218, 222, 228));
            auto prev = SelectObject(back, divider);
            MoveToEx(back, d(8), layout_.divider - scroll_, nullptr);
            LineTo(back, width_ - d(8), layout_.divider - scroll_);
            SelectObject(back, prev); DeleteObject(divider);
        }
        for (const auto &choice : layout_.choices) {
            RECT r = {choice.rect.left, choice.rect.top - scroll_, choice.rect.right, choice.rect.bottom - scroll_};
            if (r.bottom < 0 || r.top >= height_) continue;
            bool active = choice.index == selected_;
            if (active) {
                surface(back, r, d(26), RGB(38, 145, 255), RGB(0, 119, 245));
            }
            SetTextColor(back, active ? RGB(255, 255, 255) : RGB(70, 76, 86));
            SelectObject(back, small_);
            auto number = std::to_wstring(choice.index % 5 + 1);
            TextOutW(back, r.left + d(7), r.top + d(11), number.c_str(), (int)number.size());
            SelectObject(back, candidateFont_);
            const auto &value = candidates_[choice.index].text;
            for (const auto &line : choice.lines)
                TextOutW(back, line.rect.left, line.rect.top - scroll_, value.data() + line.start, (int)line.length);
        }
        // A compact two-paper copy glyph, like the Mac panel; no wide text button.
        if (copyHover_)
            surface(back, copy_, d(8), RGB(226, 234, 245), RGB(226, 234, 245));
        COLORREF ink = copyState_ == 1 ? RGB(37, 145, 100) : copyState_ == 2 ? RGB(194, 77, 66) :
                       copyHover_ ? RGB(0, 112, 232) : RGB(131, 139, 151);
        auto paper = CreatePen(PS_SOLID, std::max(1, d(1)), ink);
        auto oldPen = SelectObject(back, paper);
        auto oldBrush = SelectObject(back, GetStockObject(NULL_BRUSH));
        if (copyState_ == 1) {
            MoveToEx(back, copy_.left + d(5), copy_.top + d(11), nullptr);
            LineTo(back, copy_.left + d(9), copy_.top + d(15));
            LineTo(back, copy_.left + d(17), copy_.top + d(6));
        } else if (copyState_ == 2) {
            MoveToEx(back, copy_.left + d(11), copy_.top + d(5), nullptr);
            LineTo(back, copy_.left + d(11), copy_.top + d(13));
            Rectangle(back, copy_.left + d(10), copy_.top + d(16), copy_.left + d(12), copy_.top + d(18));
        } else {
            RoundRect(back, copy_.left + d(8), copy_.top + d(3), copy_.right - d(4), copy_.bottom - d(7), d(3), d(3));
            RoundRect(back, copy_.left + d(4), copy_.top + d(7), copy_.right - d(8), copy_.bottom - d(3), d(3), d(3));
        }
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
        case WM_TIMER:
            if (a == copyRetryTimer && !pendingCopy_.empty()) attemptCopy();
            if (a == copyFeedbackTimer) { KillTimer(window_, copyFeedbackTimer); copyFeedback(0, L"复制译文"); }
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint();
            return 0;
        case WM_LBUTTONDOWN:
            if (onCopy(point)) {
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
        case WM_MOUSELEAVE:
            copyHover_ = false;
            InvalidateRect(window_, &copy_, FALSE);
            return 0;
        case WM_MOUSEMOVE: {
            bool hover = onCopy(point) != FALSE;
            if (hover != copyHover_) {
                copyHover_ = hover;
                InvalidateRect(window_, &copy_, FALSE);
            }
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window_, 0};
            TrackMouseEvent(&track);
            if (hover) SetCursor(LoadCursorW(nullptr, IDC_HAND));
            if (dragging_) {
                selB_ = indexAt(point);
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        }
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
            AppendMenuW(menu, MF_STRING, 1, selectedText().empty() ? L"复制整句译文" : L"复制选中译文");
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
        // A window region, rather than colour-key transparency alone, excludes
        // the corners even in remote desktop compositors.
        wc.style = 0;
        RegisterClassW(&wc);
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName,
                        L"浮译", WS_POPUP, 0, 0, width_, height_, nullptr, nullptr, module, this);
        if (window_)
            SetLayeredWindowAttributes(window_, RGB(1, 2, 3), 255, LWA_COLORKEY);
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_WIN95_CLASSES};
        InitCommonControlsEx(&controls);
        tooltip_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, window_, nullptr, module, nullptr);
        if (tooltip_) {
            TOOLINFOW tool = {sizeof(tool)};
            tool.hwnd = window_; tool.uId = 1; tool.uFlags = TTF_SUBCLASS;
            tool.rect = copyHit(); tool.lpszText = copyTip_.data();
            SendMessageW(tooltip_, TTM_ADDTOOLW, 0, (LPARAM)&tool);
        }
        fonts();
    }
    ~Panel() {
        if (tooltip_) DestroyWindow(tooltip_);
        if (window_)
            DestroyWindow(window_);
        if (font_)
            DeleteObject(font_);
        if (small_)
            DeleteObject(small_);
        if (candidateFont_) DeleteObject(candidateFont_);
    }
    void hide() {
        pendingCopy_.clear();
        KillTimer(window_, copyRetryTimer);
        KillTimer(window_, copyFeedbackTimer);
        copyState_ = 0;
        if (window_)
            ShowWindow(window_, SW_HIDE);
        if (GetCapture() == window_) ReleaseCapture();
        dragging_ = false;
        copyHover_ = false;
        scroll_ = 0;
        selA_ = selB_ = 0;
    }
    bool selecting() const {
        return dragging_ || !pendingCopy_.empty();
    }
    std::wstring selectedText() const {
        size_t a = std::min(selA_, selB_), b = std::min(std::max(selA_, selB_), english_.size());
        return b > a ? english_.substr(a, b - a) : L"";
    }
    void copy() {
        KillTimer(window_, copyFeedbackTimer);
        pendingCopy_ = selectedText();
        if (pendingCopy_.empty()) pendingCopy_ = english_;
        copyAttempts_ = 0;
        attemptCopy();
    }
    void show(RECT anchor, const std::wstring &english, const std::wstring &status, const std::wstring &raw,
              const std::vector<Candidate> &choices, size_t selected, bool above, bool toChinese = false) {
        if (!window_)
            return;
        if (english_ != english) {
            KillTimer(window_, copyRetryTimer); KillTimer(window_, copyFeedbackTimer);
            pendingCopy_.clear(); copyFeedback(0, L"复制译文");
            selA_ = selB_ = 0;
            scroll_ = 0;
        }
        toChinese_ = toChinese;
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
        HRGN shape = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, d(PanelStyle::cornerDiameter), d(PanelStyle::cornerDiameter));
        if (shape && !SetWindowRgn(window_, shape, FALSE)) DeleteObject(shape);
        SetWindowPos(window_, HWND_TOPMOST, r->left, r->top, width_, height_, SWP_NOACTIVATE);
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        ShowWindow(window_, SW_SHOWNOACTIVATE);
    }
};
} // namespace fuyi
