#include "Core.h"
#include "DisplayAttribute.h"
#include "Ids.h"
#include "Panel.h"
#include "Pinyin.h"
#include "Translator.h"
#include "TsfCompat.h"
#include "TsfMenuCompat.h"
#include "WinUtil.h"
#include <inputscope.h>
namespace fuyi {
HINSTANCE module = nullptr;
std::atomic<LONG> objects{0};
class TextService;
class EditSession : public ITfEditSession {
    LONG refs_ = 1;
    IUnknown *owner_;
    ITfContext *context_;
    std::function<HRESULT(TfEditCookie)> action_;

  public:
    EditSession(IUnknown *owner, ITfContext *context, std::function<HRESULT(TfEditCookie)> action)
        : owner_(owner), context_(context), action_(std::move(action)) {
        owner_->AddRef();
        context_->AddRef();
    }
    ~EditSession() {
        context_->Release();
        owner_->Release();
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id == IID_IUnknown || id == IID_ITfEditSession) {
            *p = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs_);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override {
        try {
            return action_(cookie);
        } catch (...) {
            return E_FAIL;
        }
    }
};
class LanguageButton : public ITfLangBarItemButton, public ITfSource {
    ObjectLifetime lifetime_;
    LONG refs_ = 1;
    TF_LANGBARITEMINFO info_{};
    ITfLangBarItemSink *sink_ = nullptr;

  public:
    std::function<Options()> read;
    std::function<void(UINT)> command;
    LanguageButton() {
        info_.clsidService = serviceId;
        info_.guidItem = barId;
        info_.dwStyle = TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
        wcscpy_s(info_.szDescription, L"浮译");
    }
    ~LanguageButton() {
        release(sink_);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id == IID_IUnknown || id == IID_ITfLangBarItem || id == IID_ITfLangBarItemButton)
            *p = (ITfLangBarItemButton *)this;
        else if (id == IID_ITfSource)
            *p = (ITfSource *)this;
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs_);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE GetInfo(TF_LANGBARITEMINFO *p) override {
        if (!p)
            return E_POINTER;
        *p = info_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Show(BOOL) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTooltipString(BSTR *s) override {
        if (!s)
            return E_POINTER;
        *s = SysAllocString(L"浮译 · Shift 切换中英，菜单打开设置");
        return *s ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick click, POINT, const RECT *) override {
        if (click == TF_LBI_CLK_LEFT && command)
            command(1);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu *menu) override {
        auto o = read();
        menu->AddMenuItem(1, o.direct ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr, L"English 英译中", 11,
                          nullptr);
        menu->AddMenuItem(2, o.english ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr, L"拼音默认输出英文", 8,
                          nullptr);
        menu->AddMenuItem(5, o.chinese ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr, L"英文默认输出中文", 8, nullptr);
        menu->AddMenuItem(3, o.privacy ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr, L"隐私模式", 4, nullptr);
        menu->AddMenuItem(4, 0, nullptr, nullptr, L"浮译设置", 4, nullptr);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT id) override {
        if (command)
            command(id);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetIcon(HICON *icon) override {
        if (!icon)
            return E_POINTER;
        *icon = (HICON)LoadImageW(module, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        return *icon ? S_OK : E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE GetText(BSTR *s) override {
        if (!s)
            return E_POINTER;
        auto o = read();
        *s = SysAllocString(o.privacy ? L"隐私" : o.direct ? L"EN" : L"译");
        return *s ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id, IUnknown *sink, DWORD *cookie) override {
        if (!cookie || !sink)
            return E_POINTER;
        if (id != IID_ITfLangBarItemSink)
            return CONNECT_E_CANNOTCONNECT;
        if (sink_)
            return CONNECT_E_ADVISELIMIT;
        auto hr = sink->QueryInterface(IID_ITfLangBarItemSink, (void **)&sink_);
        if (SUCCEEDED(hr))
            *cookie = 1;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override {
        if (cookie != 1 || !sink_)
            return CONNECT_E_NOCONNECTION;
        release(sink_);
        return S_OK;
    }
    void update() {
        if (sink_)
            sink_->OnUpdate(TF_LBI_TEXT | TF_LBI_ICON);
    }
};
class TextService : public ITfTextInputProcessorEx,
                    public ITfKeyEventSink,
                    public ITfThreadMgrEventSink,
                    public ITfThreadFocusSink,
                    public ITfCompositionSink,
                    public ITfTextEditSink,
                    public ITfTextLayoutSink,
                    public ITfDisplayAttributeProvider {
    LONG refs_ = 1;
    ITfThreadMgr *manager_ = nullptr;
    TfClientId client_ = TF_CLIENTID_NULL;
    ITfContext *context_ = nullptr;
    ITfComposition *composition_ = nullptr;
    ITfRange *sentenceRange_ = nullptr;
    ITfSource *managerSource_ = nullptr;
    ITfSource *contextSource_ = nullptr;
    DWORD managerCookie_ = TF_INVALID_COOKIE, focusCookie_ = TF_INVALID_COOKIE,
          editCookie_ = TF_INVALID_COOKIE, layoutCookie_ = TF_INVALID_COOKIE;
    TfGuidAtom atom_ = TF_INVALID_GUIDATOM;
    LanguageButton *language_ = nullptr;
    ITfLangBarItemMgr *bar_ = nullptr;
    HWND messages_ = nullptr;
    std::unique_ptr<Panel> panel_;
    std::shared_ptr<TranslationTarget> target_ = std::make_shared<TranslationTarget>();
    Options options_, diskAtOverride_;
    std::optional<Options> unsavedOptions_;
    static bool sameOptions(const Options &a, const Options &b) {
        return a.english == b.english && a.above == b.above && a.privacy == b.privacy && a.direct == b.direct && a.remember == b.remember && a.chinese == b.chinese;
    }
    void storeOptions() {
        try { options_.save(); unsavedOptions_.reset(); }
        catch (...) {
            // AppContainer hosts have read-only shared preferences. Keep Shift
            // and menu changes in this service instead of resetting at 150 ms.
            diskAtOverride_ = Options::read();
            unsavedOptions_ = options_;
        }
    }
    ShiftTap shift_;
    Sentence sentence_;
    size_t insertionOffset_ = 0;
    std::string raw_;
    std::vector<std::pair<std::string, std::wstring>> selectionUndo_;
    std::wstring locked_, english_, translatedSource_, requestedSource_, status_, correction_, advice_;
    std::vector<Candidate> candidates_;
    size_t selected_ = 0;
    RECT anchor_{};
    bool haveAnchor_ = false, active_ = false, secure_ = false, closingComposition_ = false,
         expectEdit_ = false, layoutPending_ = false, editFailed_ = false, wroteText_ = false;
    uint64_t revision_ = 0;
    DWORD flags_ = 0;
    unsigned ownEditDepth_ = 0;
    static LRESULT CALLBACK proc(HWND w, UINT message, WPARAM a, LPARAM b) {
        auto self = (TextService *)GetWindowLongPtrW(w, GWLP_USERDATA);
        if (message == WM_NCCREATE) {
            self = (TextService *)((CREATESTRUCTW *)b)->lpCreateParams;
            SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)self);
        }
        if (self) {
            if (message == WM_TRANSLATED) {
                std::unique_ptr<TranslationResult> r((TranslationResult *)b);
                if (self->active_ && r->token == self->target_->token && r->ticket == self->revision_ &&
                    r->source == self->requestedSource_ && !self->secure_ && !self->options_.privacy &&
                    r->toChinese == self->options_.direct) {
                    self->english_ = r->text;
                    self->translatedSource_ = r->source;
                    self->status_ = r->status;
                    self->correction_ = r->correction; self->advice_ = r->advice;
                    self->draw();
                }
                return 0;
            }
            if (message == WM_TIMER) {
                self->tick();
                return 0;
            }
        }
        return DefWindowProcW(w, message, a, b);
    }
    void tick() {
        if (!active_)
            return;
        auto updated = Options::read();
        if (unsavedOptions_) {
            if (sameOptions(updated, diskAtOverride_)) {
                bool privacy = updated.privacy;
                updated = *unsavedOptions_;
                updated.privacy = updated.privacy || privacy; // Fail closed on inaccessible settings.
            } else unsavedOptions_.reset();
        }
        bool changed = updated.privacy != options_.privacy || updated.direct != options_.direct ||
                       updated.english != options_.english || updated.chinese != options_.chinese;
        options_.above = updated.above;
        options_.remember = updated.remember;
        if (changed) {
            finishComposition();
            clear();
            options_ = updated;
            if (language_)
                language_->update();
        }
        if (!panel_->selecting() && sentence_.expired(GetTickCount64()))
            clear();
        if (options_.privacy) {
            clear();
            secure_ = true;
        } else if (secure_ && context_)
            request(
                [this](TfEditCookie ec) {
                    secure_ = sensitive(ec);
                    return S_OK;
                },
                true, true);
    }
    IUnknown *unknown() {
        return (ITfTextInputProcessorEx *)this;
    }
    HRESULT request(std::function<HRESULT(TfEditCookie)> action, bool readOnly = false, bool async = false) {
        if (!context_ || !active_)
            return E_FAIL;
        ITfContext *expected = context_;
        auto guarded = [this, expected, action = std::move(action)](TfEditCookie ec) {
            if (context_ != expected || !active_)
                return E_ABORT;
            struct Depth {
                unsigned &value;
                explicit Depth(unsigned &v) : value(v) { ++value; }
                ~Depth() { --value; }
            } depth(ownEditDepth_);
            return action(ec);
        };
        auto session = new EditSession(unknown(), context_, std::move(guarded));
        HRESULT result = E_FAIL;
        DWORD access = readOnly ? TF_ES_READ : TF_ES_READWRITE;
        HRESULT hr = context_->RequestEditSession(
            client_, session, (async ? TF_ES_ASYNCDONTCARE : TF_ES_SYNC) | access, &result);
        session->Release();
        return FAILED(hr) ? hr : result;
    }
    // Finish the old context independently of later focus changes. Its marked text
    // already contains exactly the literal characters; never read surrounding text.
    void finishComposition() {
        if (!composition_ || !context_)
            return;
        auto old =
            std::shared_ptr<ITfComposition>(composition_, [](ITfComposition *value) { value->Release(); });
        composition_ = nullptr;
        auto session =
            new EditSession(unknown(), context_, [old](TfEditCookie ec) { return old->EndComposition(ec); });
        HRESULT result = E_FAIL;
        context_->RequestEditSession(client_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &result);
        session->Release();
    }
    void cancel() {
        ++revision_;
        target_->generation = revision_;
        english_.clear();correction_.clear();advice_.clear();
        translatedSource_.clear();
        requestedSource_.clear();
    }
    void clear() {
        finishComposition();
        cancel();
        raw_.clear();
        locked_.clear(); selectionUndo_.clear();
        candidates_.clear();
        selected_ = 0;
        sentence_.clear();
        insertionOffset_ = 0;
        release(sentenceRange_);
        status_.clear();
        if (panel_)
            panel_->hide();
        shift_.reset();
    }
    void detach() {
        if (contextSource_) {
            if (editCookie_ != TF_INVALID_COOKIE)
                contextSource_->UnadviseSink(editCookie_);
            if (layoutCookie_ != TF_INVALID_COOKIE)
                contextSource_->UnadviseSink(layoutCookie_);
        }
        editCookie_ = layoutCookie_ = TF_INVALID_COOKIE;
        release(contextSource_);
        release(composition_);
        release(context_);
        clear();
        haveAnchor_ = false;
        layoutPending_ = false;
        expectEdit_ = false;
    }
    void attach(ITfContext *context) {
        if (context == context_)
            return;
        finishComposition();
        detach();
        context_ = context;
        if (context_)
            context_->AddRef();
        if (context_ && SUCCEEDED(context_->QueryInterface(IID_ITfSource, (void **)&contextSource_))) {
            contextSource_->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink *)this, &editCookie_);
            contextSource_->AdviseSink(IID_ITfTextLayoutSink, (ITfTextLayoutSink *)this, &layoutCookie_);
        }
        if (context_)
            request(
                [this](TfEditCookie ec) {
                    secure_ = sensitive(ec);
                    return S_OK;
                },
                true, true);
    }
    bool compartment(REFGUID id) {
        ITfCompartmentMgr *mgr = nullptr;
        ITfCompartment *value = nullptr;
        VARIANT v;
        VariantInit(&v);
        bool blocked = false;
        if (context_ && SUCCEEDED(context_->QueryInterface(IID_ITfCompartmentMgr, (void **)&mgr))) {
            if (SUCCEEDED(mgr->GetCompartment(id, &value))) {
                if (SUCCEEDED(value->GetValue(&v)) && v.vt == VT_I4)
                    blocked = v.lVal != 0;
                VariantClear(&v);
                release(value);
            }
            release(mgr);
        }
        return blocked;
    }
    bool sensitive(TfEditCookie ec) {
        if (options_.privacy || (flags_ & TF_TMAE_SECUREMODE) ||
            compartment(GUID_COMPARTMENT_KEYBOARD_DISABLED))
            return true;
        GUITHREADINFO gui = {sizeof(gui)};
        if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndFocus) {
            wchar_t cls[80];
            GetClassNameW(gui.hwndFocus, cls, 80);
            if ((_wcsicmp(cls, L"Edit") == 0 || wcsstr(cls, L"RichEdit")) &&
                (GetWindowLongPtrW(gui.hwndFocus, GWL_STYLE) & ES_PASSWORD))
                return true;
        }
        TF_SELECTION sel{};
        ULONG fetched = 0;
        ITfReadOnlyProperty *property = nullptr;
        bool blocked = false;
        if (SUCCEEDED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched) {
            if (SUCCEEDED(context_->GetAppProperty(GUID_PROP_INPUTSCOPE, &property)) && property) {
                VARIANT v;
                VariantInit(&v);
                if (SUCCEEDED(property->GetValue(ec, sel.range, &v)) && v.vt == VT_UNKNOWN && v.punkVal) {
                    ITfInputScope *input = nullptr;
                    if (SUCCEEDED(v.punkVal->QueryInterface(IID_ITfInputScope, (void **)&input))) {
                        InputScope *scopes = nullptr;
                        UINT count = 0;
                        if (SUCCEEDED(input->GetInputScopes(&scopes, &count))) {
                            for (UINT i = 0; i < count; i++)
                                if (scopes[i] == IS_PASSWORD || scopes[i] == IS_PRIVATE ||
                                    scopes[i] == IS_NUMERIC_PASSWORD || scopes[i] == IS_NUMERIC_PIN)
                                    blocked = true;
                            CoTaskMemFree(scopes);
                        }
                        release(input);
                    }
                }
                VariantClear(&v);
                release(property);
            }
            sel.range->Release();
        }
        return blocked;
    }
    bool composing() const {
        return !raw_.empty() || !locked_.empty();
    }
    RangeEvidence selectionEvidence(TfEditCookie ec, ITfRange *range) {
        if (!range) return RangeEvidence::Unknown;
        BOOL invalidated = FALSE;
        if (range == sentenceRange_ && !sentence_.text.empty() &&
            SUCCEEDED(range->IsEmpty(ec, &invalidated)) && invalidated)
            return RangeEvidence::Unknown;
        TF_SELECTION sel{};
        ULONG count = 0;
        if (FAILED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &count)) || !count || !sel.range)
            return RangeEvidence::Unknown;
        BOOL empty = FALSE;
        LONG diff = 1;
        auto evidence = RangeEvidence::Unknown;
        if (SUCCEEDED(sel.range->IsEmpty(ec, &empty))) {
            if (!empty) evidence = RangeEvidence::Different;
            else if (SUCCEEDED(range->CompareEnd(ec, sel.range, TF_ANCHOR_START, &diff)))
                evidence = diff == 0 ? RangeEvidence::Matches : RangeEvidence::Different;
        }
        sel.range->Release();
        return evidence;
    }
    bool selectionAtEnd(TfEditCookie ec, ITfRange *range) {
        return selectionEvidence(ec, range) == RangeEvidence::Matches;
    }
    RangeEvidence rangeTextEvidence(TfEditCookie ec, ITfRange *range, const std::wstring &expected) {
        if (!range) return RangeEvidence::Unknown;
        std::vector<wchar_t> buffer(expected.size() + 1);
        ULONG count = 0;
        if (FAILED(range->GetText(ec, 0, buffer.data(), (ULONG)buffer.size(), &count)))
            return RangeEvidence::Unknown;
        // Some rich-edit bridges invalidate their ranges at EndComposition. This
        // disables replacement, not the memory-only preview of our own sentence.
        if (!count && !expected.empty()) return RangeEvidence::Unknown;
        return std::wstring(buffer.data(), count) == expected ? RangeEvidence::Matches : RangeEvidence::Different;
    }
    RangeEvidence prefixEvidence(TfEditCookie ec) {
        if (sentence_.text.empty()) return RangeEvidence::Matches;
        if (!sentenceRange_) return RangeEvidence::Unknown;
        ITfRange *prefix = nullptr, *current = nullptr;
        if (FAILED(sentenceRange_->Clone(&prefix))) return RangeEvidence::Unknown;
        BOOL empty = FALSE;
        if (SUCCEEDED(prefix->IsEmpty(ec, &empty)) && empty) {
            release(prefix); return RangeEvidence::Unknown;
        }
        if (composition_ && SUCCEEDED(composition_->GetRange(&current))) {
            if (insertionOffset_ < sentence_.text.size()) {
                auto before = prefix->ShiftEndToRange(ec, current, TF_ANCHOR_START);
                auto result = SUCCEEDED(before) ? rangeTextEvidence(ec, prefix, sentence_.text.substr(0, insertionOffset_)) : RangeEvidence::Unknown;
                ITfRange *tail = nullptr;
                if (result == RangeEvidence::Matches && SUCCEEDED(sentenceRange_->Clone(&tail))) {
                    result = SUCCEEDED(tail->ShiftStartToRange(ec, current, TF_ANCHOR_END))
                        ? rangeTextEvidence(ec, tail, sentence_.text.substr(insertionOffset_)) : RangeEvidence::Unknown;
                } else if (result == RangeEvidence::Matches) result = RangeEvidence::Unknown;
                release(tail); release(current); release(prefix);
                return result;
            }
            LONG diff = 0;
            if (FAILED(prefix->CompareEnd(ec, current, TF_ANCHOR_START, &diff))) {
                release(current); release(prefix); return RangeEvidence::Unknown;
            }
            if (diff < 0) {
                release(current); release(prefix); return RangeEvidence::Different;
            }
            // Only trim an overlapping owned range; never extend it to read
            // unrelated document text between an old range and a moved caret.
            if (diff > 0 && FAILED(prefix->ShiftEndToRange(ec, current, TF_ANCHOR_START))) {
                release(current); release(prefix); return RangeEvidence::Unknown;
            }
        }
        release(current);
        auto result = rangeTextEvidence(ec, prefix, sentence_.text);
        release(prefix);
        return result;
    }
    // Re-read only the range this service committed in this input context.
    // A caret outside it or a selection ends tracking; unavailable ranges only
    // disable replacement until the host provides reliable evidence again.
    RangeEvidence syncSentence(TfEditCookie ec) {
        if (composition_ || !sentenceRange_) return RangeEvidence::Unknown;
        TF_SELECTION sel{}; ULONG count = 0;
        if (FAILED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &count)) || !count || !sel.range)
            return RangeEvidence::Unknown;
        BOOL empty = FALSE; LONG start = 1, end = -1;
        if (FAILED(sel.range->IsEmpty(ec, &empty)) ||
            FAILED(sel.range->CompareStart(ec, sentenceRange_, TF_ANCHOR_START, &start)) ||
            FAILED(sel.range->CompareEnd(ec, sentenceRange_, TF_ANCHOR_END, &end))) {
            sel.range->Release(); return RangeEvidence::Unknown;
        }
        if (!empty || start < 0 || end > 0) {
            sel.range->Release(); return RangeEvidence::Different;
        }
        ITfRange *prefix = nullptr;
        wchar_t text[1001], left[1001]; ULONG length = 0, offset = 0;
        bool valid = SUCCEEDED(sentenceRange_->GetText(ec, 0, text, 1001, &length)) &&
            SUCCEEDED(sentenceRange_->Clone(&prefix)) &&
            SUCCEEDED(prefix->ShiftEndToRange(ec, sel.range, TF_ANCHOR_START)) &&
            SUCCEEDED(prefix->GetText(ec, 0, left, 1001, &offset)) && offset <= length;
        release(prefix); sel.range->Release();
        if (!valid) return RangeEvidence::Unknown;
        if (length > 1000 || std::wstring(text, length).find_first_of(L"\r\n") != std::wstring::npos)
            return RangeEvidence::Different;
        bool finished = sentence_.finished && offset == length && sentence_.text == std::wstring(text, length);
        sentence_.text.assign(text, length);
        insertionOffset_ = offset;
        sentence_.touched = GetTickCount64();
        sentence_.finished = finished;
        return RangeEvidence::Matches;
    }
    HRESULT ensureComposition(TfEditCookie ec) {
        if (composition_)
            return S_OK;
        ITfInsertAtSelection *insert = nullptr;
        ITfRange *range = nullptr;
        HRESULT hr = context_->QueryInterface(IID_ITfInsertAtSelection, (void **)&insert);
        if (SUCCEEDED(hr))
            hr = insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &range);
        release(insert);
        ITfContextComposition *manager = nullptr;
        if (SUCCEEDED(hr))
            hr = context_->QueryInterface(IID_ITfContextComposition, (void **)&manager);
        if (SUCCEEDED(hr))
            hr = manager->StartComposition(ec, range, (ITfCompositionSink *)this, &composition_);
        release(manager);
        release(range);
        return hr;
    }
    void selectEnd(TfEditCookie ec, ITfRange *range) {
        ITfRange *caret = nullptr;
        if (FAILED(range->Clone(&caret))) {
            editFailed_ = true;
            return;
        }
        if (FAILED(caret->Collapse(ec, TF_ANCHOR_END))) {
            caret->Release();
            editFailed_ = true;
            return;
        }
        TF_SELECTION selection = {caret, {TF_AE_NONE, FALSE}};
        if (FAILED(context_->SetSelection(ec, 1, &selection)))
            editFailed_ = true;
        caret->Release();
    }
    HRESULT marked(TfEditCookie ec) {
        auto hr = ensureComposition(ec);
        if (FAILED(hr)) {
            editFailed_ = true;
            return hr;
        }
        ITfRange *range = nullptr;
        if (FAILED(composition_->GetRange(&range))) {
            editFailed_ = true;
            return E_FAIL;
        }
        std::wstring value = locked_ + wide(raw_);
        hr = range->SetText(ec, 0, value.c_str(), (LONG)value.size());
        if (FAILED(hr))
            editFailed_ = true;
        if (SUCCEEDED(hr)) {
            wroteText_ = true;
            expectEdit_ = true;
            ITfProperty *property = nullptr;
            if (atom_ != TF_INVALID_GUIDATOM &&
                SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE, &property))) {
                VARIANT v;
                VariantInit(&v);
                v.vt = VT_I4;
                v.lVal = (LONG)atom_;
                property->SetValue(ec, range, &v);
                release(property);
            }
            selectEnd(ec, range);
        }
        range->Release();
        return hr;
    }
    ITfRange *commitText(TfEditCookie ec, const std::wstring &value) {
        if (FAILED(ensureComposition(ec))) {
            editFailed_ = true;
            return nullptr;
        }
        ITfRange *range = nullptr;
        if (FAILED(composition_->GetRange(&range))) {
            editFailed_ = true;
            return nullptr;
        }
        if (FAILED(range->SetText(ec, 0, value.c_str(), (LONG)value.size()))) {
            editFailed_ = true;
            range->Release();
            return nullptr;
        }
        wroteText_ = true;
        expectEdit_ = true;
        auto composition = composition_;
        composition_ = nullptr;
        closingComposition_ = true;
        composition->EndComposition(ec);
        closingComposition_ = false;
        composition->Release();
        selectEnd(ec, range);
        return range;
    }
    void literal(TfEditCookie ec) {
        if (composing()) {
            auto range = commitText(ec, locked_ + wide(raw_));
            release(range);
        }
        raw_.clear();
        locked_.clear(); selectionUndo_.clear();
        candidates_.clear();
    }
    std::optional<std::wstring> fragment() {
        if (raw_.empty())
            return locked_;
        if (selected_ >= candidates_.size())
            return {};
        auto chosen = candidates_[selected_];
        pinyin().resolve(raw_, chosen);
        if (!chosen.consumed) return {};
        candidates_[selected_] = chosen;
        auto tail = pinyin().preview(raw_.substr(chosen.consumed));
        if (!tail)
            return {};
        return locked_ + chosen.text + *tail;
    }
    std::wstring source() {
        auto part = fragment();
        return insertedSentence(sentence_.text, insertionOffset_, part ? *part : L"");
    }
    void translate() {
        auto value = source();
        if (value == requestedSource_ && !value.empty())
            return;
        cancel();
        requestedSource_ = value;
        if (value.empty()) {
            status_ = options_.direct ? L"输入英文后显示中文翻译" : L"补全拼音或选择中文后显示翻译";
            return;
        }
        status_ = L"正在翻译整句…";
        worker().submit(target_, revision_, value, options_.direct);
    }
    void geometry(TfEditCookie ec) {
        ITfContextView *view = nullptr;
        ITfRange *range = nullptr;
        if (composition_)
            composition_->GetRange(&range);
        else {
            TF_SELECTION sel{};
            ULONG count = 0;
            if (SUCCEEDED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &count)) && count)
                range = sel.range;
        }
        RECT rect{};
        BOOL clipped = FALSE;
        bool valid = false;
        if (range && SUCCEEDED(context_->GetActiveView(&view))) {
            HWND owner=nullptr;
            if(FAILED(view->GetWnd(&owner)) || !owner)owner=GetFocus();
            if(panel_)panel_->setOwner(owner);
            range->Collapse(ec, TF_ANCHOR_END);
            valid = SUCCEEDED(view->GetTextExt(ec, range, &rect, &clipped)) && !clipped &&
                    rect.bottom > rect.top && rect.bottom - rect.top < 500;
            release(view);
        }
        release(range);
        if (!valid) {
            GUITHREADINFO gui = {sizeof(gui)};
            if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndCaret) {
                POINT p = {gui.rcCaret.left, gui.rcCaret.top};
                if (ClientToScreen(gui.hwndCaret, &p) && gui.rcCaret.bottom > gui.rcCaret.top) {
                    rect = {p.x, p.y, p.x + std::max(1L, gui.rcCaret.right - gui.rcCaret.left),
                            p.y + gui.rcCaret.bottom - gui.rcCaret.top};
                    valid = true;
                }
            }
        }
        if (!valid && view == nullptr) {
            // Search/CoreText can return TS_E_NOLAYOUT while composition is
            // changing and expose no Win32 caret. Use the active TSF view's
            // screen extent until its next layout notification supplies a caret.
            ITfContextView *fallback = nullptr;
            if (SUCCEEDED(context_->GetActiveView(&fallback))) {
                RECT extent{};
                if (SUCCEEDED(fallback->GetScreenExt(&extent)) &&
                    extent.right > extent.left && extent.bottom > extent.top &&
                    extent.bottom - extent.top <= 160) {
                    rect = {extent.left, extent.top, extent.left + 1, extent.bottom};
                    valid = true;
                }
                release(fallback);
            }
        }
        if (valid) {
            anchor_ = rect;
            haveAnchor_ = true;
        }
        draw();
    }
    void draw() {
        if (!panel_ || !active_ || secure_ || options_.privacy ||
            (!composing() && sentence_.text.empty()) || !haveAnchor_) {
            if (panel_)
                panel_->hide();
            return;
        }
        panel_->show(anchor_, english_, status_, locked_ + wide(raw_), candidates_, selected_,
                     options_.above, options_.direct, correction_, advice_);
    }
    HRESULT refresh(TfEditCookie ec) {
        candidates_ = pinyin().query(raw_, true);
        if (selected_ >= candidates_.size())
            selected_ = 0;
        auto hr = marked(ec);
        if (FAILED(hr))
            return hr;
        translate();
        geometry(ec);
        return S_OK;
    }
    void chinese(TfEditCookie ec, const std::wstring &suffix = L"") {
        auto part = fragment();
        auto text = (part ? *part : locked_ + wide(raw_)) + suffix;
        if (text.empty())
            return;
        if (prefixEvidence(ec) == RangeEvidence::Different) {
            sentence_.clear(); release(sentenceRange_);
        }
        auto range = commitText(ec, text);
        if (!range) {
            status_ = L"当前应用暂不接受输入";
            draw();
            return;
        }
        if (!raw_.empty() && selected_ < candidates_.size())
            pinyin().remember(raw_, candidates_[selected_]);
        // TSF anchors move when text is inserted at their boundary. A failed
        // post-commit comparison must not discard already committed words.
        bool hadPrefix = !sentence_.text.empty();
        if (sentenceRange_) {
            LONG diff = 0;
            if (insertionOffset_ < sentence_.text.size()) {
                // Inserting inside the owned sentence already expands its range.
                if (insertionOffset_ == 0 && FAILED(sentenceRange_->ShiftStartToRange(ec, range, TF_ANCHOR_START)))
                    release(sentenceRange_);
            } else if (SUCCEEDED(sentenceRange_->CompareEnd(ec, range, TF_ANCHOR_START, &diff)) && diff == 0) {
                if (FAILED(sentenceRange_->ShiftEndToRange(ec, range, TF_ANCHOR_END)))
                    release(sentenceRange_);
            } else release(sentenceRange_);
        }
        if (!sentenceRange_ && !hadPrefix)
            range->Clone(&sentenceRange_);
        if (sentenceRange_)
            sentenceRange_->SetGravity(ec, TF_GRAVITY_FORWARD, TF_GRAVITY_BACKWARD);
        range->Release();
        auto combined = insertedSentence(sentence_.text, insertionOffset_, text);
        insertionOffset_ += text.size();
        sentence_.clear(); sentence_.append(combined, GetTickCount64());
        raw_.clear();
        locked_.clear(); selectionUndo_.clear();
        candidates_.clear();
        selected_ = 0;
        translate();
        geometry(ec);
    }
    void commit(TfEditCookie ec, bool english, bool keepEnglishSource=false) {
        if (!english) {
            chinese(ec);
            return;
        }
        auto part = fragment();
        auto full = source();
        auto output = english_;
        if (!part || full != translatedSource_ || english_.empty()) {
            status_ = part ? L"整句译文尚未就绪，请稍候" : L"请先完成输入";
            draw();
            return;
        }
        if (!sentence_.text.empty()) {
            ITfRange *compositionRange = nullptr;
            ITfRange *span = nullptr;
            LONG diff = 1;
            if (prefixEvidence(ec) != RangeEvidence::Matches || !sentenceRange_ || !composition_ ||
                FAILED(composition_->GetRange(&compositionRange)) ||
                (insertionOffset_ >= sentence_.text.size() &&
                 (FAILED(sentenceRange_->CompareEnd(ec, compositionRange, TF_ANCHOR_START, &diff)) || diff)) ||
                !selectionAtEnd(ec, compositionRange) || FAILED(sentenceRange_->Clone(&span))) {
                release(compositionRange);
                release(span);
                status_ = L"无法安全替换整句，请复制译文或纠错建议";
                draw();
                return;
            }
            if ((insertionOffset_ >= sentence_.text.size() && FAILED(span->ShiftEndToRange(ec, compositionRange, TF_ANCHOR_END))) ||
                (insertionOffset_ == 0 && FAILED(span->ShiftStartToRange(ec, compositionRange, TF_ANCHOR_START)))) {
                release(compositionRange);
                release(span);
                status_ = L"无法安全替换整句，请复制译文或纠错建议";
                draw();
                return;
            }
            release(compositionRange);
            if (SUCCEEDED(span->SetText(ec, 0, english_.c_str(), (LONG)english_.size()))) {
                wroteText_ = true;
                expectEdit_ = true;
                auto composition = composition_;
                composition_ = nullptr;
                closingComposition_ = true;
                composition->EndComposition(ec);
                closingComposition_ = false;
                composition->Release();
                selectEnd(ec, span);
                clear();
                if(keepEnglishSource){sentence_.append(output,GetTickCount64());insertionOffset_=output.size();span->Clone(&sentenceRange_);if(sentenceRange_)sentenceRange_->SetGravity(ec,TF_GRAVITY_FORWARD,TF_GRAVITY_BACKWARD);translate();geometry(ec);}
            }
            span->Release();
        } else {
            auto range = commitText(ec, english_);
            if (range) {
                clear();
                if(keepEnglishSource){sentence_.append(output,GetTickCount64());insertionOffset_=output.size();range->Clone(&sentenceRange_);if(sentenceRange_)sentenceRange_->SetGravity(ec,TF_GRAVITY_FORWARD,TF_GRAVITY_BACKWARD);translate();geometry(ec);}
                range->Release();
            }
        }
    }
    void choose(TfEditCookie ec, size_t index) {
        if (index >= candidates_.size())
            return;
        auto chosen = candidates_[index];
        pinyin().resolve(raw_, chosen);
        if (!chosen.consumed) return;
        pinyin().remember(raw_, chosen);
        selectionUndo_.push_back({raw_.substr(0, chosen.consumed), locked_});
        locked_ += chosen.text;
        raw_.erase(0, chosen.consumed);
        selected_ = 0;
        if (raw_.empty() && !options_.english)
            chinese(ec);
        else
            refresh(ec);
    }
    static bool printableEnglishKey(WPARAM key) {
        return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == VK_SPACE ||
               (key >= VK_OEM_1 && key <= VK_OEM_3) || (key >= VK_OEM_4 && key <= VK_OEM_8) ||
               (key >= VK_NUMPAD0 && key <= VK_DIVIDE);
    }
    bool englishKey(TfEditCookie ec, WPARAM code) {
        if (!composing() && !sentence_.text.empty() && syncSentence(ec) == RangeEvidence::Different) clear();
        if (!composing() && sentence_.finished && insertionOffset_ >= sentence_.text.size() && printableEnglishKey(code)) clear();
        if (code == VK_ESCAPE) {
            if (!composing()) return false;
            literal(ec); clear(); return true; // Cancel translation, keep typed English.
        }
        if (code == VK_RETURN || code == VK_TAB) {
            if (!composing()) { clear(); return false; }
            bool translated = translationOnCommit(true, options_.english, options_.chinese, code == VK_TAB);
            commit(ec, translated);
            // Committing the English source ends the marked span, not the
            // sentence: later words must still translate with this prefix.
            return true;
        }
        if (code == VK_BACK) {
            if (locked_.empty()) {
                // Let the host delete once; OnEndEdit refreshes our owned range.
                return false;
            }
            eraseLast(locked_);
            if (locked_.empty()) {
                auto range = commitText(ec,L"");release(range);
                if(sentence_.text.empty())clear();
                else {translate();geometry(ec);}
            } else { marked(ec); translate(); geometry(ec); }
            return true;
        }
        if (!printableEnglishKey(code)) {
            if (code == VK_CAPITAL) return false;
            if (!composing() && code >= VK_LEFT && code <= VK_DOWN) return false;
            if (!composing() && (code == VK_HOME || code == VK_END || code == VK_DELETE)) return false;
            literal(ec); clear(); return false;
        }
        BYTE state[256]{}; wchar_t text[8]{};
        auto keyboard = GetKeyboardLayout(0);
        if (!GetKeyboardState(state)) { literal(ec); clear(); return false; }
        int count = ToUnicodeEx((UINT)code, MapVirtualKeyExW((UINT)code, MAPVK_VK_TO_VSC, keyboard),
                                state, text, 8, 4, keyboard);
        if (count <= 0 || count > 8 || text[0] < L' ') {
            literal(ec); clear(); return false;
        }
        if (locked_.size() + count > 1000) { literal(ec); clear(); return false; }
        // Spaces belong to the same English sentence, never translate each word separately.
        locked_.append(text, count);
        candidates_.clear(); selected_ = 0;
        sentence_.touched = GetTickCount64();
        marked(ec); translate(); geometry(ec);
        return true;
    }
    bool shortcut(WPARAM key) const {
        return (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000) ||
               (GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000);
    }
    bool canHandle(WPARAM key) {
        if (secure_ || options_.privacy || (flags_ & TF_TMAE_SECUREMODE))
            return false;
        if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)
            return true;
        if (shortcut(key))
            return composing() || !sentence_.text.empty() ||
                   (key == 'C' && panel_ && !panel_->selectedText().empty());
        if (options_.direct)
            return composing() || !sentence_.text.empty() || shift_.armed || (key != VK_SPACE && printableEnglishKey(key));
        if (GetKeyState(VK_CAPITAL) & 1)
            return composing() || !sentence_.text.empty() || shift_.armed;
        if (shift_.armed)
            return true;
        if ((key >= 'A' && key <= 'Z') && !(GetKeyState(VK_SHIFT) & 0x8000))
            { try { return pinyin().ready() || pinyin().start(); } catch (...) { return false; } }
        return composing() || !sentence_.text.empty();
    }
    bool key(TfEditCookie ec, WPARAM code) {
        secure_ = sensitive(ec);
        if (secure_) {
            finishComposition();
            clear();
            return false;
        }
        if (code == VK_SHIFT || code == VK_LSHIFT || code == VK_RSHIFT) {
            shift_.down(GetTickCount64(), shortcut(code));
            return false;
        }
        shift_.key();
        if (shortcut(code)) {
            if (code == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) && !panel_->selectedText().empty()) {
                panel_->copy();
                return true;
            }
            literal(ec);
            clear();
            return false;
        }
        if (options_.direct) return englishKey(ec, code);
        if (GetKeyState(VK_CAPITAL) & 1) {
            literal(ec);
            clear();
            return false;
        }
        if (sentence_.expired(GetTickCount64())) {
            sentence_.clear();
            release(sentenceRange_);
        }
        if (!composing() && !sentence_.text.empty() && sentenceRange_ && syncSentence(ec) == RangeEvidence::Different) {
            clear();
        }
        sentence_.touched = GetTickCount64();
        if (code == VK_ESCAPE) {
            if (!composing() && sentence_.text.empty())
                return false;
            if (composition_) {
                auto range = commitText(ec, L"");
                release(range);
            }
            clear();
            return true;
        }
        if (code == VK_BACK) {
            if (!backspacePinyin(raw_, locked_, selectionUndo_))
                return false;
            selected_ = 0;
            if (!composing()) {
                auto range = commitText(ec, L"");
                release(range);
                candidates_.clear();
                translate();
                geometry(ec);
            } else
                refresh(ec);
            return true;
        }
        if (code == VK_RETURN || code == VK_TAB) {
            if (!composing()) {
                clear();
                return false;
            }
            commit(ec, translationOnCommit(false, options_.english, options_.chinese, code == VK_TAB));
            return true;
        }
        if (code == VK_SPACE) {
            if (!composing()) {
                if (!sentence_.text.empty()) {
                    chinese(ec, L" ");
                    return true;
                }
                return false;
            }
            if (raw_.empty())
                commit(ec, options_.english);
            else if (candidates_.empty()) {
                literal(ec);
                clear();
                return false;
            } else
                choose(ec, selected_);
            return true;
        }
        if (!raw_.empty() && !candidates_.empty() && !(GetKeyState(VK_SHIFT) & 0x8000) &&
            (code == VK_OEM_PLUS || code == VK_OEM_MINUS)) {
            selected_ = candidatePage(selected_, candidates_.size(), code == VK_OEM_PLUS);
            translate();
            geometry(ec);
            return true;
        }
        if ((code >= VK_LEFT && code <= VK_DOWN) || code == VK_PRIOR || code == VK_NEXT || code == VK_HOME ||
            code == VK_END || code == VK_DELETE) {
            if (raw_.empty()) {
                if (composing()) { literal(ec); clear(); }
                return false;
            }
            if (code == VK_DELETE) return false;
            int movement = code == VK_LEFT || code == VK_UP ? -1
                           : code == VK_PRIOR               ? -5
                           : code == VK_NEXT                ? 5
                                                            : 1;
            if (!candidates_.empty()) {
                if (code == VK_PRIOR || code == VK_NEXT)
                    selected_ = candidatePage(selected_, candidates_.size(), code == VK_NEXT);
                else if (code == VK_HOME) selected_ = 0;
                else if (code == VK_END) selected_ = candidates_.size() - 1;
                else selected_ = (size_t)std::clamp((int)selected_ + movement, 0, (int)candidates_.size() - 1);
            }
            translate();
            geometry(ec);
            return true;
        }
        if (code >= '1' && code <= '5' && !raw_.empty() && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            size_t index = (selected_ / 5) * 5 + code - '1';
            choose(ec, index);
            return true;
        }
        if (code >= 'A' && code <= 'Z' && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            if (raw_.size() >= 100 || locked_.size() >= 500) {
                MessageBeep(MB_OK);
                return true;
            }
            if (sentence_.finished && insertionOffset_ >= sentence_.text.size()) {
                sentence_.clear();
                release(sentenceRange_);
            }
            sentence_.touched = GetTickCount64();
            raw_ += (char)(code - 'A' + 'a');
            selected_ = 0;
            refresh(ec);
            return true;
        }
        if (code == VK_OEM_7 && !raw_.empty() && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            raw_ += '\'';
            selected_ = 0;
            refresh(ec);
            return true;
        }
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        std::wstring mark;
        if (code == VK_OEM_COMMA)
            mark = L"，";
        if (code == VK_OEM_PERIOD)
            mark = L"。";
        if (code == VK_OEM_1)
            mark = shift ? L"：" : L"；";
        if (code == VK_OEM_2 && shift)
            mark = L"？";
        if (code == '1' && shift)
            mark = L"！";
        if (!mark.empty() && (composing() || !sentence_.text.empty())) {
            if (options_.english && composing() && fragment()) {
                locked_ = *fragment() + mark;
                raw_.clear();
                selected_ = 0;
                refresh(ec);
            } else
                chinese(ec, mark);
            return true;
        }
        if (composing())
            chinese(ec);
        clear();
        return false;
    }

  public:
    TextService() {
        ++objects;
        options_ = Options::read();
        target_->token = ((uint64_t)GetTickCount64() << 16) ^ (uintptr_t)this;
    }
    ~TextService() {
        Deactivate();
        --objects;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id == IID_IUnknown || id == IID_ITfTextInputProcessor || id == IID_ITfTextInputProcessorEx)
            *p = (ITfTextInputProcessorEx *)this;
        else if (id == IID_ITfKeyEventSink)
            *p = (ITfKeyEventSink *)this;
        else if (id == IID_ITfThreadMgrEventSink)
            *p = (ITfThreadMgrEventSink *)this;
        else if (id == IID_ITfThreadFocusSink)
            *p = (ITfThreadFocusSink *)this;
        else if (id == IID_ITfCompositionSink)
            *p = (ITfCompositionSink *)this;
        else if (id == IID_ITfTextEditSink)
            *p = (ITfTextEditSink *)this;
        else if (id == IID_ITfTextLayoutSink)
            *p = (ITfTextLayoutSink *)this;
        else if (id == IID_ITfDisplayAttributeProvider)
            *p = (ITfDisplayAttributeProvider *)this;
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs_);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr *manager, TfClientId client) override {
        return ActivateEx(manager, client, 0);
    }
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr *manager, TfClientId client, DWORD flags) override {
        if (!manager)
            return E_INVALIDARG;
        if (manager_)
            Deactivate();
        manager_ = manager;
        manager_->AddRef();
        client_ = client;
        flags_ = flags;
        secure_ = (flags & TF_TMAE_SECUREMODE) != 0;
        active_ = true;
        if (secure_)
            return S_OK;
        try {
            pinModule();
            try { pinyin().start(); } catch (...) {}
            WNDCLASSW wc = {};
            wc.lpfnWndProc = proc;
            wc.hInstance = module;
            wc.lpszClassName = L"Fuyi.TSF.Messages.1";
            RegisterClassW(&wc);
            messages_ =
                CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, module, this);
            target_->window = messages_;
            panel_ = std::make_unique<Panel>();
            panel_->choose = [this](size_t index) {
                auto raw = raw_;
                request(
                    [this, index, raw](TfEditCookie ec) {
                        if (raw_ != raw)
                            return S_OK;
                        if (!sensitive(ec))
                            choose(ec, index);
                        else
                            clear();
                        return S_OK;
                    },
                    false, true);
            };
            panel_->applyCorrection=[this] {
                auto expected=source(), correction=correction_;
                if(!options_.direct||correction.empty())return;
                request([this,expected,correction](TfEditCookie ec){
                    if(sensitive(ec)||source()!=expected||correction_!=correction)return S_OK;
                    // Only explicitly apply to the same verified sentence range.
                    if(!composition_ && sentenceRange_) {
                        if(prefixEvidence(ec)!=RangeEvidence::Matches || !selectionAtEnd(ec,sentenceRange_))return S_OK;
                        if(SUCCEEDED(sentenceRange_->SetText(ec,0,correction.c_str(),(LONG)correction.size()))) {
                            wroteText_=true;expectEdit_=true;selectEnd(ec,sentenceRange_);
                            sentence_.text=correction;insertionOffset_=correction.size();sentence_.touched=GetTickCount64();
                            sentence_.finished=!correction.empty()&&std::wstring(L".!?\n").find(correction.back())!=std::wstring::npos;
                            translate();geometry(ec);
                        }
                    } else {english_=correction;translatedSource_=expected;commit(ec,true,true);}
                    return S_OK;
                },false,true);
            };
            SetTimer(messages_, 1, 150, nullptr);
            ITfCompartmentMgr *compartments = nullptr;
            if (SUCCEEDED(manager_->QueryInterface(IID_ITfCompartmentMgr,(void**)&compartments))) {
                ITfCompartment *open = nullptr;
                if(SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&open))) {
                    VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=1;
                    open->SetValue(client_,&value);open->Release();
                }
                compartments->Release();
            }
            ITfKeystrokeMgr *keys = nullptr;
            if (FAILED(manager_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&keys)))
                return E_FAIL;
            auto hr = keys->AdviseKeyEventSink(client_, (ITfKeyEventSink *)this, TRUE);
            keys->Release();
            if (FAILED(hr))
                return hr;
            if (SUCCEEDED(manager_->QueryInterface(IID_ITfSource, (void **)&managerSource_))) {
                managerSource_->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this,
                                           &managerCookie_);
                managerSource_->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink *)this, &focusCookie_);
            }
            ITfCategoryMgr *categories = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_ITfCategoryMgr, (void **)&categories))) {
                categories->RegisterGUID(attributeId, &atom_);
                categories->Release();
            }
            if (SUCCEEDED(manager_->QueryInterface(IID_ITfLangBarItemMgr, (void **)&bar_))) {
                language_ = new LanguageButton;
                language_->read = [this] { return options_; };
                language_->command = [this](UINT id) {
                    if (id == 4) {
                        settings();
                        return;
                    }
                    finishComposition();
                    clear();
                    if (id == 1)
                        options_.direct = !options_.direct;
                    if (id == 2)
                        options_.english = !options_.english;
                    if (id == 3)
                        options_.privacy = !options_.privacy;
                    if (id == 5)
                        options_.chinese = !options_.chinese;
                    storeOptions();
                    language_->update();
                };
                bar_->AddItem(language_);
            }
            ITfDocumentMgr *doc = nullptr;
            if (SUCCEEDED(manager_->GetFocus(&doc)) && doc) {
                ITfContext *context = nullptr;
                if (SUCCEEDED(doc->GetTop(&context))) {
                    attach(context);
                    release(context);
                }
                release(doc);
            }
            return S_OK;
        } catch (...) {
            Deactivate();
            return E_FAIL;
        }
    }
    HRESULT STDMETHODCALLTYPE Deactivate() override {
        if (!manager_)
            return S_OK;
        if (active_)
            finishComposition();
        active_ = false;
        target_->window = nullptr;
        target_->generation = ++revision_;
        if (messages_) {
            KillTimer(messages_, 1);
            MSG message;
            while (PeekMessageW(&message, messages_, WM_TRANSLATED, WM_TRANSLATED, PM_REMOVE))
                delete (TranslationResult *)message.lParam;
            DestroyWindow(messages_);
            messages_ = nullptr;
        }
        detach();
        panel_.reset();
        if (language_) {
            language_->read = [] { return Options::read(); };
            language_->command = {};
        }
        if (bar_ && language_)
            bar_->RemoveItem(language_);
        release(language_);
        release(bar_);
        ITfKeystrokeMgr *keys = nullptr;
        if (SUCCEEDED(manager_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&keys))) {
            keys->UnadviseKeyEventSink(client_);
            keys->Release();
        }
        if (managerSource_) {
            if (managerCookie_ != TF_INVALID_COOKIE)
                managerSource_->UnadviseSink(managerCookie_);
            if (focusCookie_ != TF_INVALID_COOKIE)
                managerSource_->UnadviseSink(focusCookie_);
        }
        managerCookie_ = focusCookie_ = TF_INVALID_COOKIE;
        release(managerSource_);
        release(manager_);
        client_ = TF_CLIENTID_NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override {
        if (!foreground) {
            finishComposition();
            clear();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext *context, WPARAM key, LPARAM, BOOL *eaten) override {
        if (!eaten)
            return E_POINTER;
        try { attach(context); *eaten = canHandle(key); }
        catch (...) { *eaten = FALSE; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext *context, WPARAM code, LPARAM, BOOL *eaten) override {
        if (!eaten)
            return E_POINTER;
        *eaten = FALSE;
        editFailed_ = wroteText_ = false;
        try {
            attach(context);
            auto result = std::make_shared<bool>(false);
            auto hr = request([this, code, result](TfEditCookie ec) {
                *result = key(ec, code);
                if (editFailed_) {
                    // If the host rejected SetText, preserve the previous marked text
                    // as literal input and let this original key reach the host.
                    *result = wroteText_;
                    clear();
                }
                return S_OK;
            });
            if (hr == TF_S_ASYNC)
                *eaten = canHandle(code);
            else if (SUCCEEDED(hr))
                *eaten = *result;
            else {
                *eaten = wroteText_;
                clear();
            }
            return S_OK;
        } catch (...) {
            *eaten = wroteText_;
            clear();
            return S_OK;
        }
    }
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext *, WPARAM code, LPARAM, BOOL *eaten) override {
        if (!eaten)
            return E_POINTER;
        *eaten =
            !secure_ && !options_.privacy && (code == VK_SHIFT || code == VK_LSHIFT || code == VK_RSHIFT);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext *context, WPARAM code, LPARAM, BOOL *eaten) override {
        if (!eaten)
            return E_POINTER;
        *eaten = FALSE;
        if (code == VK_SHIFT || code == VK_LSHIFT || code == VK_RSHIFT) {
            bool toggle = shift_.up(GetTickCount64(), shortcut(code));
            if (toggle && !options_.privacy && !secure_) {
                attach(context);
                finishComposition();
                clear();
                options_.direct = !options_.direct;
                storeOptions();
                if (language_)
                    language_->update();
                *eaten = FALSE;
            }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext *, REFGUID, BOOL *eaten) override {
        if (!eaten)
            return E_POINTER;
        *eaten = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext *context) override {
        attach(context);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext *context) override {
        if (context == context_)
            detach();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr *focus, ITfDocumentMgr *) override {
        ITfContext *context = nullptr;
        if (focus)
            focus->GetTop(&context);
        attach(context);
        release(context);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSetThreadFocus() override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKillThreadFocus() override {
        finishComposition();
        clear();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie ec, ITfComposition *terminated) override {
        if (!closingComposition_ && terminated == composition_) {
            ITfRange *range = nullptr;
            auto text = locked_ + wide(raw_);
            bool retained = !text.empty() && SUCCEEDED(terminated->GetRange(&range)) &&
                retainEnglishPrefix(options_.direct,sensitive(ec),rangeTextEvidence(ec,range,text),
                                    selectionEvidence(ec,range),prefixEvidence(ec));
            if (retained) {
                bool prefix = !sentence_.text.empty();
                LONG diff=1;
                if (sentenceRange_ && insertionOffset_ < sentence_.text.size()) {
                    if (insertionOffset_ == 0 && FAILED(sentenceRange_->ShiftStartToRange(ec, range, TF_ANCHOR_START)))
                        release(sentenceRange_);
                } else if (sentenceRange_ && SUCCEEDED(sentenceRange_->CompareEnd(ec,range,TF_ANCHOR_START,&diff)) && diff==0)
                    {if(FAILED(sentenceRange_->ShiftEndToRange(ec,range,TF_ANCHOR_END)))release(sentenceRange_);}
                else { release(sentenceRange_); if(!prefix)range->Clone(&sentenceRange_); }
                if(sentenceRange_)sentenceRange_->SetGravity(ec,TF_GRAVITY_FORWARD,TF_GRAVITY_BACKWARD);
                auto combined = insertedSentence(sentence_.text, insertionOffset_, text);
                insertionOffset_ += text.size();
                sentence_.clear(); sentence_.append(combined, GetTickCount64());
                release(composition_);raw_.clear();locked_.clear(); selectionUndo_.clear();candidates_.clear();selected_=0;
                translate(); geometry(ec);
            } else { release(composition_); clear(); }
            release(range);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext *context, TfEditCookie ec,
                                        ITfEditRecord *record) override {
        if (!active_ || !context_ || context != context_)
            return S_OK;
        try {
            if (sensitive(ec)) {
                secure_ = true;
                clear();
                return S_OK;
            }
            IEnumTfRanges *ranges = nullptr;
            bool changed = false;
            if (SUCCEEDED(record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, nullptr, 0, &ranges))) {
                ITfRange *range = nullptr;
                ULONG count = 0;
                if (SUCCEEDED(ranges->Next(1, &range, &count)) && count) {
                    changed = true;
                    range->Release();
                }
                ranges->Release();
            }
            if (ownEditDepth_) return S_OK;
            if (!composition_ && sentenceRange_) {
                auto evidence = syncSentence(ec);
                if (evidence == RangeEvidence::Different) { clear(); return S_OK; }
                if (evidence == RangeEvidence::Unknown && changed) release(sentenceRange_);
                translate(); geometry(ec); return S_OK;
            }
            // Hosts may send several text/selection notifications for one IME
            // write. Validate our bounded range instead of clearing on the second.
            auto prefix = prefixEvidence(ec);
            ITfRange *current = nullptr;
            if (composition_) composition_->GetRange(&current);
            auto caret = selectionEvidence(ec, current ? current : sentenceRange_);
            auto markedText = current ? rangeTextEvidence(ec, current, locked_ + wide(raw_)) : RangeEvidence::Matches;
            release(current);
            if (breaksSentence(caret, prefix) || (changed && markedText == RangeEvidence::Different)) {
                clear(); return S_OK;
            }
            if (changed && !expectEdit_ && prefix == RangeEvidence::Unknown)
                release(sentenceRange_); // copy preview remains; automatic replacement is unsafe
            if (changed) expectEdit_ = false;
            if (composing() || !sentence_.text.empty())
                geometry(ec);
        } catch (...) {
            clear();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLayoutChange(ITfContext *context, TfLayoutCode, ITfContextView *) override {
        if (!context_ || context != context_ || layoutPending_ || (!composing() && sentence_.text.empty()))
            return S_OK;
        layoutPending_ = true;
        auto hr = request(
            [this](TfEditCookie ec) {
                layoutPending_ = false;
                geometry(ec);
                return S_OK;
            },
            true, true);
        if (FAILED(hr))
            layoutPending_ = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **p) override {
        if (!p)
            return E_POINTER;
        *p = new AttributeEnum;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(REFGUID id, ITfDisplayAttributeInfo **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id != attributeId)
            return E_INVALIDARG;
        *p = new AttributeInfo;
        return S_OK;
    }
};
class Factory : public IClassFactory {
    LONG refs_ = 1;

  public:
    Factory() {
        ++objects;
    }
    ~Factory() {
        --objects;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id != IID_IUnknown && id != IID_IClassFactory)
            return E_NOINTERFACE;
        *p = this;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs_);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID id, void **p) override {
        if (outer)
            return CLASS_E_NOAGGREGATION;
        auto service = new TextService;
        auto hr = service->QueryInterface(id, p);
        service->Release();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        objects += lock ? 1 : -1;
        return S_OK;
    }
};
} // namespace fuyi
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *) {
    if (reason == DLL_PROCESS_ATTACH) {
        fuyi::module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID id, void **out) {
    if (clsid != fuyi::serviceId)
        return CLASS_E_CLASSNOTAVAILABLE;
    auto factory = new fuyi::Factory;
    auto hr = factory->QueryInterface(id, out);
    factory->Release();
    return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    return fuyi::objects ? S_FALSE : S_OK;
}
