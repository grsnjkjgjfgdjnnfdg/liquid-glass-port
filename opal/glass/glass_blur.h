#pragma once
#include <d3d11.h>
#include "../backend/dx11_resources.h"
#include "../backend/dx11_shaders.h"

namespace opal::glass {

class GaussianBlur {
public:
    bool Init(ID3D11Device* device, int width, int height);
    void Shutdown();
    void Resize(ID3D11Device* device, int width, int height);
    void Execute(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* inputSRV,
                 ID3D11ShaderResourceView** outSRV, float radius);

private:
    ID3D11Device*        m_device    = nullptr;
    ID3D11DeviceContext* m_ctx       = nullptr;

    ID3D11Texture2D*          m_pingTex = nullptr;
    ID3D11Texture2D*          m_pongTex = nullptr;
    ID3D11RenderTargetView*   m_pingRTV = nullptr;
    ID3D11RenderTargetView*   m_pongRTV = nullptr;
    ID3D11ShaderResourceView* m_pingSRV = nullptr;
    ID3D11ShaderResourceView* m_pongSRV = nullptr;

    ID3D11VertexShader*       m_vs      = nullptr;
    ID3D11PixelShader*        m_psH     = nullptr;
    ID3D11PixelShader*        m_psV     = nullptr;
    ID3D11SamplerState*       m_sampler = nullptr;
    ID3D11Buffer*             m_cb      = nullptr;

    ID3D11RasterizerState*    m_rast    = nullptr;
    ID3D11BlendState*         m_blend   = nullptr;
    ID3D11DepthStencilState*  m_depth   = nullptr;

    int  m_width  = 0;
    int  m_height = 0;
    bool m_initialized = false;
};

inline bool GaussianBlur::Init(ID3D11Device* device, int width, int height) {
    m_device = device;
    m_width  = width;
    m_height = height;
    device->GetImmediateContext(&m_ctx);

    m_pingTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    m_pongTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_pingTex) { m_pingRTV = backend::CreateRTV(device, m_pingTex); m_pingSRV = backend::CreateSRV(device, m_pingTex); }
    if (m_pongTex) { m_pongRTV = backend::CreateRTV(device, m_pongTex); m_pongSRV = backend::CreateSRV(device, m_pongTex); }

    m_vs  = backend::CreateFullscreenVS(device);
    m_psH = backend::CreateBlurHPS(device);
    m_psV = backend::CreateBlurVPS(device);

    D3D11_SAMPLER_DESC samp = {};
    samp.Filter         = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samp.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.MaxAnisotropy  = 1;
    samp.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samp.MaxLOD         = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&samp, &m_sampler);

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = 16;
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbDesc, nullptr, &m_cb);

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

    m_initialized = (m_pingTex && m_pongTex && m_vs && m_psH && m_psV);
    return m_initialized;
}

inline void GaussianBlur::Shutdown() {
    backend::SafeRelease(m_pingSRV); backend::SafeRelease(m_pongSRV);
    backend::SafeRelease(m_pingRTV); backend::SafeRelease(m_pongRTV);
    backend::SafeRelease(m_pingTex); backend::SafeRelease(m_pongTex);
    backend::SafeRelease(m_vs); backend::SafeRelease(m_psH); backend::SafeRelease(m_psV);
    backend::SafeRelease(m_sampler); backend::SafeRelease(m_cb);
    backend::SafeRelease(m_rast); backend::SafeRelease(m_blend); backend::SafeRelease(m_depth);
    backend::SafeRelease(m_ctx);
    m_initialized = false;
}

inline void GaussianBlur::Resize(ID3D11Device* device, int width, int height) {
    backend::SafeRelease(m_pingSRV); backend::SafeRelease(m_pongSRV);
    backend::SafeRelease(m_pingRTV); backend::SafeRelease(m_pongRTV);
    backend::SafeRelease(m_pingTex); backend::SafeRelease(m_pongTex);
    m_width  = width;
    m_height = height;
    m_pingTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    m_pongTex = backend::CreateTexture2D(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (m_pingTex) { m_pingRTV = backend::CreateRTV(device, m_pingTex); m_pingSRV = backend::CreateSRV(device, m_pingTex); }
    if (m_pongTex) { m_pongRTV = backend::CreateRTV(device, m_pongTex); m_pongSRV = backend::CreateSRV(device, m_pongTex); }
}

inline void GaussianBlur::Execute(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* inputSRV,
                                   ID3D11ShaderResourceView** outSRV, float radius) {
    if (!m_initialized) return;
    ID3D11DeviceContext* dc = ctx ? ctx : m_ctx;

    // Match the established menu kernel: a full-resolution,
    // normalized separable Gaussian pass every frame. The tap count stays
    // bounded so the requested 15px loader blur remains stable under v-sync.
    const float sigma = radius < 0.5f ? 0.5f : radius;
    int tapCount = (int)(sigma * 2.0f + 0.999f);
    if (tapCount < 1) tapCount = 1;
    int numTaps = tapCount * 2 + 1;
    if (numTaps < 3) numTaps = 3;
    if (numTaps > 15) numTaps = 15;
    float cbData[4] = { 1.0f / (float)m_width, 1.0f / (float)m_height, sigma, (float)numTaps };
    D3D11_MAPPED_SUBRESOURCE mapped;
    dc->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, cbData, sizeof(cbData));
    dc->Unmap(m_cb, 0);

    D3D11_VIEWPORT vp = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
    dc->RSSetViewports(1, &vp);
    dc->RSSetState(m_rast);
    dc->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(m_depth, 0);
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    dc->VSSetShader(m_vs, nullptr, 0);
    dc->PSSetSamplers(0, 1, &m_sampler);
    dc->PSSetConstantBuffers(0, 1, &m_cb);

    // H pass: inputSRV → pongRTV
    dc->OMSetRenderTargets(1, &m_pongRTV, nullptr);
    dc->PSSetShader(m_psH, nullptr, 0);
    ID3D11ShaderResourceView* srvsH[1] = { inputSRV };
    dc->PSSetShaderResources(0, 1, srvsH);
    dc->Draw(3, 0);

    // V pass: pongSRV → pingRTV
    dc->OMSetRenderTargets(1, &m_pingRTV, nullptr);
    dc->PSSetShader(m_psV, nullptr, 0);
    ID3D11ShaderResourceView* srvsV[1] = { m_pongSRV };
    dc->PSSetShaderResources(0, 1, srvsV);
    dc->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    dc->PSSetShaderResources(0, 1, nullSRV);

    *outSRV = m_pingSRV;
}

} // namespace opal::glass
