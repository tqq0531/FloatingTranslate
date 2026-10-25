#pragma once
#include "Core.h"
#include "Ids.h"
#include "Lifecycle.h"
#include "WinUtil.h"
#include <msctf.h>
namespace fuyi {
// ABI declarations for interfaces missing from the bundled MinGW headers.
// Signatures and IDs follow Microsoft's msctf.h / ctffunc.h.
namespace tsf {
inline const IID uiId={0xea1ea137,0x19df,0x11d7,{0xa6,0xd2,0x00,0x06,0x5b,0x84,0x43,0x5c}};
inline const IID managerId={0xea1ea135,0x19df,0x11d7,{0xa6,0xd2,0x00,0x06,0x5b,0x84,0x43,0x5c}};
inline const IID candidatesId={0xea1ea138,0x19df,0x11d7,{0xa6,0xd2,0x00,0x06,0x5b,0x84,0x43,0x5c}};
inline const IID behaviorId={0x85fad185,0x58ce,0x497a,{0x94,0x60,0x35,0x53,0x66,0xb6,0x4b,0x9a}};
inline const IID integratedId={0xc7a6f54f,0xb180,0x416f,{0xb2,0xbf,0x7b,0xf2,0xe4,0x68,0x3d,0x7b}};
inline const GUID searchStyle={0xe6d1bd11,0x82f7,0x4903,{0xae,0x21,0x1a,0x63,0x97,0xcd,0xe2,0xeb}};
inline const GUID enabledCategory={0x49d2f9cf,0x1f5e,0x11d7,{0xa6,0xd3,0x00,0x06,0x5b,0x84,0x43,0x5c}};
struct UIElement : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDescription(BSTR *)=0;
    virtual HRESULT STDMETHODCALLTYPE GetGUID(GUID *)=0;
    virtual HRESULT STDMETHODCALLTYPE Show(BOOL)=0;
    virtual HRESULT STDMETHODCALLTYPE IsShown(BOOL *)=0;
};
struct UIManager : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE BeginUIElement(UIElement *,BOOL *,DWORD *)=0;
    virtual HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD)=0;
    virtual HRESULT STDMETHODCALLTYPE EndUIElement(DWORD)=0;
    virtual HRESULT STDMETHODCALLTYPE GetUIElement(DWORD,UIElement **)=0;
    virtual HRESULT STDMETHODCALLTYPE EnumUIElements(IUnknown **)=0;
};
struct CandidateList : UIElement {
    virtual HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD *)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr **)=0;
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT *)=0;
    virtual HRESULT STDMETHODCALLTYPE GetSelection(UINT *)=0;
    virtual HRESULT STDMETHODCALLTYPE GetString(UINT,BSTR *)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPageIndex(UINT *,UINT,UINT *)=0;
    virtual HRESULT STDMETHODCALLTYPE SetPageIndex(UINT *,UINT)=0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT *)=0;
};
struct CandidateBehavior : CandidateList {
    virtual HRESULT STDMETHODCALLTYPE SetSelection(UINT)=0;
    virtual HRESULT STDMETHODCALLTYPE Finalize()=0;
    virtual HRESULT STDMETHODCALLTYPE Abort()=0;
};
struct IntegratedCandidates : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetIntegrationStyle(GUID)=0;
    virtual HRESULT STDMETHODCALLTYPE GetSelectionStyle(int *)=0;
    virtual HRESULT STDMETHODCALLTYPE OnKeyDown(WPARAM,LPARAM,BOOL *)=0;
    virtual HRESULT STDMETHODCALLTYPE ShowCandidateNumbers(BOOL *)=0;
    virtual HRESULT STDMETHODCALLTYPE FinalizeExactCompositionString()=0;
};
}
class CandidateUI : public tsf::CandidateBehavior, public tsf::IntegratedCandidates {
    ObjectLifetime lifetime_;
    LONG refs_=1;
    ITfDocumentMgr *document_=nullptr;
    std::vector<UINT> pages_;
    bool shown_=true;
  public:
    std::vector<Candidate> items;
    UINT selected=0;
    unsigned queried=0;
    std::function<HRESULT(UINT,bool)> select;
    std::function<HRESULT()> abort;
    std::function<HRESULT(WPARAM,BOOL *)> key;
    std::function<void(bool)> visibility;
    ~CandidateUI(){release(document_);}
    void document(ITfDocumentMgr *value){if(value)value->AddRef();release(document_);document_=value;}
    bool update(const std::vector<Candidate> &values,size_t index){
        bool selectionChanged=selected!=index;
        bool changed=values.size()!=items.size();
        if(!changed)for(size_t i=0;i<values.size();++i)if(values[i].text!=items[i].text){changed=true;break;}
        items=values;selected=(UINT)index;
        if(changed || pages_.empty()){pages_.clear();for(UINT i=0;i<items.size();i+=5)pages_.push_back(i);}
        return changed || selectionChanged;
    }
    void disconnect(){select={};abort={};key={};visibility={};items.clear();document(nullptr);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==tsf::uiId || id==tsf::candidatesId || id==tsf::behaviorId)*out=(tsf::CandidateBehavior*)this;
        else if(id==tsf::integratedId){queried|=4;*out=(tsf::IntegratedCandidates*)this;}
        else return E_NOINTERFACE;
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto count=InterlockedDecrement(&refs_);if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR *out) override{if(!out)return E_POINTER;*out=SysAllocString(L"浮译候选");return *out?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetGUID(GUID *out) override{if(!out)return E_POINTER;*out=attributeId;out->Data4[7]^=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE Show(BOOL show) override{if(shown_!=(show!=FALSE)){shown_=show!=FALSE;if(visibility)visibility(shown_);}return S_OK;}
    HRESULT STDMETHODCALLTYPE IsShown(BOOL *out) override{if(!out)return E_POINTER;*out=shown_;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD *out) override{if(!out)return E_POINTER;*out=0x3f;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr **out) override{if(!out)return E_POINTER;*out=document_;if(*out)(*out)->AddRef();return *out?S_OK:E_FAIL;}
    HRESULT STDMETHODCALLTYPE GetCount(UINT *out) override{queried|=1;if(!out)return E_POINTER;*out=(UINT)items.size();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSelection(UINT *out) override{if(!out)return E_POINTER;*out=selected;return selected<items.size()?S_OK:S_FALSE;}
    HRESULT STDMETHODCALLTYPE GetString(UINT index,BSTR *out) override{queried|=2;if(!out)return E_POINTER;*out=nullptr;if(index>=items.size())return E_INVALIDARG;*out=SysAllocString(items[index].text.c_str());return *out?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetPageIndex(UINT *out,UINT size,UINT *count) override{
        if(!count || (size&&!out))return E_POINTER;*count=(UINT)pages_.size();
        std::copy_n(pages_.begin(),std::min<size_t>(size,pages_.size()),out);return size<pages_.size()?S_FALSE:S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPageIndex(UINT *values,UINT count) override{
        if(!values || !count)return E_INVALIDARG;
        if(values[0]!=0)return E_INVALIDARG;
        for(UINT i=0;i<count;++i)if(values[i]>=items.size() || (i&&values[i]<=values[i-1]))return E_INVALIDARG;
        pages_.assign(values,values+count);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT *out) override{if(!out)return E_POINTER;auto i=std::upper_bound(pages_.begin(),pages_.end(),selected);*out=i==pages_.begin()?0:(UINT)(i-pages_.begin()-1);return S_OK;}
    HRESULT STDMETHODCALLTYPE SetSelection(UINT index) override{if(index>=items.size())return E_INVALIDARG;selected=index;auto action=select;return action?action(index,false):E_ABORT;}
    HRESULT STDMETHODCALLTYPE Finalize() override{auto action=select;return action?action(selected,true):E_ABORT;}
    HRESULT STDMETHODCALLTYPE Abort() override{auto action=abort;return action?action():E_ABORT;}
    HRESULT STDMETHODCALLTYPE SetIntegrationStyle(GUID style) override{return style==tsf::searchStyle?S_OK:E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetSelectionStyle(int *out) override{if(!out)return E_POINTER;*out=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE OnKeyDown(WPARAM code,LPARAM,BOOL *eaten) override{if(!eaten)return E_POINTER;*eaten=FALSE;auto action=key;return action?action(code,eaten):S_OK;}
    HRESULT STDMETHODCALLTYPE ShowCandidateNumbers(BOOL *out) override{if(!out)return E_POINTER;*out=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE FinalizeExactCompositionString() override{return E_NOTIMPL;}
};
}
