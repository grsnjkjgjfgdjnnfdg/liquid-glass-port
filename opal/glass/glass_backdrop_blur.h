#pragma once
#include <d3d11.h>
#include <algorithm>
#include <cstring>
#include "../imgui/imgui.h"
#include "../opal_types.h"
#include "../backend/dx11_resources.h"
#include "../backend/dx11_shaders.h"
#include "glass_blur.h"

struct ImDrawList;

namespace opal::glass {

// ---------------------------------------------------------------------------
// True backdrop blur for a screen region.
//
// A component cannot blur its own ImGui draw commands after the fact, so this
// captures the live frame at render time (which already contains the element),
// runs the same separable Gaussian the glass uses, and composites the blurred
// copy back over just that region. The element is genuinely blurred on top of
// whatever glass is behind it, and the radius decays to zero so the element
// resolves to full sharpness.
//
// State lives at namespace scope rather than in function-local statics for the
// same NO-TLS reason documented in comp_panel.h.
// ---------------------------------------------------------------------------
struct BackdropBlurState {
    ID3D11Device*             device = nullptr;
    ID3D11DeviceContext*      ctx = nullptr;
    GaussianBlur              blur;
    ID3D11Texture2D*          captureTex = nullptr;
    ID3D11ShaderResourceView* captureSRV = nullptr;
    ID3D11VertexShader*       copyVS = nullptr;
    ID3D11PixelShader*        copyPS = nullptr;
    ID3D11SamplerState*       sampler = nullptr;
    ID3D11RasterizerState*    rast = nullptr;
    ID3D11BlendState*         blend = nullptr;   // constant-factor alpha fade
    ID3D11DepthStencilState*  depth = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    int   width = 0;
    int   height = 0;
    float dpiScale = 1.0f;
    bool  initialized = false;
};

inline BackdropBlurState g_backdropBlur;

inline void BackdropBlurShutdown();

inline bool BackdropBlurInit(ID3D11Device* device, int width, int height) {
    if (!device || width <= 0 || height <= 0) return false;
    // Idempotent: GlassRenderer::Init runs this on every device (re)creation,
    // and hosts may have configured the DPI scale before it.
    BackdropBlurShutdown();
    auto& s = g_backdropBlur;
    s.device = device;
    s.width  = width;
    s.height = height;
    device->GetImmediateContext(&s.ctx);

    s.copyVS  = backend::CreateFullscreenVS(device);
    s.copyPS  = backend::CreateCopyPS(device);
    s.sampler = backend::CreateSampler(device);
    if (!s.blur.Init(device, width, height)) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode      = D3D11_FILL_SOLID;
    rd.CullMode      = D3D11_CULL_NONE;
    rd.ScissorEnable = TRUE;
    device->CreateRasterizerState(&rd, &s.rast);

    // Constant-factor alpha fade: result = blurred * a + dst * (1 - a). The
    // dest alpha is preserved so an opaque visual (DComp) keeps its alpha=1.
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_BLEND_FACTOR;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_BLEND_FACTOR;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&bd, &s.blend);

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    device->CreateDepthStencilState(&dd, &s.depth);

    s.initialized = (s.copyVS && s.copyPS && s.sampler && s.rast && s.blend && s.depth);
    return s.initialized;
}

inline void BackdropBlurShutdown() {
    auto& s = g_backdropBlur;
    s.blur.Shutdown();
    backend::SafeRelease(s.captureSRV);
    backend::SafeRelease(s.captureTex);
    backend::SafeRelease(s.copyVS);
    backend::SafeRelease(s.copyPS);
    backend::SafeRelease(s.sampler);
    backend::SafeRelease(s.rast);
    backend::SafeRelease(s.blend);
    backend::SafeRelease(s.depth);
    backend::SafeRelease(s.ctx);
    s.device = nullptr;
    s.width = s.height = 0;
    s.format = DXGI_FORMAT_UNKNOWN;
    s.initialized = false;
}

inline void BackdropBlurSetDpiScale(float scale) { g_backdropBlur.dpiScale = scale > 0.0f ? scale : 1.0f; }

namespace backdrop_detail {

struct Item { float x0, y0, x1, y1, radius, alpha; };

inline void EnsureCapture(ID3D11Device* device, ID3D11Texture2D* bbTex) {
    auto& s = g_backdropBlur;
    D3D11_TEXTURE2D_DESC d = {};
    bbTex->GetDesc(&d);
    if (s.captureTex && s.format == d.Format &&
        s.width == (int)d.Width && s.height == (int)d.Height)
        return;
    backend::SafeRelease(s.captureSRV);
    backend::SafeRelease(s.captureTex);
    s.format = d.Format;
    s.width  = (int)d.Width;
    s.height = (int)d.Height;
    s.captureTex = backend::CreateTexture2D(device, s.width, s.height, s.format,
                                            D3D11_BIND_SHADER_RESOURCE);
    if (s.captureTex) s.captureSRV = backend::CreateSRV(device, s.captureTex);
    s.blur.Resize(device, s.width, s.height);
}

inline void Draw(ID3D11DeviceContext* dc, const Item* d) {
    auto& s = g_backdropBlur;
    // The capture texture is created lazily below from the current render
    // target, so it must not be part of this guard.
    if (!dc || !s.initialized || !s.device) return;

    ID3D11VertexShader* oldVS = nullptr;
    ID3D11PixelShader*  oldPS = nullptr;
    dc->VSGetShader(&oldVS, nullptr, nullptr);
    dc->PSGetShader(&oldPS, nullptr, nullptr);

    UINT vpCount = 1;
    D3D11_VIEWPORT oldVP = {};
    dc->RSGetViewports(&vpCount, &oldVP);
    ID3D11RasterizerState* oldRS = nullptr;
    dc->RSGetState(&oldRS);
    D3D11_PRIMITIVE_TOPOLOGY oldTopo;
    dc->IAGetPrimitiveTopology(&oldTopo);
    ID3D11InputLayout* oldLayout = nullptr;
    dc->IAGetInputLayout(&oldLayout);

    ID3D11ShaderResourceView* oldSRV0 = nullptr;
    ID3D11SamplerState* oldSamp = nullptr;
    ID3D11Buffer* oldCB0 = nullptr;
    dc->PSGetShaderResources(0, 1, &oldSRV0);
    dc->PSGetSamplers(0, 1, &oldSamp);
    dc->PSGetConstantBuffers(0, 1, &oldCB0);

    ID3D11BlendState* oldBlend = nullptr;
    FLOAT oldFactor[4] = { 1, 1, 1, 1 };
    UINT oldMask = 0xFFFFFFFF;
    dc->OMGetBlendState(&oldBlend, oldFactor, &oldMask);
    ID3D11DepthStencilState* oldDepth = nullptr;
    UINT oldStencilRef = 0;
    dc->OMGetDepthStencilState(&oldDepth, &oldStencilRef);

    UINT scissorCount = 0;
    dc->RSGetScissorRects(&scissorCount, nullptr);
    D3D11_RECT oldScissor[1] = {};
    if (scissorCount >= 1) { scissorCount = 1; dc->RSGetScissorRects(&scissorCount, oldScissor); }

    ID3D11RenderTargetView* oldRTV = nullptr;
    dc->OMGetRenderTargets(1, &oldRTV, nullptr);

    ID3D11Resource* rtRes = nullptr;
    if (oldRTV) oldRTV->GetResource(&rtRes);
    ID3D11Texture2D* bbTex = nullptr;
    if (rtRes) rtRes->QueryInterface(IID_PPV_ARGS(&bbTex));

    if (bbTex) {
        EnsureCapture(s.device, bbTex);
        if (s.captureTex) {
            ID3D11RenderTargetView* nullRTV = nullptr;
            dc->OMSetRenderTargets(1, &nullRTV, nullptr);
            dc->CopyResource(s.captureTex, bbTex);

            ID3D11ShaderResourceView* blurred = nullptr;
            s.blur.Execute(dc, s.captureSRV, &blurred, d->radius);
            if (blurred) {
                dc->OMSetRenderTargets(1, &oldRTV, nullptr);
                D3D11_VIEWPORT vp = { 0, 0, (float)s.width, (float)s.height, 0.0f, 1.0f };
                dc->RSSetViewports(1, &vp);
                dc->RSSetState(s.rast);
                D3D11_RECT sr = { (LONG)d->x0, (LONG)d->y0, (LONG)d->x1, (LONG)d->y1 };
                dc->RSSetScissorRects(1, &sr);
                const float factor[4] = { d->alpha, d->alpha, d->alpha, d->alpha };
                dc->OMSetBlendState(s.blend, factor, 0xFFFFFFFF);
                dc->OMSetDepthStencilState(s.depth, 0);
                dc->IASetInputLayout(nullptr);
                dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                dc->VSSetShader(s.copyVS, nullptr, 0);
                dc->PSSetShader(s.copyPS, nullptr, 0);
                dc->PSSetShaderResources(0, 1, &blurred);
                dc->PSSetSamplers(0, 1, &s.sampler);
                dc->Draw(3, 0);
                ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
                dc->PSSetShaderResources(0, 1, nullSRV);
            }
        }
        bbTex->Release();
    }
    if (rtRes) rtRes->Release();

    dc->VSSetShader(oldVS, nullptr, 0);
    dc->PSSetShader(oldPS, nullptr, 0);
    dc->RSSetViewports(1, &oldVP);
    dc->RSSetState(oldRS);
    if (scissorCount >= 1) dc->RSSetScissorRects(1, oldScissor);
    dc->IASetPrimitiveTopology(oldTopo);
    dc->IASetInputLayout(oldLayout);
    dc->PSSetShaderResources(0, 1, &oldSRV0);
    dc->PSSetSamplers(0, 1, &oldSamp);
    dc->PSSetConstantBuffers(0, 1, &oldCB0);
    dc->OMSetBlendState(oldBlend, oldFactor, oldMask);
    dc->OMSetDepthStencilState(oldDepth, oldStencilRef);
    dc->OMSetRenderTargets(1, &oldRTV, nullptr);

    if (oldVS) oldVS->Release();
    if (oldPS) oldPS->Release();
    if (oldRS) oldRS->Release();
    if (oldLayout) oldLayout->Release();
    if (oldSRV0) oldSRV0->Release();
    if (oldSamp) oldSamp->Release();
    if (oldCB0) oldCB0->Release();
    if (oldBlend) oldBlend->Release();
    if (oldDepth) oldDepth->Release();
    if (oldRTV) oldRTV->Release();
}

} // namespace backdrop_detail

// Queue a true backdrop blur over `rect` (logical coordinates). `radius` is the
// Gaussian sigma in logical pixels, `alpha` the composite mix. The draw is
// appended to the draw list, so it must be called AFTER the element it blurs.
// `pad` grows the region so the blurred edge does not seam against the sharp
// surround; pass 0 to blur exactly the element bounds.
inline void BackdropBlurComposite(ImDrawList* dl, const Rect& rect, float radius, float alpha,
                                  float pad = 0.0f) {
    auto& s = g_backdropBlur;
    if (!dl || !s.initialized) return;
    if (alpha <= 0.003f || radius <= 0.05f) return;
    if (rect.w <= 1.0f || rect.h <= 1.0f) return;

    const float k = s.dpiScale;
    auto* d = new backdrop_detail::Item();
    d->x0 = (rect.x - pad) * k;
    d->y0 = (rect.y - pad) * k;
    d->x1 = (rect.x + rect.w + pad) * k;
    d->y1 = (rect.y + rect.h + pad) * k;
    d->radius = radius * k;
    d->alpha  = alpha;
    dl->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd) {
        auto* item = (backdrop_detail::Item*)cmd->UserCallbackData;
        backdrop_detail::Draw(g_backdropBlur.ctx, item);
        delete item;
    }, d);
}

} // namespace opal::glass
