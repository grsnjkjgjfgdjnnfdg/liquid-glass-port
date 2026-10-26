#pragma once
#include <d3d11.h>
#include "../backend/dx11_shaders.h"
#include "../backend/dx11_resources.h"
#include "../opal_types.h"

namespace opal::glass {

class GradientBackground {
public:
    bool Init(ID3D11Device* device, int width, int height);
    void Shutdown();
    void Update(float dt);
    void Render(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv);
    void SetMode(int m)          { m_mode = m; }
    void SetColors(Color c1, Color c2) { m_color1 = c1; m_color2 = c2; }
    int  GetMode() const         { return m_mode; }

private:
    ID3D11Device*        m_device  = nullptr;
    ID3D11VertexShader*  m_vs      = nullptr;
    ID3D11PixelShader*   m_ps      = nullptr;
    ID3D11SamplerState*  m_sampler = nullptr;
    ID3D11Buffer*        m_cb      = nullptr;
    ID3D11RasterizerState*    m_rast  = nullptr;
    ID3D11BlendState*         m_blend = nullptr;
    ID3D11DepthStencilState*  m_depth = nullptr;
    int   m_width  = 0;
    int   m_height = 0;
    float m_time   = 0.0f;
    bool  m_initialized = false;
    int   m_mode    = 0;
    Color m_color1  = Color(0.12f, 0.12f, 0.18f, 1);
    Color m_color2  = Color(0.06f, 0.06f, 0.12f, 1);
};

inline bool GradientBackground::Init(ID3D11Device* device, int width, int height) {
    m_device = device;
    m_width  = width;
    m_height = height;

    m_vs = backend::CreateFullscreenVS(device);
    m_ps = backend::CreateGradientPS(device);
    m_sampler = backend::CreateSampler(device);
    m_cb = backend::CreateConstantBuffer(device, 48);

    D3D11_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D11_FILL_SOLID;
    rast.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rast, &m_rast);

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &m_blend);

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    device->CreateDepthStencilState(&depth, &m_depth);

    m_initialized = (m_vs && m_ps);
    return m_initialized;
}

inline void GradientBackground::Shutdown() {
    backend::SafeRelease(m_vs); backend::SafeRelease(m_ps);
    backend::SafeRelease(m_sampler);
    backend::SafeRelease(m_cb);
    backend::SafeRelease(m_rast); backend::SafeRelease(m_blend); backend::SafeRelease(m_depth);
    m_initialized = false;
}

inline void GradientBackground::Update(float dt) { m_time += dt; }

inline void GradientBackground::Render(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv) {
    if (!m_initialized) return;

    float cb[12] = {
        m_color1.r, m_color1.g, m_color1.b, m_color1.a,
        m_color2.r, m_color2.g, m_color2.b, m_color2.a,
        (float)m_mode, m_time, 0, 0
    };
    D3D11_MAPPED_SUBRESOURCE m;
    if (m_cb) {
        ctx->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        memcpy(m.pData, cb, 48);
        ctx->Unmap(m_cb, 0);
    }

    D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(m_rast);
    ctx->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_depth, 0);
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &m_sampler);
    if (m_cb) ctx->PSSetConstantBuffers(0, 1, &m_cb);
    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);
}

} // namespace opal::glass
