#pragma once
#include "Core.h"
#include "PanelLayout.h"
#include "WinUtil.h"
#include "Glass.h"
#include "CompositionBackdrop.h"
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
namespace fuyi {
// TSF runs inside hosts with different DPI awareness. Draw the popup at its
// monitor's physical resolution rather than letting Windows scale a bitmap.
struct PanelDpiScope {
    using SetContext = HANDLE(WINAPI *)(HANDLE);
    SetContext set = (SetContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
    HANDLE previous = set ? set((HANDLE)-4) : nullptr;
    ~PanelDpiScope() { if (set && previous) set(previous); }
};
class Panel {
#ifdef FUYI_PANEL_PREVIEW
    friend struct PanelPreview;
#endif
    HWND window_ = nullptr, tooltip_ = nullptr, owner_ = nullptr;
    ULONG_PTR graphicsToken_ = 0;
    bool glass_ = false;
    bool layered_ = false;
    bool compositionAtCreation_ = false;
    std::unique_ptr<CompositionBackdrop> composition_;
    HRESULT compositionError_=S_OK;
    void configureBackdrop() {
        bool wanted=graphicsToken_ && glassAllowed();
        if(wanted && composition_)return;
        composition_.reset();glass_=false;
        auto style=GetWindowLongPtrW(window_,GWL_EXSTYLE)&~(WS_EX_LAYERED|WS_EX_NOREDIRECTIONBITMAP);
        if(wanted) {
            SetWindowLongPtrW(window_,GWL_EXSTYLE,style|WS_EX_NOREDIRECTIONBITMAP);
            auto candidate=std::make_unique<CompositionBackdrop>();
            if(candidate->start(window_)){composition_=std::move(candidate);glass_=true;compositionError_=S_OK;}
            else compositionError_=candidate->error();
        }
        layered_=graphicsToken_ && !composition_;
        if(!composition_)SetWindowLongPtrW(window_,GWL_EXSTYLE,style|(layered_?WS_EX_LAYERED:0));
    }
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
    std::wstring english_, status_, raw_, correction_, advice_;
    std::vector<TextLine> proofLines_;
    RECT applyRect_{}, proofCopyRect_{};
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
            self->compositionAtCreation_ = (((CREATESTRUCTW *)b)->dwExStyle & WS_EX_NOREDIRECTIONBITMAP) != 0;
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
        layout_ = layoutPanel(english_.empty()?status_:english_, candidates_, selected_,
                              scale_,maxWidth,maxHeight,measure,correction_.empty()?L"":L"基础纠错建议: "+correction_);
        proofLines_=layout_.proof;
        applyRect_={layout_.applyAdvice.left,layout_.applyAdvice.top,layout_.applyAdvice.right,layout_.applyAdvice.bottom};
        proofCopyRect_={layout_.copyAdvice.left,layout_.copyAdvice.top,layout_.copyAdvice.right,layout_.copyAdvice.bottom};
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
        if (graphicsToken_) {
            Gdiplus::Graphics graphics(dc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            const float inset=0.75f;
            float x=rect.left+inset,y=rect.top+inset;
            float w=rect.right-rect.left-2*inset,h=rect.bottom-rect.top-2*inset;
            float arc=std::min<float>(diameter,std::min(w,h));
            if(w<=0 || h<=0)return;
            Gdiplus::GraphicsPath path;
            path.AddArc(x,y,arc,arc,180,90);
            path.AddArc(x+w-arc,y,arc,arc,270,90);
            path.AddArc(x+w-arc,y+h-arc,arc,arc,0,90);
            path.AddArc(x,y+h-arc,arc,arc,90,90);
            path.CloseFigure();
            auto color=[](COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));};
            Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(x,y),Gdiplus::PointF(x,y+h),color(top),color(bottom));
            graphics.FillPath(&fill,&path);
            if(outline!=CLR_INVALID){Gdiplus::Pen pen(color(outline),std::max(1.0f,scale_/96.0f));graphics.DrawPath(&pen,&path);}
            return;
        }
        int saved = SaveDC(dc);
        HRGN shape = CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, diameter, diameter);
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
            // Draw inward from the same pixel-aligned shape as the window
            // region. A centred one-pixel pen was clipped at the outer edge,
            // and remained only one physical pixel on high-DPI monitors.
            HRGN border = CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, diameter, diameter);
            HBRUSH brush = CreateSolidBrush(outline);
            FrameRgn(dc, border, brush, std::max(2, d(2)), std::max(2, d(2)));
            DeleteObject(brush); DeleteObject(border);
        }
    }
    std::unique_ptr<Gdiplus::Bitmap> glassImage(HDC dc) {
        // Draw into an explicit premultiplied-alpha surface. Making the whole
        // HWND translucent would also fade the text and selection; ordinary
        // GDI painting would overwrite the alpha needed by the DWM backdrop.
        auto result = std::make_unique<Gdiplus::Bitmap>(width_, height_, PixelFormat32bppPARGB);
        auto &image = *result;
        {
            Gdiplus::Graphics g(&image);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            auto rounded = [&](const RECT &r, float radius, Gdiplus::Color top, Gdiplus::Color bottom,
                               Gdiplus::Color stroke) {
                float x=r.left+0.75f, y=r.top+0.75f;
                float w=r.right-r.left-1.5f, h=r.bottom-r.top-1.5f;
                float arc=std::min(radius*2, std::min(w,h));
                if(w<=0 || h<=0)return;
                Gdiplus::GraphicsPath path;
                path.AddArc(x,y,arc,arc,180,90); path.AddArc(x+w-arc,y,arc,arc,270,90);
                path.AddArc(x+w-arc,y+h-arc,arc,arc,0,90); path.AddArc(x,y+h-arc,arc,arc,90,90);
                path.CloseFigure();
                Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(x,y),Gdiplus::PointF(x,y+h),top,bottom);
                g.FillPath(&brush,&path);
                Gdiplus::Pen pen(stroke, std::max(1.0f,scale_/96.0f)); g.DrawPath(&pen,&path);
            };
            RECT card{0,0,width_,height_};
            rounded(card, d(PanelStyle::cornerDiameter/2),
                    glass_?Gdiplus::Color(150,42,46,54):Gdiplus::Color(255,49,50,55),
                    glass_?Gdiplus::Color(145,26,30,38):Gdiplus::Color(255,37,38,42),
                    Gdiplus::Color(85,210,217,232));
            Gdiplus::Font english(dc,font_), small(dc,small_), candidate(dc,candidateFont_);
            Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
            format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap |
                                  Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
            auto text = [&](const std::wstring &value, const Gdiplus::Font &font, float x, float y, Gdiplus::Color color) {
                Gdiplus::SolidBrush brush(color);
                g.DrawString(value.data(), (INT)value.size(), &font, Gdiplus::PointF(x,y), &format, &brush);
            };
            g.SetClip(Gdiplus::Rect(d(3),d(3),width_-d(6),height_-d(6)));
            text(toChinese_?L"中":L"EN",small,d(11),d(12)-scroll_,Gdiplus::Color(255,166,174,189));
            auto &display=english_.empty()?status_:english_;
            size_t lo=std::min(selA_,selB_),hi=std::max(selA_,selB_);
            for(auto &line:lines_) {
                int y=line.rect.top-scroll_;
                auto value=display.substr(line.start,line.length);
                text(value,english,line.rect.left,y,english_.empty()?Gdiplus::Color(255,163,170,183):Gdiplus::Color(255,246,248,252));
                if(!english_.empty() && hi>line.start && lo<line.start+line.length) {
                    size_t a=std::max(lo,line.start),b=std::min(hi,line.start+line.length);
                    auto old=SelectObject(dc,font_); SIZE prefix{},part{};
                    GetTextExtentPoint32W(dc,english_.data()+line.start,(int)(a-line.start),&prefix);
                    GetTextExtentPoint32W(dc,english_.data()+a,(int)(b-a),&part); SelectObject(dc,old);
                    Gdiplus::Rect selected(line.rect.left+prefix.cx,y,part.cx,line.rect.bottom-line.rect.top);
                    Gdiplus::SolidBrush brush(Gdiplus::Color(255,20,115,240)); g.FillRectangle(&brush,selected);
                    auto state=g.Save(); g.SetClip(selected,Gdiplus::CombineModeIntersect);
                    text(value,english,line.rect.left,y,Gdiplus::Color(255,255,255,255)); g.Restore(state);
                }
            }
            if(layout_.divider) {
                Gdiplus::Pen divider(Gdiplus::Color(45,225,232,245),1);
                g.DrawLine(&divider,d(12),layout_.divider-scroll_,width_-d(12),layout_.divider-scroll_);
            }
            for(auto &choice:layout_.choices) {
                auto r=choice.rect; RECT box{r.left,r.top-scroll_,r.right,r.bottom-scroll_};
                bool selected=choice.index==selected_;
                if(selected)rounded(box,d(13),Gdiplus::Color(255,42,144,255),Gdiplus::Color(255,10,111,239),Gdiplus::Color(255,100,183,255));
                text(std::to_wstring(choice.index%5+1),small,r.left+d(7),r.top+d(8)-scroll_,
                     selected?Gdiplus::Color(255,240,248,255):Gdiplus::Color(255,157,168,185));
                for(auto &line:choice.lines)
                    text(candidates_[choice.index].text.substr(line.start,line.length),candidate,line.rect.left,line.rect.top-scroll_,Gdiplus::Color(255,247,249,253));
            }
            if(!correction_.empty()) {
                auto proof=L"基础纠错建议: "+correction_;
                for(auto &line:proofLines_)text(proof.substr(line.start,line.length),english,line.rect.left,line.rect.top-scroll_,Gdiplus::Color(255,184,213,255));
                for(auto item:{std::pair<RECT,const wchar_t*>{applyRect_,L"采用建议"},{proofCopyRect_,L"复制建议"}}) {
                    auto r=item.first;OffsetRect(&r,0,-scroll_);
                    rounded(r,d(5),Gdiplus::Color(160,67,80,105),Gdiplus::Color(160,49,64,88),Gdiplus::Color(80,145,174,213));
                    Gdiplus::StringFormat centered; centered.SetAlignment(Gdiplus::StringAlignmentCenter);centered.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                    Gdiplus::SolidBrush ink(Gdiplus::Color(255,225,238,255));
                    g.DrawString(item.second,-1,&english,Gdiplus::RectF(r.left,r.top,r.right-r.left,r.bottom-r.top),&centered,&ink);
                }
            }
            if(copyHover_)rounded(copy_,d(5),Gdiplus::Color(90,140,155,181),Gdiplus::Color(60,100,115,144),Gdiplus::Color(0,0,0,0));
            Gdiplus::Pen copyPen(copyState_==1?Gdiplus::Color(255,98,219,166):copyState_==2?Gdiplus::Color(255,255,135,128):Gdiplus::Color(255,185,197,215),std::max(1.0f,scale_/96.0f));
            g.DrawRectangle(&copyPen,Gdiplus::Rect(copy_.left+d(8),copy_.top+d(3),copy_.right-copy_.left-d(12),copy_.bottom-copy_.top-d(10)));
            g.DrawRectangle(&copyPen,Gdiplus::Rect(copy_.left+d(4),copy_.top+d(7),copy_.right-copy_.left-d(12),copy_.bottom-copy_.top-d(10)));
            if(layout_.contentHeight>height_) {
                int track=height_-d(20),thumb=std::max(d(18),track*height_/layout_.contentHeight);
                int top=d(10)+(track-thumb)*scroll_/(layout_.contentHeight-height_);
                Gdiplus::SolidBrush brush(Gdiplus::Color(150,190,202,222));g.FillRectangle(&brush,width_-d(5),top,d(2),thumb);
            }
        }
        return result;
    }
    void renderGlass(HDC dc) {
        auto rendered = glassImage(dc);
        auto &image = *rendered;
        Gdiplus::BitmapData data{};Gdiplus::Rect rect(0,0,width_,height_);
        if(image.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok)return;
        if(composition_) {
            bool drawn=composition_->draw(data.Scan0,data.Stride,width_,height_,(float)d(PanelStyle::cornerDiameter/2));
            image.UnlockBits(&data);
            if(!drawn) {
                compositionError_=composition_->error();
                composition_.reset();glass_=false;layered_=true;
                auto style=GetWindowLongPtrW(window_,GWL_EXSTYLE);
                SetWindowLongPtrW(window_,GWL_EXSTYLE,(style&~WS_EX_NOREDIRECTIONBITMAP)|WS_EX_LAYERED);
                renderGlass(dc);
            }
            return;
        }
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width_;info.bmiHeader.biHeight=-height_;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void *pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(bitmap) {
            for(int y=0;y<height_;++y)memcpy((BYTE*)pixels+y*width_*4,(BYTE*)data.Scan0+y*data.Stride,width_*4);
            HDC source=CreateCompatibleDC(dc);auto old=SelectObject(source,bitmap);
            if(layered_ && window_) {
                POINT origin{}; SIZE size{width_,height_};
                BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
                if(!UpdateLayeredWindow(window_,dc,nullptr,&size,source,&origin,0,&blend,ULW_ALPHA)) {
                    // Retain usable candidates when a host/compositor refuses
                    // per-pixel surfaces; never alter the host's window state.
                    layered_=false;
                    SetWindowLongPtrW(window_,GWL_EXSTYLE,GetWindowLongPtrW(window_,GWL_EXSTYLE)&~WS_EX_LAYERED);
                    BitBlt(dc,0,0,width_,height_,source,0,0,SRCCOPY);
                }
            } else BitBlt(dc,0,0,width_,height_,source,0,0,SRCCOPY);
            SelectObject(source,old);DeleteDC(source);DeleteObject(bitmap);
        }
        image.UnlockBits(&data);
    }
    void render(HDC dc) {
        if((glass_ || layered_) && graphicsToken_){renderGlass(dc);return;}
        RECT client{0, 0, width_, height_};
        HDC back = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width_, height_);
        auto oldBitmap = SelectObject(back, bitmap);
        HBRUSH transparent = CreateSolidBrush(RGB(38, 39, 43));
        FillRect(back, &client, transparent); DeleteObject(transparent);
        RECT card = {0, 0, width_, height_};
        surface(back, card, d(PanelStyle::cornerDiameter), RGB(49, 50, 55), RGB(37, 38, 42),
                RGB(89, 91, 99));
        // Keep content inside the solid border without repainting its pixels.
        HRGN clip = CreateRoundRectRgn(d(2), d(2), width_ - d(2) + 1, height_ - d(2) + 1, d(PanelStyle::cornerDiameter) - d(4), d(PanelStyle::cornerDiameter) - d(4));
        SelectClipRgn(back, clip); DeleteObject(clip);
        SetBkMode(back, TRANSPARENT);
        SelectObject(back, small_); SetTextColor(back, RGB(153, 156, 166));
        TextOutW(back, d(11), d(12) - scroll_, toChinese_ ? L"中" : L"EN", toChinese_ ? 1 : 2);
        SelectObject(back, font_);
        const auto &text = english_.empty() ? status_ : english_;
        size_t lo = std::min(selA_, selB_), hi = std::max(selA_, selB_);
        for (auto &line : lines_) {
            int y = line.rect.top - scroll_;
            if (line.rect.bottom - scroll_ < 0 || y >= height_) continue;
            SetTextColor(back, english_.empty() ? RGB(151, 154, 163) : RGB(244, 245, 248));
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
            auto divider = CreatePen(PS_SOLID, 1, RGB(67, 69, 77));
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
                surface(back, r, d(26), RGB(40, 139, 255), RGB(10, 111, 237), RGB(82, 165, 255));
            }
            SetTextColor(back, active ? RGB(255, 255, 255) : RGB(228, 230, 235));
            SelectObject(back, small_);
            auto number = std::to_wstring(choice.index % 5 + 1);
            TextOutW(back, r.left + d(7), r.top + d(11), number.c_str(), (int)number.size());
            SelectObject(back, candidateFont_);
            const auto &value = candidates_[choice.index].text;
            for (const auto &line : choice.lines)
                TextOutW(back, line.rect.left, line.rect.top - scroll_, value.data() + line.start, (int)line.length);
        }
        if(!correction_.empty()) {
            const auto proof=L"基础纠错建议: "+correction_;
            SelectObject(back,font_);SetTextColor(back,RGB(169,197,235));
            for(const auto &line:proofLines_) if(line.rect.bottom-scroll_>0&&line.rect.top-scroll_<height_)
                TextOutW(back,line.rect.left,line.rect.top-scroll_,proof.data()+line.start,(int)line.length);
            for(auto item:{std::pair<RECT,const wchar_t*>{applyRect_,L"采用建议"},{proofCopyRect_,L"复制建议"}}){
                auto rect=item.first;OffsetRect(&rect,0,-scroll_);
                surface(back,rect,d(8),RGB(61,70,86),RGB(50,60,77));
                DrawTextW(back,item.second,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            }
        }
        // A compact two-paper copy glyph, like the Mac panel; no wide text button.
        if (copyHover_)
            surface(back, copy_, d(8), RGB(63, 66, 75), RGB(55, 58, 66));
        COLORREF ink = copyState_ == 1 ? RGB(37, 145, 100) : copyState_ == 2 ? RGB(194, 77, 66) :
                       copyHover_ ? RGB(0, 112, 232) : RGB(174, 180, 192);
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
        SelectObject(back, oldBitmap); DeleteObject(bitmap); DeleteDC(back);
    }
    void paint() {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window_, &ps);
        render(dc);
        EndPaint(window_, &ps);
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
        case WM_DWMCOMPOSITIONCHANGED:
        case WM_SETTINGCHANGE:
            if(window_)configureBackdrop();
            InvalidateRect(window_,nullptr,FALSE);
            return 0;
        case WM_PAINT:
            paint();
            return 0;
        case WM_LBUTTONDOWN:
            if(!correction_.empty()) {
                POINT logical=point;logical.y+=scroll_;
                if(PtInRect(&applyRect_,logical)){if(applyCorrection)applyCorrection();return 0;}
                if(PtInRect(&proofCopyRect_,logical)){
                    pendingCopy_=correction_;copyAttempts_=0;attemptCopy();return 0;
                }
            }
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
    std::string displayState() const {
        PanelDpiScope dpiScope;
        RECT rect{}; GetWindowRect(window_, &rect);
        auto owner = GetWindow(window_, GW_OWNER);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor);
        DWORD cloaked = 0xffffffff;
        auto library = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Attribute = HRESULT(WINAPI *)(HWND, DWORD, PVOID, DWORD);
        auto attribute = library ? (Attribute)GetProcAddress(library, "DwmGetWindowAttribute") : nullptr;
        if (attribute) attribute(window_, 14, &cloaked, sizeof(cloaked));
        if (library) FreeLibrary(library);
        return "created="+std::to_string(IsWindow(window_))+" visible="+std::to_string(IsWindowVisible(window_))+
            " ownerExists="+std::to_string(owner!=nullptr)+" ownerVisible="+std::to_string(IsWindowVisible(owner))+
            " composition="+std::to_string(composition_!=nullptr)+" compositionError="+std::to_string((unsigned)compositionError_)+
            " compositionAtCreation="+std::to_string(compositionAtCreation_)+
            " cloaked="+std::to_string(cloaked)+" dpi="+std::to_string(scale_)+" rect="+std::to_string(rect.left)+","+
            std::to_string(rect.top)+","+std::to_string(rect.right)+","+std::to_string(rect.bottom)+" work="+
            std::to_string(monitor.rcWork.left)+","+std::to_string(monitor.rcWork.top)+","+
            std::to_string(monitor.rcWork.right)+","+std::to_string(monitor.rcWork.bottom);
    }
    std::function<void(size_t)> choose;
    std::function<void()> applyCorrection;
    void createWindow() {
        if (window_) return;
        PanelDpiScope dpiScope;
        WNDCLASSW wc = {};
        wc.lpfnWndProc = proc;
        wc.hInstance = module;
        wc.lpszClassName = L"Fuyi.TranslationPanel.1";
        wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
        // A window region, rather than colour-key transparency alone, excludes
        // the corners even in remote desktop compositors.
        // The shell must recognize this as an IME window. An ordinary tool
        // window created inside Search/AppContainer is shell-cloaked.
        wc.style = CS_IME;
        RegisterClassW(&wc);
        // The compositor must own the surface from creation. Switching this
        // style after CreateWindowEx can leave the original black HWND bitmap
        // visible outside the visual tree's rounded clip.
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST |
                        (graphicsToken_?WS_EX_NOREDIRECTIONBITMAP:0), wc.lpszClassName,
                        L"浮译", WS_POPUP, 0, 0, width_, height_, owner_, nullptr, module, this);
        if(window_)configureBackdrop();
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
    }
    Panel() {
        Gdiplus::GdiplusStartupInput startup;
        if(Gdiplus::GdiplusStartup(&graphicsToken_, &startup, nullptr)!=Gdiplus::Ok)graphicsToken_=0;
        fonts();
    }
    ~Panel() {
        composition_.reset();
        if (tooltip_) DestroyWindow(tooltip_);
        if (window_)
            DestroyWindow(window_);
        if (font_)
            DeleteObject(font_);
        if (small_)
            DeleteObject(small_);
        if (candidateFont_) DeleteObject(candidateFont_);
        if(graphicsToken_)Gdiplus::GdiplusShutdown(graphicsToken_);
    }
    void setOwner(HWND owner) {
        // Use the TSF view's exact HWND, including a hidden CoreText transport
        // window. The shell uses this association to identify the app's IME UI;
        // replacing it with a visible root loses that association.
        if (!owner) owner = GetFocus();
        if (owner == window_ || (owner && !IsWindow(owner))) owner = nullptr;
        owner_ = owner;
        if (window_) SetWindowLongPtrW(window_, GWLP_HWNDPARENT, (LONG_PTR)owner_);
    }

    void hide() {
        if(window_ && IsWindowVisible(window_))NotifyWinEvent(EVENT_OBJECT_IME_HIDE,window_,OBJID_CLIENT,CHILDID_SELF);
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
        correction_.clear();advice_.clear();proofLines_.clear();english_.clear();status_.clear();
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
              const std::vector<Candidate> &choices, size_t selected, bool above, bool toChinese = false, const std::wstring &correction=L"", const std::wstring &advice=L"") {
        PanelDpiScope dpiScope;
        createWindow();
        if (!window_)
            return;
        if (english_ != english) {
            KillTimer(window_, copyRetryTimer); KillTimer(window_, copyFeedbackTimer);
            pendingCopy_.clear(); copyFeedback(0, L"复制译文");
            selA_ = selB_ = 0;
            scroll_ = 0;
        }
        if(correction_!=correction){KillTimer(window_,copyRetryTimer);pendingCopy_.clear();}
        correction_=correction;advice_=advice;
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
        // A window region is a one-bit mask: it cuts antialiased pixels into
        // stair steps. A per-pixel surface carries the complete smooth edge.
        if(layered_ || composition_) SetWindowRgn(window_,nullptr,FALSE);
        else {
            HRGN shape = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, d(PanelStyle::cornerDiameter), d(PanelStyle::cornerDiameter));
            if (shape && !SetWindowRgn(window_, shape, FALSE)) DeleteObject(shape);
        }
        SetWindowPos(window_, HWND_TOPMOST, r->left, r->top, width_, height_, SWP_NOACTIVATE);
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        bool wasVisible=IsWindowVisible(window_)!=FALSE;
        ShowWindow(window_, SW_SHOWNOACTIVATE);
        NotifyWinEvent(wasVisible?EVENT_OBJECT_IME_CHANGE:EVENT_OBJECT_IME_SHOW,window_,OBJID_CLIENT,CHILDID_SELF);
    }
};
} // namespace fuyi
