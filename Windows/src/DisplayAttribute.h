#pragma once
#include "Ids.h"
#include "Lifecycle.h"
#include <msctf.h>
namespace fuyi {
class AttributeInfo : public ITfDisplayAttributeInfo {
    ObjectLifetime lifetime_;
    LONG refs_ = 1;

  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id == IID_IUnknown || id == IID_ITfDisplayAttributeInfo) {
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
    HRESULT STDMETHODCALLTYPE GetGUID(GUID *id) override {
        if (!id)
            return E_POINTER;
        *id = attributeId;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR *s) override {
        if (!s)
            return E_POINTER;
        *s = SysAllocString(L"浮译拼音组合");
        return *s ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE GetAttributeInfo(TF_DISPLAYATTRIBUTE *attr) override {
        if (!attr)
            return E_POINTER;
        *attr = {};
        attr->crText.type = TF_CT_NONE;
        attr->crBk.type = TF_CT_NONE;
        attr->lsStyle = TF_LS_SOLID;
        attr->fBoldLine = FALSE;
        attr->crLine.type = TF_CT_COLORREF;
        attr->crLine.cr = RGB(70, 110, 240);
        attr->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetAttributeInfo(const TF_DISPLAYATTRIBUTE *) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        return S_OK;
    }
};
class AttributeEnum : public IEnumTfDisplayAttributeInfo {
    ObjectLifetime lifetime_;
    LONG refs_ = 1;
    bool used_ = false;

  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id == IID_IUnknown || id == IID_IEnumTfDisplayAttributeInfo) {
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
    HRESULT STDMETHODCALLTYPE Clone(IEnumTfDisplayAttributeInfo **p) override {
        if (!p)
            return E_POINTER;
        auto n = new AttributeEnum;
        n->used_ = used_;
        *p = n;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Next(ULONG count, ITfDisplayAttributeInfo **items, ULONG *fetched) override {
        if (!items)
            return E_POINTER;
        if (fetched)
            *fetched = 0;
        if (!count)
            return S_OK;
        if (used_)
            return S_FALSE;
        items[0] = new AttributeInfo;
        used_ = true;
        if (fetched)
            *fetched = 1;
        return count == 1 ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        used_ = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override {
        if (!count)
            return S_OK;
        if (used_)
            return S_FALSE;
        used_ = true;
        return count == 1 ? S_OK : S_FALSE;
    }
};
} // namespace fuyi
