#pragma once
#include <d3d11.h>
#include <algorithm>
#include "glass_blur.h"
#include "glass_backdrop_blur.h"
#include "glass_composite.h"
#include "glass_gradient.h"
#include "../backend/dx11_resources.h"
#include "../backend/dx11_shaders.h"

namespace opal::glass {

class GlassRenderer {
public:
    bool    Init(ID3D11Device* device, int width, int height);
    void    Shutdown();
    void    Resize(int width, int height);
    void    BeginFrame(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* backbufferRTV, float dt);
    void    SetBlurRadius(float r) { m_blurRadius = r; }
    float   GetBlurRadius() const  { return m_blurRadius; }
    bool    FrameRan() const       { return m_frameRan; }

    ID3D11ShaderResourceView* GetBlurTexture() const { return m_finalBlurSRV; }
    ID3D11ShaderResourceView* GetCaptureSRV() const { return m_captureSRV; }
    GradientBackground&       GetGradient()        { return m_gradient; }
    void SetBgMode(int m)                       { m_bgMode = m; m_gradient.SetMode(m); }
    void SetBgColors(Color c1, Color c2)       { m_gradient.SetColors(c1, c2); }
    void SetBackgroundImage(ID3D11ShaderResourceView* srv) { m_bgImageSRV = srv; }
    int     Width()  const { return m_width; }
    int     Height() const { return m_height; }
    void    SetPremultEnabled(bool e) { m_premultEnabled = e; }
    void    SetForceOpaqueBackground(bool e) { m_forceOpaqueBackground = e; }
    void    SetPresentBlurredBackground(bool e) { m_presentBlurredBackground = e; }
    // Overlay mode: the renderer is drawn on top of an existing scene (a game
    // backbuffer) instead of owning the window. The backbuffer is copied into
    // the capture texture so the glass frosts the real frame, and nothing is
    // ever blitted back over the scene.
    void    SetOverlayMode(bool e) {
        m_overlayMode = e;
        if (e) {
            m_premultEnabled = true;
            m_forceOpaqueBackground = true;
            m_presentBlurredBackground = false;
        }
    }
    bool    GetOverlayMode() const { return m_overlayMode; }
    void    SetBackgroundPreblurred(bool e) { m_backgroundPreblurred = e; }
    void    SetDownsampleBlur(bool e) { m_downsampleBlur = e; }
    bool    GetDownsampleBlur() const  { return m_downsampleBlur; }
    void    SetDownsampleFactor(int factor) {
        const int next = factor < 2 ? 2 : (factor > 8 ? 8 : factor);
        if (next == m_downsampleFactor) return;
        m_downsampleFactor = next;
        if (m_initialized && m_width > 0 && m_height > 0) Resize(m_width, m_height);
    }
    int     GetDownsampleFactor() const { return m_downsampleFactor; }
    void    SetBlurPasses(int passes) { m_blurPasses = passes < 1 ? 1 : (passes > 4 ? 4 : passes); }
    int     GetBlurPasses() const { return m_blurPasses; }
    void    SetRaycasting(bool enabled, float strength = 0.65f) {
        g_glass.raycastStrength = enabled ? Clamp(strength, 0.0f, 1.0f) : 0.0f;
    }
    bool    GetRaycasting() const { return g_glass.raycastStrength > 0.001f; }
    void    SetHDR(bool enabled, float strength = 1.0f) {
        g_glass.hdrStrength = enabled ? Clamp(strength, 0.0f, 1.0f) : 0.0f;
    }
    bool    GetHDR() const { return g_glass.hdrStrength > 0.001f; }

private:
    ID3D11Device*        m_device      = nullptr;
    ID3D11DeviceContext* m_ctx         = nullptr;
    int                  m_width       = 0;
    int                  m_height      = 0;
    bool                 m_initialized  = false;
    // Native-resolution blur avoids enlarged low-resolution artefacts.
    float                m_blurRadius  = 6.0f;
    bool                 m_frameRan    = false;

    GaussianBlur         m_blur;
    GaussianBlur         m_dsBlur;
    GradientBackground   m_gradient;

    ID3D11Texture2D*          m_captureTex   = nullptr;
    ID3D11RenderTargetView*   m_captureRTV   = nullptr;
    ID3D11ShaderResourceView* m_captureSRV   = nullptr;
    ID3D11ShaderResourceView* m_finalBlurSRV = nullptr;

    ID3D11VertexShader*       m_copyVS     = nullptr;
    ID3D11PixelShader*        m_copyPS     = nullptr;
    ID3D11SamplerState*       m_copySamp   = nullptr;
    ID3D11RasterizerState*    m_copyRast   = nullptr;
    ID3D11BlendState*         m_copyBlend  = nullptr;
    ID3D11BlendState*         m_rgbOnlyBlend = nullptr;
    ID3D11DepthStencilState*  m_copyDepth  = nullptr;

    ID3D11Texture2D*          m_premultTex = nullptr;
    ID3D11RenderTargetView*   m_premultRTV = nullptr;
    ID3D11ShaderResourceView* m_premultSRV = nullptr;
    ID3D11PixelShader*        m_premultPS  = nullptr;
    bool                      m_premultEnabled = true;
    bool                      m_forceOpaqueBackground = false;

    ID3D11Texture2D*          m_dsTex    = nullptr;
    ID3D11RenderTargetView*   m_dsRTV    = nullptr;
    ID3D11ShaderResourceView* m_dsSRV    = nullptr;
    ID3D11PixelShader*        m_dsPS     = nullptr;
    // Downsampling is an optional performance mode, not the visual default.
    bool                      m_downsampleBlur = false;
    int                       m_downsampleFactor = 2;
    int                       m_blurPasses = 2;
    bool                      m_presentBlurredBackground = false;
    bool                      m_backgroundPreblurred = false;

    int                       m_bgMode     = 0;
    ID3D11ShaderResourceView* m_bgImageSRV = nullptr;

    bool                      m_overlayMode = false;
    DXGI_FORMAT               m_captureFmt  = DXGI_FORMAT_UNKNOWN;
};

inline bool GlassRenderer::Init(ID3D11Device* device, int width, int height) {
    m_device = device;
    m_width  = width;
    m_height = height;
    device->GetImmediateContext(&m_ctx);

    const int dsW = (std::max)(1, width / m_downsampleFactor);
    const int dsH = (std::max)(1, height / m_downsampleFactor);

    if (!m_blur.Init(device, width, height)) return false;
    if (!m_dsBlur.Init(device, dsW, dsH)) return false;
    if (!m_gradient.Init(device, width, height)) return false;
    if (!GlassCompositeInit(device, width, height)) return false;

    m_captureTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                             D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_captureTex) {
        m_captureRTV = backend::CreateRTV(device, m_captureTex);
        m_captureSRV = backend::CreateSRV(device, m_captureTex);
    }

    m_premultTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                             D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_premultTex) {
        m_premultRTV = backend::CreateRTV(device, m_premultTex);
        m_premultSRV = backend::CreateSRV(device, m_premultTex);
    }
    m_premultPS = backend::CreatePremultiplyPS(device);

    m_dsTex = backend::CreateTexture2D(device, dsW, dsH, DXGI_FORMAT_R8G8B8A8_UNORM,
                                        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_dsTex) {
        m_dsRTV = backend::CreateRTV(device, m_dsTex);
        m_dsSRV = backend::CreateSRV(device, m_dsTex);
    }
    m_dsPS = backend::CreateDownsamplePS(device);

    m_copyVS   = backend::CreateFullscreenVS(device);
    m_copyPS   = backend::CreateCopyPS(device);
    m_copySamp = backend::CreateSampler(device);

    D3D11_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D11_FILL_SOLID;
    rast.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rast, &m_copyRast);

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &m_copyBlend);
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED |
                                                   D3D11_COLOR_WRITE_ENABLE_GREEN |
                                                   D3D11_COLOR_WRITE_ENABLE_BLUE;
    device->CreateBlendState(&blend, &m_rgbOnlyBlend);

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    device->CreateDepthStencilState(&depth, &m_copyDepth);

    m_initialized = (m_captureRTV && m_captureSRV && m_copyVS && m_copyPS &&
                     m_premultPS && m_copyBlend && m_rgbOnlyBlend);
    // The reveal-blur veil (fx::RevealBlur) rides on the same device/backbuffer
    // the glass uses, so it is owned here: every menu gets working
    // blur reveals without its host having to know about them.
    if (m_initialized) BackdropBlurInit(device, width, height);
    return m_initialized;
}

inline void GlassRenderer::Shutdown() {
    BackdropBlurShutdown();
    m_blur.Shutdown();
    m_dsBlur.Shutdown();
    m_gradient.Shutdown();
    GlassCompositeShutdown();
    backend::SafeRelease(m_captureSRV); backend::SafeRelease(m_captureRTV); backend::SafeRelease(m_captureTex);
    backend::SafeRelease(m_copyVS); backend::SafeRelease(m_copyPS); backend::SafeRelease(m_copySamp);
    backend::SafeRelease(m_copyRast); backend::SafeRelease(m_copyBlend);
    backend::SafeRelease(m_rgbOnlyBlend); backend::SafeRelease(m_copyDepth);
    backend::SafeRelease(m_premultSRV); backend::SafeRelease(m_premultRTV); backend::SafeRelease(m_premultTex);
    backend::SafeRelease(m_premultPS);
    backend::SafeRelease(m_dsSRV); backend::SafeRelease(m_dsRTV); backend::SafeRelease(m_dsTex);
    backend::SafeRelease(m_dsPS);
    backend::SafeRelease(m_ctx);
    m_initialized = false;
}

inline void GlassRenderer::Resize(int width, int height) {
    m_width  = width;
    m_height = height;
    m_captureFmt = DXGI_FORMAT_UNKNOWN;
    backend::SafeRelease(m_captureSRV); backend::SafeRelease(m_captureRTV); backend::SafeRelease(m_captureTex);
    m_captureTex = backend::CreateTexture2D(m_device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                             D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_captureTex) {
        m_captureRTV = backend::CreateRTV(m_device, m_captureTex);
        m_captureSRV = backend::CreateSRV(m_device, m_captureTex);
    }
    backend::SafeRelease(m_premultSRV); backend::SafeRelease(m_premultRTV); backend::SafeRelease(m_premultTex);
    m_premultTex = backend::CreateTexture2D(m_device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                             D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_premultTex) {
        m_premultRTV = backend::CreateRTV(m_device, m_premultTex);
        m_premultSRV = backend::CreateSRV(m_device, m_premultTex);
    }
    backend::SafeRelease(m_dsSRV); backend::SafeRelease(m_dsRTV); backend::SafeRelease(m_dsTex);
    const int dsW = (std::max)(1, width / m_downsampleFactor);
    const int dsH = (std::max)(1, height / m_downsampleFactor);
    m_dsTex = backend::CreateTexture2D(m_device, dsW, dsH, DXGI_FORMAT_R8G8B8A8_UNORM,
                                        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_dsTex) {
        m_dsRTV = backend::CreateRTV(m_device, m_dsTex);
        m_dsSRV = backend::CreateSRV(m_device, m_dsTex);
    }
    m_blur.Resize(m_device, width, height);
    m_dsBlur.Resize(m_device, dsW, dsH);
}

inline void GlassRenderer::BeginFrame(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* backbufferRTV, float dt) {
    if (!m_initialized || !backbufferRTV) return;
    ID3D11DeviceContext* dc = ctx ? ctx : m_ctx;
    m_frameRan = false;

    m_gradient.Update(dt);

    // 1. Populate the capture texture.
    if (m_overlayMode) {
        // Overlay: grab the existing backbuffer (the finished game frame) so the
        // glass frosts the real scene instead of painting a backdrop over it.
        ID3D11Resource* bbRes = nullptr;
        backbufferRTV->GetResource(&bbRes);
        ID3D11Texture2D* bbTex = nullptr;
        if (bbRes && SUCCEEDED(bbRes->QueryInterface(IID_PPV_ARGS(&bbTex)))) {
            D3D11_TEXTURE2D_DESC bbDesc = {};
            bbTex->GetDesc(&bbDesc);
            if (!m_captureTex || m_captureFmt != bbDesc.Format ||
                m_width != (int)bbDesc.Width || m_height != (int)bbDesc.Height) {
                backend::SafeRelease(m_captureSRV); backend::SafeRelease(m_captureRTV); backend::SafeRelease(m_captureTex);
                m_captureTex = backend::CreateTexture2D(m_device, (int)bbDesc.Width, (int)bbDesc.Height, bbDesc.Format,
                                                        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
                if (m_captureTex) {
                    m_captureRTV = backend::CreateRTV(m_device, m_captureTex);
                    m_captureSRV = backend::CreateSRV(m_device, m_captureTex);
                }
                m_captureFmt = bbDesc.Format;
            }
            if (m_captureTex) dc->CopyResource(m_captureTex, bbTex);
            bbTex->Release();
        }
        if (bbRes) bbRes->Release();
    } else if (m_bgImageSRV && m_bgMode >= 8) {
        // Blit image texture to capture RTV
        D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        dc->RSSetState(m_copyRast);
        dc->OMSetBlendState(m_copyBlend, nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(m_copyDepth, 0);
        dc->OMSetRenderTargets(1, &m_captureRTV, nullptr);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->VSSetShader(m_copyVS, nullptr, 0);
        dc->PSSetShader(m_copyPS, nullptr, 0);
        dc->PSSetShaderResources(0, 1, &m_bgImageSRV);
        dc->PSSetSamplers(0, 1, &m_copySamp);
        dc->Draw(3, 0);
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullSRV);
    } else {
        m_gradient.Render(dc, m_captureRTV);
    }

    // 2. Overlay mode presents the sharp capture. Window-glass mode waits for
    // the Gaussian result below; leaving the sharp copy underneath allowed it
    // to leak through translucent root panels and read as "no blur".
    if (!m_presentBlurredBackground && !m_overlayMode) {
        D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        dc->RSSetState(m_copyRast);
        dc->OMSetBlendState(m_copyBlend, nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(m_copyDepth, 0);
        dc->OMSetRenderTargets(1, &backbufferRTV, nullptr);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->VSSetShader(m_copyVS, nullptr, 0);
        dc->PSSetShader(m_copyPS, nullptr, 0);
        dc->PSSetShaderResources(0, 1, &m_captureSRV);
        dc->PSSetSamplers(0, 1, &m_copySamp);
        dc->Draw(3, 0);
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullSRV);
    }

    // 3. Premultiply alpha: capture → premult (RGB *= A)
    ID3D11ShaderResourceView* blurInput = m_captureSRV;
    if (m_premultEnabled && m_premultPS) {
        D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        dc->RSSetState(m_copyRast);
        if (m_forceOpaqueBackground) {
            const float opaqueClear[4] = { 0, 0, 0, 1 };
            dc->ClearRenderTargetView(m_premultRTV, opaqueClear);
            // Desktop Duplication commonly reports zero/undefined alpha.
            // Copy only RGB so the alpha=1 clear survives for DComp.
            dc->OMSetBlendState(m_rgbOnlyBlend, nullptr, 0xFFFFFFFF);
        } else {
            dc->OMSetBlendState(m_copyBlend, nullptr, 0xFFFFFFFF);
        }
        dc->OMSetDepthStencilState(m_copyDepth, 0);
        dc->OMSetRenderTargets(1, &m_premultRTV, nullptr);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->VSSetShader(m_copyVS, nullptr, 0);
        dc->PSSetShader(m_forceOpaqueBackground ? m_copyPS : m_premultPS, nullptr, 0);
        dc->PSSetShaderResources(0, 1, &m_captureSRV);
        dc->PSSetSamplers(0, 1, &m_copySamp);
        dc->Draw(3, 0);
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullSRV);
        blurInput = m_premultSRV;
    }

    // 4. Gaussian blur. Loader desktop snapshots are blurred in monitor
    // coordinates before the moving window crop is taken, so running another
    // window-local pass would reintroduce edge clamp and downsample shimmer.
    if (m_backgroundPreblurred) {
        m_finalBlurSRV = blurInput;
    } else if (m_downsampleBlur && m_dsRTV) {
        const float dsW = (float)(std::max)(1, m_width / m_downsampleFactor);
        const float dsH = (float)(std::max)(1, m_height / m_downsampleFactor);
        D3D11_VIEWPORT vpDs = { 0, 0, dsW, dsH, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vpDs);
        dc->RSSetState(m_copyRast);
        dc->OMSetBlendState(m_copyBlend, nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(m_copyDepth, 0);
        dc->OMSetRenderTargets(1, &m_dsRTV, nullptr);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->VSSetShader(m_copyVS, nullptr, 0);
        dc->PSSetShader(m_copyPS, nullptr, 0);
        dc->PSSetShaderResources(0, 1, &blurInput);
        dc->PSSetSamplers(0, 1, &m_copySamp);
        dc->Draw(3, 0);
        ID3D11ShaderResourceView* nullDs[1] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullDs);

        // Preserve the requested screen-space footprint at any configured
        // downsample factor. Loader windows use a smaller field than overlays
        // so detailed desktop captures cannot read as simple transparency.
        const float scaledRadius = m_blurRadius / (float)m_downsampleFactor;
        m_dsBlur.Execute(dc, m_dsSRV, &m_finalBlurSRV, scaledRadius);
        for (int pass = 1; pass < m_blurPasses && m_finalBlurSRV; ++pass)
            m_dsBlur.Execute(dc, m_finalBlurSRV, &m_finalBlurSRV, scaledRadius);
    } else {
        m_blur.Execute(dc, blurInput, &m_finalBlurSRV, m_blurRadius);
        for (int pass = 1; pass < m_blurPasses && m_finalBlurSRV; ++pass)
            m_blur.Execute(dc, m_finalBlurSRV, &m_finalBlurSRV, m_blurRadius);
    }

    // 5. Window mode: present the final Gaussian result as the backdrop.
    // Overlay callers leave this disabled and sample it only from panels.
    if (m_presentBlurredBackground && m_finalBlurSRV) {
        D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        dc->RSSetState(m_copyRast);
        dc->OMSetBlendState(m_copyBlend, nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(m_copyDepth, 0);
        dc->OMSetRenderTargets(1, &backbufferRTV, nullptr);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->VSSetShader(m_copyVS, nullptr, 0);
        dc->PSSetShader(m_copyPS, nullptr, 0);
        dc->PSSetShaderResources(0, 1, &m_finalBlurSRV);
        dc->PSSetSamplers(0, 1, &m_copySamp);
        dc->Draw(3, 0);
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullSRV);
    }

    // 6. Restore backbuffer RTV
    dc->OMSetRenderTargets(1, &backbufferRTV, nullptr);
    m_frameRan = true;
}

} // namespace opal::glass
