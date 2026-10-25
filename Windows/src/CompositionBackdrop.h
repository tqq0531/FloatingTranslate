#pragma once
#include "WinUtil.h"
#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi.h>
#include <roapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
namespace fuyi {
namespace composition_native {
// SDK COM interop contracts. Keeping these ABI-only declarations avoids mixing
// the legacy MinGW WinRT projections with the generated C++/WinRT projection.
struct Desktop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopWindowTarget(HWND, BOOL, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD) = 0;
};
struct Graphics : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForHandle(HANDLE, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForSwapChain(IUnknown *, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateGraphicsDevice(IUnknown *, void **) = 0;
};
struct Drawing : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE BeginDraw(const RECT *, REFIID, void **, POINT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE EndDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE Resize(SIZE) = 0;
    virtual HRESULT STDMETHODCALLTYPE Scroll(const RECT *, const RECT *, int, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResumeDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE SuspendDraw() = 0;
};
inline constexpr GUID desktopId{0x29e691fa,0x4567,0x4dca,{0xb3,0x19,0xd0,0xf2,0x07,0xeb,0x68,0x07}};
inline constexpr GUID graphicsId{0x25297d5c,0x3ad4,0x4c9c,{0xb5,0xcf,0xe3,0x6a,0x38,0x51,0x23,0x30}};
inline constexpr GUID drawingId{0xfd04e6e3,0xfe0c,0x4c3c,{0xab,0x19,0xa0,0x76,0x01,0xa5,0x76,0xee}};
template<class T,class W> winrt::com_ptr<T> query(const W &object,const GUID &id) {
    winrt::com_ptr<T> result;
    winrt::check_hresult(winrt::get_unknown(object)->QueryInterface(id,result.put_void()));
    return result;
}
}
class CompositionBackdrop {
    winrt::Windows::UI::Composition::Compositor compositor_{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target_{nullptr};
    winrt::Windows::UI::Composition::ContainerVisual root_{nullptr};
    winrt::Windows::UI::Composition::SpriteVisual backdrop_{nullptr}, foreground_{nullptr};
    winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry geometry_{nullptr};
    winrt::Windows::UI::Composition::CompositionGraphicsDevice graphics_{nullptr};
    winrt::Windows::UI::Composition::CompositionDrawingSurface surface_{nullptr};
    winrt::Windows::System::DispatcherQueueController queue_{nullptr};
    winrt::com_ptr<ID3D11Device> d3d_;
    winrt::com_ptr<ID2D1Device> d2d_;
    bool initialized_ = false;
    int width_=0,height_=0;
    HRESULT error_=S_OK;
    HWND window_=nullptr;
    bool hostBackdrop(HWND window,bool allow=true) {
        auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Set=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        auto set=dwm?(Set)GetProcAddress(dwm,"DwmSetWindowAttribute"):nullptr;
        BOOL enabled=allow?TRUE:FALSE;
        bool result=set && SUCCEEDED(set(window,17,&enabled,sizeof(enabled)));
        if(dwm)FreeLibrary(dwm);
        if(result)return true;
        struct Accent{int state,flags;DWORD tint;int animation;} accent{allow?5:0,0,0,0};
        struct Attribute{int attribute;void *data;SIZE_T size;} attribute{19,&accent,sizeof(accent)};
        using SetComposition=BOOL(WINAPI*)(HWND,Attribute*);
        auto native=(SetComposition)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetWindowCompositionAttribute");
        return native && native(window,&attribute);
    }
  public:
    ~CompositionBackdrop(){reset();}
    HRESULT error()const{return error_;}
    void reset() noexcept {
        try{if(target_)target_.Root(nullptr);}catch(...){}
        if(window_ && IsWindow(window_))hostBackdrop(window_,false);
        window_=nullptr;
        surface_=nullptr;graphics_=nullptr;geometry_=nullptr;
        foreground_=nullptr;backdrop_=nullptr;root_=nullptr;target_=nullptr;compositor_=nullptr;
        d2d_=nullptr;d3d_=nullptr;width_=height_=0;
        try{if(queue_)queue_.ShutdownQueueAsync();}catch(...){}
        queue_=nullptr;
        if(initialized_){RoUninitialize();initialized_=false;}
    }
    bool start(HWND window) noexcept {
        try {
            window_=window;
            HRESULT init=RoInitialize(RO_INIT_SINGLETHREADED);
            initialized_=SUCCEEDED(init);
            if(FAILED(init) && init!=RPC_E_CHANGED_MODE)winrt::check_hresult(init);
            if(!winrt::Windows::System::DispatcherQueue::GetForCurrentThread()) {
                auto core=LoadLibraryExW(L"CoreMessaging.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
                struct Options{DWORD size;int thread,apartment;} options{sizeof(Options),2,0};
                using Create=HRESULT(WINAPI*)(Options,void**);
                auto create=core?(Create)GetProcAddress(core,"CreateDispatcherQueueController"):nullptr;
                HRESULT status=create?create(options,winrt::put_abi(queue_)):E_NOINTERFACE;
                if(core)FreeLibrary(core);
                // UWP/CoreText may already have a CoreDispatcher without a
                // Windows.System queue. In that case the host's dispatcher can
                // support Compositor directly; do not reject it prematurely.
                if(FAILED(status))queue_=nullptr;
            }
            compositor_=winrt::Windows::UI::Composition::Compositor();
            auto desktop=composition_native::query<composition_native::Desktop>(compositor_,composition_native::desktopId);
            winrt::check_hresult(desktop->CreateDesktopWindowTarget(window,TRUE,winrt::put_abi(target_)));
            if(!hostBackdrop(window))winrt::throw_hresult(E_NOTIMPL);
            D3D_FEATURE_LEVEL level{};
            HRESULT hardware=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr,0,D3D11_SDK_VERSION,d3d_.put(),&level,nullptr);
            if(FAILED(hardware))winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr,0,D3D11_SDK_VERSION,d3d_.put(),&level,nullptr));
            winrt::com_ptr<ID2D1Factory1> factory;
            winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,factory.put_void()));
            auto dxgi=d3d_.as<IDXGIDevice>();
            winrt::check_hresult(factory->CreateDevice(dxgi.get(),d2d_.put()));
            auto interop=composition_native::query<composition_native::Graphics>(compositor_,composition_native::graphicsId);
            winrt::check_hresult(interop->CreateGraphicsDevice(d2d_.get(),winrt::put_abi(graphics_)));
            surface_=graphics_.CreateDrawingSurface({1,1},
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied);
            root_=compositor_.CreateContainerVisual();
            geometry_=compositor_.CreateRoundedRectangleGeometry();
            root_.Clip(compositor_.CreateGeometricClip(geometry_));
            backdrop_=compositor_.CreateSpriteVisual();
            backdrop_.Brush(compositor_.CreateHostBackdropBrush());
            foreground_=compositor_.CreateSpriteVisual();
            foreground_.Brush(compositor_.CreateSurfaceBrush(surface_));
            root_.Children().InsertAtTop(backdrop_);
            root_.Children().InsertAtTop(foreground_);
            target_.Root(root_);
            return true;
        }catch(const winrt::hresult_error &e){error_=e.code();reset();return false;}
        catch(...){error_=E_FAIL;reset();return false;}
    }
    bool draw(const void *pixels,int stride,int width,int height,float radius) noexcept {
        try {
            auto interop=composition_native::query<composition_native::Drawing>(surface_,composition_native::drawingId);
            if(width_!=width || height_!=height) {
                winrt::check_hresult(interop->Resize({width,height}));
                width_=width;height_=height;
                winrt::Windows::Foundation::Numerics::float2 size{(float)width,(float)height};
                root_.Size(size);backdrop_.Size(size);foreground_.Size(size);
                geometry_.Size(size);
            }
            geometry_.CornerRadius({radius,radius});
            winrt::com_ptr<ID2D1DeviceContext> context; POINT offset{};
            winrt::check_hresult(interop->BeginDraw(nullptr,__uuidof(ID2D1DeviceContext),context.put_void(),&offset));
            HRESULT drawStatus=S_OK;
            try {
                context->SetTransform(D2D1::Matrix3x2F::Identity());
                context->Clear(D2D1::ColorF(0,0));
                winrt::com_ptr<ID2D1Bitmap1> bitmap;
                auto properties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
                winrt::check_hresult(context->CreateBitmap(D2D1::SizeU(width,height),pixels,stride,&properties,bitmap.put()));
                auto destination=D2D1::RectF((float)offset.x,(float)offset.y,(float)offset.x+width,(float)offset.y+height);
                context->DrawBitmap(bitmap.get(),destination,1.0f,D2D1_INTERPOLATION_MODE_LINEAR,nullptr,nullptr);
            }catch(const winrt::hresult_error &e){drawStatus=e.code();}
            catch(...){drawStatus=E_FAIL;}
            HRESULT ended=interop->EndDraw();
            winrt::check_hresult(drawStatus);winrt::check_hresult(ended);
            return true;
        }catch(const winrt::hresult_error &e){error_=e.code();return false;}
        catch(...){error_=E_FAIL;return false;}
    }
};
} // namespace fuyi
