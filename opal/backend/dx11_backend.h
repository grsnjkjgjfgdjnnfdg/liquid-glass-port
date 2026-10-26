#pragma once
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <Windows.h>

namespace opal::backend {

struct D3D11Context {
    ID3D11Device*           device    = nullptr;
    ID3D11DeviceContext*    context   = nullptr;
    IDXGISwapChain*         swapChain = nullptr;
    ID3D11RenderTargetView* rtv       = nullptr;
    IDCompositionDevice*   dcompDevice = nullptr;
    IDCompositionTarget*   dcompTarget = nullptr;
    IDCompositionVisual*   dcompVisual = nullptr;
    IDCompositionRectangleClip* dcompClip = nullptr;
    HWND                    hwnd      = nullptr;
    int                     width     = 1280;
    int                     height    = 720;
    bool                    initialized = false;
    bool                    transparentComposition = false;
};

inline void ShutdownD3D11(D3D11Context& ctx);

inline bool InitD3D11(D3D11Context& ctx, HWND hwnd, int width = 1280, int height = 720,
                      bool transparentComposition = false) {
    ctx.hwnd   = hwnd;
    ctx.width  = width;
    ctx.height = height;
    ctx.transparentComposition = transparentComposition;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (transparentComposition) {
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels, 2, D3D11_SDK_VERSION,
            &ctx.device, &featureLevel, &ctx.context);
        if (FAILED(hr)) return false;

        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory = nullptr;
        IDXGISwapChain1* compositionSwapChain = nullptr;
        hr = ctx.device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
        if (SUCCEEDED(hr)) hr = adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);

        DXGI_SWAP_CHAIN_DESC1 sd1 = {};
        sd1.Width = (UINT)width; sd1.Height = (UINT)height;
        sd1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd1.SampleDesc.Count = 1;
        sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd1.BufferCount = 2;
        sd1.Scaling = DXGI_SCALING_STRETCH;
        sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (SUCCEEDED(hr)) hr = factory->CreateSwapChainForComposition(ctx.device, &sd1, nullptr, &compositionSwapChain);
        if (SUCCEEDED(hr)) ctx.swapChain = compositionSwapChain;

        if (SUCCEEDED(hr)) hr = DCompositionCreateDevice(dxgiDevice, __uuidof(IDCompositionDevice), (void**)&ctx.dcompDevice);
        if (SUCCEEDED(hr)) hr = ctx.dcompDevice->CreateTargetForHwnd(hwnd, FALSE, &ctx.dcompTarget);
        if (SUCCEEDED(hr)) hr = ctx.dcompDevice->CreateVisual(&ctx.dcompVisual);
        if (SUCCEEDED(hr)) hr = ctx.dcompVisual->SetContent(ctx.swapChain);
        if (SUCCEEDED(hr)) hr = ctx.dcompDevice->CreateRectangleClip(&ctx.dcompClip);
        if (SUCCEEDED(hr)) {
            // One antialiased DirectComposition clip owns the complete loader
            // silhouette. It matches the shell radius so no second
            // blur layer can peek through at the corners.
            constexpr float radius = 24.0f;
            ctx.dcompClip->SetLeft(0.0f); ctx.dcompClip->SetTop(0.0f);
            ctx.dcompClip->SetRight((float)width); ctx.dcompClip->SetBottom((float)height);
            ctx.dcompClip->SetTopLeftRadiusX(radius); ctx.dcompClip->SetTopLeftRadiusY(radius);
            ctx.dcompClip->SetTopRightRadiusX(radius); ctx.dcompClip->SetTopRightRadiusY(radius);
            ctx.dcompClip->SetBottomLeftRadiusX(radius); ctx.dcompClip->SetBottomLeftRadiusY(radius);
            ctx.dcompClip->SetBottomRightRadiusX(radius); ctx.dcompClip->SetBottomRightRadiusY(radius);
            hr = ctx.dcompVisual->SetClip(ctx.dcompClip);
        }
        if (SUCCEEDED(hr)) hr = ctx.dcompTarget->SetRoot(ctx.dcompVisual);
        if (SUCCEEDED(hr)) hr = ctx.dcompDevice->Commit();

        if (factory) factory->Release();
        if (adapter) adapter->Release();
        if (dxgiDevice) dxgiDevice->Release();
        if (FAILED(hr)) { ShutdownD3D11(ctx); return false; }
    } else {

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = (UINT)width; sd.BufferDesc.Height = (UINT)height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, 2, D3D11_SDK_VERSION, &sd, &ctx.swapChain, &ctx.device,
            &featureLevel, &ctx.context);
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                featureLevels, 2, D3D11_SDK_VERSION, &sd, &ctx.swapChain, &ctx.device,
                &featureLevel, &ctx.context);
        }
        if (FAILED(hr)) return false;
    }

    // Keep at most one frame queued ahead of the display. Together with
    // Present(1, 0) in each host this prevents the live glass pass from
    // building latency while preserving display v-sync.
    IDXGIDevice1* dxgiDevice = nullptr;
    if (SUCCEEDED(ctx.device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice)) && dxgiDevice) {
        dxgiDevice->SetMaximumFrameLatency(1);
        dxgiDevice->Release();
    }

    ID3D11Texture2D* backBuffer = nullptr;
    ctx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        ctx.device->CreateRenderTargetView(backBuffer, nullptr, &ctx.rtv);
        backBuffer->Release();
    }

    ctx.initialized = true;
    return true;
}

inline void ShutdownD3D11(D3D11Context& ctx) {
    if (ctx.rtv)       { ctx.rtv->Release();       ctx.rtv       = nullptr; }
    if (ctx.dcompClip) { ctx.dcompClip->Release(); ctx.dcompClip = nullptr; }
    if (ctx.dcompVisual) { ctx.dcompVisual->Release(); ctx.dcompVisual = nullptr; }
    if (ctx.dcompTarget) { ctx.dcompTarget->Release(); ctx.dcompTarget = nullptr; }
    if (ctx.dcompDevice) { ctx.dcompDevice->Release(); ctx.dcompDevice = nullptr; }
    if (ctx.swapChain) { ctx.swapChain->Release(); ctx.swapChain = nullptr; }
    if (ctx.context)   { ctx.context->Release();   ctx.context   = nullptr; }
    if (ctx.device)    { ctx.device->Release();    ctx.device    = nullptr; }
    ctx.initialized = false;
}

inline void ResizeD3D11(D3D11Context& ctx, int width, int height) {
    if (!ctx.initialized || width <= 0 || height <= 0) return;
    ctx.width  = width;
    ctx.height = height;
    if (ctx.rtv) { ctx.rtv->Release(); ctx.rtv = nullptr; }
    ctx.context->OMSetRenderTargets(0, nullptr, nullptr);
    ctx.swapChain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* backBuffer = nullptr;
    ctx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        ctx.device->CreateRenderTargetView(backBuffer, nullptr, &ctx.rtv);
        backBuffer->Release();
    }
    if (ctx.dcompClip) {
        ctx.dcompClip->SetRight((float)width);
        ctx.dcompClip->SetBottom((float)height);
        if (ctx.dcompDevice) ctx.dcompDevice->Commit();
    }
}

} // namespace opal::backend
