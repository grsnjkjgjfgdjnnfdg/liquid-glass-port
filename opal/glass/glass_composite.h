#pragma once
#include <d3d11.h>
#include <cstring>
#include <algorithm>
#include "../imgui/imgui.h"
#include "../opal_types.h"
#include "../backend/dx11_shaders.h"
#include "../backend/dx11_resources.h"

struct ImDrawList;

namespace opal::glass {

struct GlassPanelData {
    ID3D11ShaderResourceView* blurSRV;
    ID3D11ShaderResourceView* captureSRV;
    float panelX, panelY, panelW, panelH;
    float cbData[32];
    bool  clipSnap = false;
    float clipX = 0, clipY = 0, clipW = 0, clipH = 0;
};

struct GlassCompositeState {
    ID3D11Device*        device       = nullptr;
    ID3D11DeviceContext* ctx          = nullptr;
    ID3D11PixelShader*   compositePS  = nullptr;
    ID3D11VertexShader*  fullscreenVS = nullptr;
    ID3D11SamplerState*  sampler      = nullptr;
    ID3D11Buffer*        glassCB      = nullptr;
    ID3D11RasterizerState* rastState  = nullptr;
    int   width     = 0;
    int   height    = 0;
    float raycastStrength = 0.0f;
    float hdrStrength     = 0.0f;
    bool  initialized = false;
    bool  clipEnabled = false;
    float clipX = 0, clipY = 0, clipW = 0, clipH = 0;
    struct ClipEntry { float x, y, w, h; };
    ClipEntry clipStack[8];
    int clipStackDepth = 0;
};

struct GlassParams {
    ID3D11ShaderResourceView* blurSRV      = nullptr;
    ID3D11ShaderResourceView* captureSRV   = nullptr;
    Color  tintColor         = Color(1, 1, 1, 0.3f);
    float  tintStrength      = 0.3f;
    float  glassAlpha        = 0.85f;
    int    screenW           = 1920;
    int    screenH           = 1080;
    Rect   panelRect         = Rect(0, 0, 0, 0);
    float  panelRadius       = 24.0f;
    float  fresnelPower      = 1.0f;
    float  chromaticStrength = 0.5f;
    float  grainStrength     = 0.005f;
    float  saturation        = 1.15f;
    float  time              = 0.0f;
    float  refraction        = 0.25f;
    float  innerShadow       = 0.35f;
    float  specularStrength  = 0.4f;
    float  lensThickness     = 0.5f;
    // Negative inherits the renderer-wide feature strength. Large/root glass
    // can opt out so screen-space desktop lines remain geometrically stable.
    float  raycastStrength   = -1.0f;
    Vec2   mousePos          = Vec2(-9999.0f, -9999.0f);
    float  mouseStrength     = 0.0f;
};

inline GlassCompositeState g_glass;

inline bool GlassCompositeInit(ID3D11Device* device, int width, int height) {
    g_glass.device = device;
    g_glass.width  = width;
    g_glass.height = height;
    device->GetImmediateContext(&g_glass.ctx);

    g_glass.compositePS  = backend::CreateCompositePS(device);
    g_glass.fullscreenVS = backend::CreateFullscreenVS(device);
    g_glass.sampler      = backend::CreateSampler(device);
    g_glass.glassCB      = backend::CreateConstantBuffer(device, 128);

    D3D11_RASTERIZER_DESC rast = {};
    rast.FillMode              = D3D11_FILL_SOLID;
    rast.CullMode              = D3D11_CULL_NONE;
    rast.ScissorEnable         = TRUE;
    device->CreateRasterizerState(&rast, &g_glass.rastState);

    g_glass.initialized = (g_glass.compositePS && g_glass.fullscreenVS
                        && g_glass.sampler && g_glass.glassCB && g_glass.rastState);
    return g_glass.initialized;
}

inline void GlassCompositeShutdown() {
    backend::SafeRelease(g_glass.compositePS);
    backend::SafeRelease(g_glass.fullscreenVS);
    backend::SafeRelease(g_glass.sampler);
    backend::SafeRelease(g_glass.glassCB);
    backend::SafeRelease(g_glass.rastState);
    backend::SafeRelease(g_glass.ctx);
    g_glass.initialized = false;
}

inline void GlassSetClip(float x, float y, float w, float h) {
    if (g_glass.clipStackDepth < 8) {
        g_glass.clipStack[g_glass.clipStackDepth] = { g_glass.clipX, g_glass.clipY, g_glass.clipW, g_glass.clipH };
        g_glass.clipStackDepth++;
    }
    if (g_glass.clipEnabled) {
        float ix0 = (std::max)(x, g_glass.clipX);
        float iy0 = (std::max)(y, g_glass.clipY);
        float ix1 = (std::min)(x + w, g_glass.clipX + g_glass.clipW);
        float iy1 = (std::min)(y + h, g_glass.clipY + g_glass.clipH);
        g_glass.clipX = ix0; g_glass.clipY = iy0;
        g_glass.clipW = (std::max)(0.0f, ix1 - ix0);
        g_glass.clipH = (std::max)(0.0f, iy1 - iy0);
    } else {
        g_glass.clipX = x; g_glass.clipY = y; g_glass.clipW = w; g_glass.clipH = h;
    }
    g_glass.clipEnabled = true;
}

inline void GlassClearClip() {
    if (g_glass.clipStackDepth > 0) {
        g_glass.clipStackDepth--;
        auto& e = g_glass.clipStack[g_glass.clipStackDepth];
        g_glass.clipX = e.x; g_glass.clipY = e.y; g_glass.clipW = e.w; g_glass.clipH = e.h;
        g_glass.clipEnabled = (g_glass.clipStackDepth > 0 || e.w > 0.0f);
    } else {
        g_glass.clipEnabled = false;
    }
}

inline void GlassBeginPanel(ImDrawList* dl, const GlassParams& params) {
    if (!g_glass.initialized) return;

    auto* data = new GlassPanelData();
    data->blurSRV    = params.blurSRV;
    data->captureSRV = params.captureSRV;
    data->panelX     = params.panelRect.x;
    data->panelY     = params.panelRect.y;
    data->panelW     = params.panelRect.w;
    data->panelH     = params.panelRect.h;

    memset(data->cbData, 0, sizeof(data->cbData));
    data->cbData[0]  = params.tintColor.r;
    data->cbData[1]  = params.tintColor.g;
    data->cbData[2]  = params.tintColor.b;
    data->cbData[3]  = params.tintColor.a;
    data->cbData[4]  = params.tintStrength;
    data->cbData[5]  = params.glassAlpha;
    data->cbData[6]  = (float)params.screenW;
    data->cbData[7]  = (float)params.screenH;
    data->cbData[8]  = params.panelRect.x;
    data->cbData[9]  = params.panelRect.y;
    data->cbData[10] = params.panelRect.w;
    data->cbData[11] = params.panelRect.h;
    data->cbData[12] = params.panelRadius;
    data->cbData[13] = params.fresnelPower;
    data->cbData[14] = params.chromaticStrength;
    data->cbData[15] = params.grainStrength;
    data->cbData[16] = params.saturation;
    data->cbData[17] = params.time;
    data->cbData[18] = params.refraction;
    data->cbData[19] = params.innerShadow;
    data->cbData[20] = params.specularStrength;
    data->cbData[21] = params.lensThickness;
    data->cbData[22] = params.raycastStrength >= 0.0f
        ? params.raycastStrength : g_glass.raycastStrength;
    data->cbData[23] = g_glass.hdrStrength;
    data->cbData[24] = params.mousePos.x;
    data->cbData[25] = params.mousePos.y;
    data->cbData[26] = params.mouseStrength;
    data->cbData[27] = 0.0f;

    data->clipSnap = g_glass.clipEnabled;
    if (data->clipSnap) {
        data->clipX = g_glass.clipX;
        data->clipY = g_glass.clipY;
        data->clipW = g_glass.clipW;
        data->clipH = g_glass.clipH;
    }

    dl->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd) {
        auto* d = (GlassPanelData*)cmd->UserCallbackData;
        ID3D11DeviceContext* dc = g_glass.ctx;
        if (!dc) { delete d; return; }

        ID3D11VertexShader* oldVS = nullptr;
        ID3D11PixelShader* oldPS = nullptr;
        dc->VSGetShader(&oldVS, nullptr, nullptr);
        dc->PSGetShader(&oldPS, nullptr, nullptr);

        UINT vpCount = 1;
        D3D11_VIEWPORT oldVP;
        dc->RSGetViewports(&vpCount, &oldVP);

        ID3D11RasterizerState* oldRS = nullptr;
        dc->RSGetState(&oldRS);

        D3D11_PRIMITIVE_TOPOLOGY oldTopo;
        dc->IAGetPrimitiveTopology(&oldTopo);

        ID3D11InputLayout* oldLayout = nullptr;
        dc->IAGetInputLayout(&oldLayout);

        ID3D11ShaderResourceView* oldPSResources[2] = { nullptr, nullptr };
        ID3D11SamplerState* oldPSSampler = nullptr;
        ID3D11Buffer* oldPSConstantBuffer = nullptr;
        dc->PSGetShaderResources(1, 2, oldPSResources);
        dc->PSGetSamplers(0, 1, &oldPSSampler);
        dc->PSGetConstantBuffers(0, 1, &oldPSConstantBuffer);

        UINT oldScissorCount = 0;
        D3D11_RECT oldScissor[1] = {};
        dc->RSGetScissorRects(&oldScissorCount, nullptr);
        if (oldScissorCount > 0 && oldScissorCount <= 1)
            dc->RSGetScissorRects(&oldScissorCount, oldScissor);

        D3D11_MAPPED_SUBRESOURCE m;
        dc->Map(g_glass.glassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        memcpy(m.pData, d->cbData, 128);
        dc->Unmap(g_glass.glassCB, 0);

        constexpr float kVpPad = 6.0f; // Padding so SDF 3px anti-aliased edge ramp is never clipped by viewport bounds
        float vx = d->panelX - kVpPad;
        float vy = d->panelY - kVpPad;
        float vw = d->panelW + kVpPad * 2.0f;
        float vh = d->panelH + kVpPad * 2.0f;

        float screenW = d->cbData[6] > 0.0f ? d->cbData[6] : 4096.0f;
        float screenH = d->cbData[7] > 0.0f ? d->cbData[7] : 2160.0f;

        if (vx < 0.0f) { vw += vx; vx = 0.0f; }
        if (vy < 0.0f) { vh += vy; vy = 0.0f; }
        if (vx + vw > screenW) vw = screenW - vx;
        if (vy + vh > screenH) vh = screenH - vy;

        D3D11_VIEWPORT vp = { vx, vy, vw, vh, 0.0f, 1.0f };
        if (d->clipSnap) {
            float cx0 = (std::max)(vp.TopLeftX, d->clipX);
            float cy0 = (std::max)(vp.TopLeftY, d->clipY);
            float cx1 = (std::min)(vp.TopLeftX + vp.Width, d->clipX + d->clipW);
            float cy1 = (std::min)(vp.TopLeftY + vp.Height, d->clipY + d->clipH);
            if (cx1 <= cx0 || cy1 <= cy0) {
                dc->VSSetShader(oldVS, nullptr, 0);
                dc->PSSetShader(oldPS, nullptr, 0);
                dc->RSSetViewports(1, &oldVP);
                dc->RSSetState(oldRS);
                if (oldScissorCount > 0) dc->RSSetScissorRects(oldScissorCount, oldScissor);
                dc->IASetPrimitiveTopology(oldTopo);
                dc->IASetInputLayout(oldLayout);
                dc->PSSetShaderResources(1, 2, oldPSResources);
                dc->PSSetSamplers(0, 1, &oldPSSampler);
                dc->PSSetConstantBuffers(0, 1, &oldPSConstantBuffer);
                if (oldVS) oldVS->Release();
                if (oldPS) oldPS->Release();
                if (oldRS) oldRS->Release();
                if (oldLayout) oldLayout->Release();
                if (oldPSResources[0]) oldPSResources[0]->Release();
                if (oldPSResources[1]) oldPSResources[1]->Release();
                if (oldPSSampler) oldPSSampler->Release();
                if (oldPSConstantBuffer) oldPSConstantBuffer->Release();
                delete d; return;
            }
            vp.TopLeftX = cx0; vp.TopLeftY = cy0;
            vp.Width = cx1 - cx0; vp.Height = cy1 - cy0;
        }
        dc->RSSetViewports(1, &vp);
        dc->RSSetState(g_glass.rastState);

        if (d->clipSnap) {
            D3D11_RECT sr = { (LONG)d->clipX, (LONG)d->clipY,
                              (LONG)(d->clipX + d->clipW), (LONG)(d->clipY + d->clipH) };
            dc->RSSetScissorRects(1, &sr);
        }

        dc->VSSetShader(g_glass.fullscreenVS, nullptr, 0);
        dc->PSSetShader(g_glass.compositePS, nullptr, 0);
        dc->PSSetShaderResources(1, 1, &d->blurSRV);
        dc->PSSetShaderResources(2, 1, &d->captureSRV);
        dc->PSSetSamplers(0, 1, &g_glass.sampler);
        dc->PSSetConstantBuffers(0, 1, &g_glass.glassCB);
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        dc->Draw(3, 0);

        dc->VSSetShader(oldVS, nullptr, 0);
        dc->PSSetShader(oldPS, nullptr, 0);
        dc->RSSetViewports(1, &oldVP);
        dc->RSSetState(oldRS);
        if (oldScissorCount > 0) dc->RSSetScissorRects(oldScissorCount, oldScissor);
        dc->IASetPrimitiveTopology(oldTopo);
        dc->IASetInputLayout(oldLayout);
        dc->PSSetShaderResources(1, 2, oldPSResources);
        dc->PSSetSamplers(0, 1, &oldPSSampler);
        dc->PSSetConstantBuffers(0, 1, &oldPSConstantBuffer);
        if (oldVS)     oldVS->Release();
        if (oldPS)     oldPS->Release();
        if (oldRS)     oldRS->Release();
        if (oldLayout) oldLayout->Release();
        if (oldPSResources[0]) oldPSResources[0]->Release();
        if (oldPSResources[1]) oldPSResources[1]->Release();
        if (oldPSSampler) oldPSSampler->Release();
        if (oldPSConstantBuffer) oldPSConstantBuffer->Release();

        delete d;
    }, data);
}

inline void GlassEndPanel(ImDrawList* dl) {
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

} // namespace opal::glass
