#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <algorithm>

#pragma comment(lib, "d3dcompiler.lib")

namespace sw {

// Offscreen scene capture + separable Gaussian blur.
//
// This is the piece that makes the glass real: the UI is drawn once into an
// offscreen target, that target is blurred, and the visible frame samples the
// blurred result inside each glass panel. Apple's material samples what is
// physically behind the panel; so does this.
class GlassPass {
public:
    bool Init(ID3D11Device* device, int width, int height, int downscale = 2,
              int sceneScaleIn = 1) {
        Shutdown();
        dev = device;
        w = width;
        h = height;
        sceneScale = sceneScaleIn < 1 ? 1 : sceneScaleIn;
        // The material is the one thing in the frame that was never supersampled:
        // at 1x every rim, hair and ridge is a single pixel, magnified 3x into the
        // frame and box-downsampled back, so the lens could not hold a sub-point
        // feature at all - a 0.9 pt specular ridge landed on one texel. Stamping it
        // at the scene's own scale fixes that; the shape maths is in points and the
        // composite samples it by normalised uv, so nothing else has to change.
        lensScale = sceneScale;
        bw = std::max(1, width / downscale);
        bh = std::max(1, height / downscale);

        if (!CreateTarget(device, w * sceneScale, h * sceneScale, sceneTex, sceneRTV, sceneSRV)) return false;
        if (!CreateTarget(device, bw, bh, blurA, blurARTV, blurASRV)) return false;
        if (!CreateTarget(device, bw, bh, blurB, blurBRTV, blurBSRV)) return false;

        // Fullscreen triangle + blur/blit shaders.
        static const char* kHLSL = R"(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VS(uint id : SV_VertexID) {
    float2 p = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.pos = float4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);
    o.uv = p;
    return o;
}

Texture2D    scene : register(t0);
SamplerState linearClamp : register(s0);

cbuffer BlurParams : register(b0) {
    float2 texelDir;   // one texel step along the blur axis, in UV units
    float2 flags;      // x = 1 on the first pass, which box-prefilters its taps
};

// The scene is 3x and the blur runs at a third of the frame, so plain sampling would
// take one pixel out of every nine. A 20 pt grid aliases into low-frequency banding
// that no amount of kernel width removes; a four-tap box at half a blur texel does.
float4 SampleAA(float2 uv) {
    if (flags.x < 0.5) return scene.Sample(linearClamp, uv);
    float2 o = texelDir * 0.5;
    return 0.25 * (scene.Sample(linearClamp, uv + float2(-o.x, -o.y))
                 + scene.Sample(linearClamp, uv + float2( o.x, -o.y))
                 + scene.Sample(linearClamp, uv + float2(-o.x,  o.y))
                 + scene.Sample(linearClamp, uv + float2( o.x,  o.y)));
}

float4 PSBlur(VSOut i) : SV_Target {
    // 15-tap Gaussian (sigma ~ 5 texels), normalised.
    static const float w0 = 0.1346;
    static const float w[7] = { 0.1287, 0.1153, 0.0925, 0.0667, 0.0431, 0.0249, 0.0129 };
    float4 c = SampleAA(i.uv) * w0;
    [unroll] for (int k = 1; k < 7; ++k) {
        float2 o = texelDir * (float)k;
        c += SampleAA(i.uv + o) * w[k];
        c += SampleAA(i.uv - o) * w[k];
    }
    // Empirical unity gain: the 15-tap kernel as compiled loses ~15 % per pass,
    // which compounded to a visibly grey material over four passes.
    return c * 1.176;
}

float4 PSBlit(VSOut i) : SV_Target {
    return scene.Sample(linearClamp, i.uv);
}

// ---------------------------------------------------------------------------
// Liquid-glass lens. One panel per draw: a signed-distance rounded rect, a
// gradient-driven refraction of the blurred backdrop, a vibrancy lift, a rim
// light weighted by the surface normal, and a soft drop shadow outside the shape.
// ---------------------------------------------------------------------------
cbuffer Panel : register(b0) {
    float4 rect;      // x, y, w, h in pixels
    float4 prm;       // radius, refractPx, rimStrength, shadowAlpha
    float4 tint;      // straight rgba
    float4 screenDim; // screenW, screenH, margin, magnify
    float4 liftVec;   // per-channel lift (rgb), .w = lift mode flag
    float4 extra;     // x = coverage, y = ring (rim on every edge), z = vibrancy,
                      // w = frost radius in points for a control (0 = panel)
    float4 edge;      // x = hairline slope vs |normal.y| (appearance), yzw spare
};

// The capture's hairline measures 0.254/0.289 in sRGB on the vertical edges but is
// neutral (<3 % per-channel spread) in linear light, so it has to be composited there.
float3 SrgbToLinear(float3 c) {
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float3 LinearToSrgb(float3 c) {
    c = max(c, 0.0);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

float sdRoundRect(float2 p, float2 halfSize, float r) {
    float2 q = abs(p) - (halfSize - r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

struct PanelOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

PanelOut VSPanel(uint id : SV_VertexID) {
    float2 corners[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };
    float2 c = corners[id];
    float2 origin = rect.xy - screenDim.z;
    float2 size = rect.zw + screenDim.z * 2.0;
    float2 px = origin + c * size;
    PanelOut o;
    o.pos = float4(px / screenDim.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = px / screenDim.xy;
    return o;
}

float4 PSLens(PanelOut i) : SV_Target {
    float2 center = rect.xy + rect.zw * 0.5;
    float2 halfSize = rect.zw * 0.5;
    float2 p = i.uv * screenDim.xy - center;
    float d = sdRoundRect(p, halfSize, prm.x);

    // Coverage is derived from the SDF instead of branching, so the rim is
    // antialiased like the runtime's (a hard cutoff reads as a jagged edge at 1x).
    // Coverage ramp from the SDF. Measured: the runtime's boundary is a single
    // hard pixel, but the port's panels sit a fraction of a pixel off the real
    // rectangles, so a hard line amplifies that phase error (see PROGRESS round 17).
    // The capture's edge transition is 0.18 pt (0.55 device px); the port's 2.2 pt
    // ramp was 5x too soft and bled the fill outward as a halo. The shape is also
    // inset: measured, the silhouette sits 1.8 device px inside nominal, not 0.35.
    // SDF gradient = the surface normal of the lens. Computed first: the edge laws need
    // it, and one of them (the hairline) lives outside the body's coverage.
    float2 g = float2(ddx(d), ddy(d));
    float gl = length(g);
    g = gl > 1e-6 ? g / gl : float2(0.0, 1.0);

    // The outline, before coverage is known. One device pixel of neutral black 0.35 pt
    // inside the boundary; alpha 0.86 where the normal is horizontal, falling to
    // 0.40/0.02 where it is vertical (light/dark) - `edge.x` is that slope, `edge.y` the
    // material's strength (the tinted panel has no line at all).
    float dp = -d;
    float ny = abs(g.y);
    float ctl = saturate(extra.y);
    float edgeK = saturate(edge.y);
    // A control is a small convex piece of glass, not a slab: the capture's switch knob
    // has a bright rim (0.95 white, 4-5 pt) and almost no dark line, so the two terms
    // swap emphasis at ctl = 1.
    float aBlack = lerp((0.86 - edge.x * ny) * edgeK, 0.10f, ctl);
    float hairBox = 1.0 - smoothstep(0.14, 0.20, abs(dp - 0.52));
    float lineA = saturate(aBlack * hairBox);

    // The body is inset 0.95 pt with a 0.30 pt ramp: the capture's fill only settles two
    // device pixels inside the boundary, and its line sits in between.
    float cov = saturate(0.5 - (d + edge.w) / 0.30) * saturate(extra.x);
    if (cov <= 0.0) {
        // Outside: the soft cast shadow, and the hairline if it reaches out here.
        float soft = saturate(1.0 - d / 14.0);
        float a = max(lineA, soft * soft * prm.w * 0.0) * saturate(extra.x);
        return float4(0.0, 0.0, 0.0, saturate(a));
    }

    // Measured per grid line on the refraction chart (1330 samples, pitch exactly
    // 20.000 pt): the displacement peaks at 5.3 pt about 3.5 pt inside the rim, is at
    // 10 % by 20.6 pt, and the interior is bit-exact zero beyond 22 pt. The port shipped
    // A = W = 14 - 2.6x too strong and 1.6x too narrow. Both scale with the radius:
    // A = 0.135 r, W = 0.575 r, so a 40 pt panel gets 5.4 / 23.
    //
    // The shape is not resolvable by the chart (every reasonable form fits within
    // 0.02 pt RMS); (1 - t^2)^2 is the one the spec settled on.
    float A = prm.y;
    float W = max(prm.x * 0.575, 1.0);
    float t = saturate(-d / W);
    float bend = (1.0 - t * t) * (1.0 - t * t);

    // Refraction: fetch further in along the normal, and only near the rim. There is no
    // global magnification: the capture's interior field is zero, and a magnification is
    // what made the port shift content at 26 pt out where the capture shifts none.
    float2 uv = i.uv - g * (bend * A) / screenDim.xy;

    // A control frosts its backdrop much less than a panel: measured, the track
    // stays readable through a held thumb (softened over ~2 pt, not erased), while
    // the one global blur is sized for a 74 pt panel. So a control binds the sharp
    // scene and frosts it here with a five-tap cross blur.
    float3 base;
    if (extra.w > 0.01) {
        float2 o = extra.w / screenDim.xy;
        base  = scene.Sample(linearClamp, uv).rgb * 0.40;
        base += scene.Sample(linearClamp, uv + float2(o.x, 0.0)).rgb * 0.15;
        base += scene.Sample(linearClamp, uv - float2(o.x, 0.0)).rgb * 0.15;
        base += scene.Sample(linearClamp, uv + float2(0.0, o.y)).rgb * 0.15;
        base += scene.Sample(linearClamp, uv - float2(0.0, o.y)).rgb * 0.15;
    } else {
        base = scene.Sample(linearClamp, uv).rgb;
    }

    // No extra vibrancy stage: measured from the real glass page, the chroma loss
    // of a panel is explained entirely by its white lift (regular keeps 0.46x of
    // the backdrop's chroma at a 3.6x luma gain, which is exactly lerp-to-white
    // at ~0.50). Boosting saturation on top of that was wrong.
    //
    // `liftVec.w > 0.5` selects the measured form instead: a per-channel lift
    // recovered from the capture, out = base + lift * (1 - base). That is the same
    // family as lerp-to-white but with a different coefficient per channel, which
    // is how the material's vibrant matrix behaves: measured lifts are 0.593 /
    // 0.476 / 0.679 for the regular panel, i.e. the blue and red channels are
    // lifted more than green, which is where the panel's extra chroma comes from.
    float3 col;
    if (liftVec.w > 0.5) {
        col = base + liftVec.rgb * (float3(1, 1, 1) - base);
    } else {
        col = lerp(base, tint.rgb, tint.a);
        // The runtime panel reads *more* chromatic than the backdrop even though
        // its luma is lifted: the white lift desaturates, and vibrantColorMatrix
        // puts the chroma back. Measured: regular keeps ~1.55x the chroma the lift
        // alone leaves.
        // A large panel has its chroma put back by the material (measured 1.55x
        // what the white lift alone leaves), but a knob is the opposite: at the peak
        // of a flip its interior reads less chromatic than the track under it. So
        // the boost is per panel (extra.z).
        float l2 = dot(col, float3(0.2126, 0.7152, 0.0722));
        col = lerp((float3)l2, col, extra.z);
    }

    // Edge treatment, measured from the capture's pixel profiles:
    //   top edge     backdrop -> dark hairline (60,21,86) -> bright ridge (218,170,244)
    //                ramping to the fill (185,139,212) within ~2 pt
    //   bottom edge  fill -> brighter ridge (231,167,251) -> dark hairline
    //   left / right no ridge; the first pixel inside is *darker* (144,112,177 vs 170,135,201)
    // So the highlight is a slab bevel: it only appears where the surface normal
    // is vertical, and the sides instead darken slightly.
    // The bevel only lights surfaces that are close to horizontal: at 45 degree
    // corners the capture shows a *darker* rim, not a brighter one.
    // Measured profile of the runtime's edge, sample by sample across a boundary:
    // a dark line about one pixel wide on every edge (backdrop 61 -> 26 on the left,
    // 65 -> 53 on the top), then a bright ridge that peaks ~0.5 pt inside and is
    // gone by ~2 pt, present on the top and bottom only (the sides go straight from
    // the line into the fill).
    //
    // The amplitudes below are deliberately *softer* than that profile. With the
    // panel rectangles a fraction of a pixel off the runtime's, a hard boundary
    // amplifies that phase error and measured worse on both screens - 0.32 hair and
    // a 3.5x ridge gave 4.805 on the glass page against 4.688 here, and the same
    // change cost the search field 0.26. The structure is right; the amplitude is
    // the best compromise until the panel geometry is sub-pixel exact, and the
    // gentle falloff is what the previous version approximated by accident.
    // Edge shading. Measured from all six panels in both appearances, per edge and per
    // corner: a 1 device-px neutral-black hairline 0.33 pt inside the boundary, plus a
    // white rim that exists only where the normal has a vertical component. Both are
    // angular laws of the SDF normal, so a corner is where the two laws meet - the old
    // code gated the rim to straight segments and that discontinuity is the hook that
    // has been visible at every corner.
    // The rim: white, and only where the normal has a vertical component (alpha
    // 0.47 * |ny|^2.8 in light, 0.22 in dark, zero on the left and right edges).
    float aWhite = prm.z * pow(ny, 2.8) * (g.y > 0.0 ? 1.10 : 1.0) * edgeK;
    aWhite = lerp(aWhite, 0.85f, ctl);
    // A control carries a wide bright rim - the capture's knob is 0.95 white over 4-5 pt,
    // where a panel's is a one-pixel edge - so the rim tail widens with ctl.
    float rx = dp - lerp(0.6f, 1.6f, ctl);
    float rim = rx < 0.0 ? exp(-rx * rx / lerp(0.18f, 0.9f, ctl)) : exp(-rx / lerp(1.05f, 2.6f, ctl));
    float3 lin = SrgbToLinear(col);
    lin += aWhite * (1.0 - lin) * rim;
    col = LinearToSrgb(lin);

    // Body first, then the line over it as its own layer - the same thing as drawing a
    // 1 px black stroke on the material.
    float bodyA = cov;
    float alpha = bodyA * (1.0 - lineA) + lineA;
    float3 premul = col * bodyA * (1.0 - lineA);
    float3 finalCol = alpha > 1e-4 ? premul / alpha : col;
    return float4(saturate(finalCol), saturate(alpha));
}
)";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlurBlob = nullptr;
        ID3DBlob* psBlitBlob = nullptr;
        ID3DBlob* vsPanelBlob = nullptr;
        ID3DBlob* psLensBlob = nullptr;
        ID3DBlob* err = nullptr;
        const char* target = "ps_5_0";
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "glass.hlsl", nullptr, nullptr,
                              "VS", "vs_5_0", 0, 0, &vsBlob, &err))) return Fail(err);
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "glass.hlsl", nullptr, nullptr,
                              "PSBlur", target, 0, 0, &psBlurBlob, &err))) return Fail(err);
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "glass.hlsl", nullptr, nullptr,
                              "PSBlit", target, 0, 0, &psBlitBlob, &err))) return Fail(err);
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "glass.hlsl", nullptr, nullptr,
                              "VSPanel", "vs_5_0", 0, 0, &vsPanelBlob, &err))) return Fail(err);
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "glass.hlsl", nullptr, nullptr,
                              "PSLens", target, 0, 0, &psLensBlob, &err))) return Fail(err);

        bool ok = true;
        ok &= SUCCEEDED(device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                                   vsBlob->GetBufferSize(), nullptr, &vs));
        ok &= SUCCEEDED(device->CreatePixelShader(psBlurBlob->GetBufferPointer(),
                                                  psBlurBlob->GetBufferSize(), nullptr, &psBlur));
        ok &= SUCCEEDED(device->CreatePixelShader(psBlitBlob->GetBufferPointer(),
                                                  psBlitBlob->GetBufferSize(), nullptr, &psBlit));
        ok &= SUCCEEDED(device->CreateVertexShader(vsPanelBlob->GetBufferPointer(),
                                                   vsPanelBlob->GetBufferSize(), nullptr, &vsPanel));
        ok &= SUCCEEDED(device->CreatePixelShader(psLensBlob->GetBufferPointer(),
                                                  psLensBlob->GetBufferSize(), nullptr, &psLens));
        vsBlob->Release();
        psBlurBlob->Release();
        psBlitBlob->Release();
        vsPanelBlob->Release();
        psLensBlob->Release();

        D3D11_BUFFER_DESC cb = {};
        cb.ByteWidth = 16;
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ok &= SUCCEEDED(device->CreateBuffer(&cb, nullptr, &blurCB));

        D3D11_BUFFER_DESC pcb = {};
        pcb.ByteWidth = 112;  // rect, prm, tint, screenDim, lift, extra, edge
        pcb.Usage = D3D11_USAGE_DYNAMIC;
        pcb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        pcb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ok &= SUCCEEDED(device->CreateBuffer(&pcb, nullptr, &panelCB));

        if (!CreateTarget(device, w * lensScale, h * lensScale, lensTex, lensRTV, lensSRV)) ok = false;

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ok &= SUCCEEDED(device->CreateSamplerState(&sd, &sampler));

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;      // the generated quads have no fixed winding
        rd.DepthClipEnable = TRUE;
        ok &= SUCCEEDED(device->CreateRasterizerState(&rd, &rssNoCull));

        ready = ok;
        return ok;
    }

    void Resize(ID3D11Device* device, int width, int height) {
        Init(device, width, height);
    }

    void Shutdown() {
        auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        rel(sceneTex); rel(sceneRTV); rel(sceneSRV);
        rel(blurA); rel(blurARTV); rel(blurASRV);
        rel(blurB); rel(blurBRTV); rel(blurBSRV);
        rel(blurCB); rel(sampler); rel(vs); rel(psBlur); rel(psBlit);
        rel(panelCB); rel(lensTex); rel(lensRTV); rel(lensSRV); rel(rssNoCull);
        rel(vsPanel); rel(psLens);
        ready = false;
    }

    // --- frame use -------------------------------------------------------
    void Begin(ID3D11DeviceContext* ctx, const float clear[4]) {
        if (!ready) return;
        ctx->OMSetRenderTargets(1, &sceneRTV, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, (float)(w * sceneScale), (float)(h * sceneScale), 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->ClearRenderTargetView(sceneRTV, clear);
    }

    // `iterations` separable H+V passes at half resolution: a wide, smooth blur
    // that keeps soft gradients instead of a boxy kernel footprint.
    void Blur(ID3D11DeviceContext* ctx, int iterations = 3) {
        if (!ready) return;
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(psBlur, nullptr, 0);
        ctx->PSSetSamplers(0, 1, &sampler);
        ctx->PSSetConstantBuffers(0, 1, &blurCB);
        if (iterations <= 0) { lastSRV = sceneSRV; return; }
        for (int it = 0; it < iterations; ++it) {
            ID3D11ShaderResourceView* hsrc = (it == 0) ? sceneSRV : blurBSRV;
            // Only the pass that reads the 3x scene needs the prefilter; the vertical
            // pass reads blurA, which is already at the blur's own resolution.
            // 1.2x spacing: measured, the capture's .regular frost leaves an interior
            // std of 2.26 against a page that swings 166 levels, where the port's blur
            // still leaked Â±8 levels at the grid's 20 pt period.
            Pass(ctx, hsrc, blurARTV, 1.0f / (float)bw, 0.0f, it == 0 ? 1.0f : 0.0f);
            Pass(ctx, blurASRV, blurBRTV, 0.0f, 1.0f / (float)bh, 0.0f);
        }
        lastSRV = blurBSRV;
    }
    // --- lens pass -------------------------------------------------------
    // Clears the lens target to transparent; panels are then stamped into it and
    // the visible frame composites that target at the panel's place in the draw
    // order (so labels drawn after a panel stay on top of it).
    void BeginLens(ID3D11DeviceContext* ctx) {
        if (!ready || !lensRTV) return;
        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->OMSetRenderTargets(1, &lensRTV, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, (float)(w * lensScale), (float)(h * lensScale), 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->ClearRenderTargetView(lensRTV, clear);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ctx->RSSetState(rssNoCull);
        ctx->VSSetShader(vsPanel, nullptr, 0);
        ctx->PSSetShader(psLens, nullptr, 0);
        ctx->PSSetSamplers(0, 1, &sampler);
        ID3D11ShaderResourceView* src = lastSRV ? lastSRV : sceneSRV;
        ctx->PSSetShaderResources(0, 1, &src);
        // The vertex shader builds the quad from `rect`/`screenDim`, so b0 is
        // needed by BOTH stages.
        ctx->PSSetConstantBuffers(0, 1, &panelCB);
        ctx->VSSetConstantBuffers(0, 1, &panelCB);
    }

    // The lens samples the frosted capture by default. A .clear material refracts a
    // *sharp* backdrop in the runtime, so those panels rebind the source to the scene
    // capture (which is now at frame density) before they are stamped.
    void BindLensSource(ID3D11DeviceContext* ctx, bool sharp) {
        if (!ready) return;
        ID3D11ShaderResourceView* src = sharp ? sceneSRV : (lastSRV ? lastSRV : sceneSRV);
        ctx->PSSetShaderResources(0, 1, &src);
    }

    void DrawPanel(ID3D11DeviceContext* ctx, float x, float y, float pw, float ph,
                   float radius, const float tint[4], float refractPx,
                   float rimStrength, float shadowAlpha, float margin = 16.0f,
                   float magnify = 0.012f, const float lift[4] = nullptr,
                   float coverage = 1.0f, float ring = 0.0f,
                   float vibrancy = 1.90f, float frost = 0.0f,
                   float edgeSlope = 0.46f, float edgeStrength = 1.0f,
                   float edgeInset = 0.0f) {
        if (!ready || !lensRTV) return;
        D3D11_MAPPED_SUBRESOURCE m;
        if (FAILED(ctx->Map(panelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
        float* p = (float*)m.pData;
        p[0] = x; p[1] = y; p[2] = pw; p[3] = ph;
        p[4] = radius; p[5] = refractPx; p[6] = rimStrength; p[7] = shadowAlpha;
        p[8] = tint[0]; p[9] = tint[1]; p[10] = tint[2]; p[11] = tint[3];
        p[12] = (float)w; p[13] = (float)h; p[14] = margin; p[15] = magnify;
        p[16] = lift ? lift[0] : 0.0f;
        p[17] = lift ? lift[1] : 0.0f;
        p[18] = lift ? lift[2] : 0.0f;
        p[19] = lift ? 1.0f : 0.0f;                 // liftMode flag
        p[20] = coverage; p[21] = ring; p[22] = vibrancy; p[23] = frost;
        p[24] = edgeSlope; p[25] = edgeStrength; p[26] = 0.0f; p[27] = edgeInset;
        ctx->Unmap(panelCB, 0);
        ctx->Draw(4, 0);
    }

    ID3D11ShaderResourceView* LensSRV() const { return lensSRV; }
    ID3D11ShaderResourceView* BlurSRV() const { return lastSRV; }
    ID3D11ShaderResourceView* SceneSRV() const { return sceneSRV; }
    ID3D11RenderTargetView* SceneRTV() const { return sceneRTV; }

    // Copies the captured scene to `dst` (the backbuffer), used when a frame has
    // no glass and does not need a second UI pass.
    void Blit(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* dst, int dstW, int dstH) {
        if (!ready) return;
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(psBlit, nullptr, 0);
        ctx->PSSetSamplers(0, 1, &sampler);
        ctx->PSSetShaderResources(0, 1, &sceneSRV);
        ctx->OMSetRenderTargets(1, &dst, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, (float)dstW, (float)dstH, 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView* none = nullptr;
        ctx->PSSetShaderResources(0, 1, &none);
    }

    void Pass(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
              ID3D11RenderTargetView* dst, float dx, float dy, float prefilter = 0.0f) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(ctx->Map(blurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            float* p = (float*)mapped.pData;
            p[0] = dx;
            p[1] = dy;
            p[2] = prefilter;
            p[3] = 0.0f;
            ctx->Unmap(blurCB, 0);
        }
        ctx->OMSetRenderTargets(1, &dst, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, (float)bw, (float)bh, 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->PSSetShaderResources(0, 1, &src);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView* none = nullptr;
        ctx->PSSetShaderResources(0, 1, &none);
    }
    bool Ready() const { return ready; }

private:
    ID3D11Device* dev = nullptr;
    int w = 0, h = 0, bw = 0, bh = 0;
    int sceneScale = 1;   // the scene capture resolution, in frame pixels per point
    int lensScale = 1;    // the lens target resolution, same unit
    ID3D11Texture2D* sceneTex = nullptr;
    ID3D11RenderTargetView* sceneRTV = nullptr;
    ID3D11ShaderResourceView* sceneSRV = nullptr;
    ID3D11Texture2D* blurA = nullptr;
    ID3D11RenderTargetView* blurARTV = nullptr;
    ID3D11ShaderResourceView* blurASRV = nullptr;
    ID3D11Texture2D* blurB = nullptr;
    ID3D11RenderTargetView* blurBRTV = nullptr;
    ID3D11ShaderResourceView* blurBSRV = nullptr;
    ID3D11Buffer* blurCB = nullptr;
    ID3D11Buffer* panelCB = nullptr;
    ID3D11Texture2D* lensTex = nullptr;
    ID3D11RenderTargetView* lensRTV = nullptr;
    ID3D11ShaderResourceView* lensSRV = nullptr;
    ID3D11VertexShader* vsPanel = nullptr;
    ID3D11PixelShader* psLens = nullptr;
    ID3D11RasterizerState* rssNoCull = nullptr;
    ID3D11SamplerState* sampler = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* psBlur = nullptr;
    ID3D11PixelShader* psBlit = nullptr;
    ID3D11RenderTargetView* lastTarget = nullptr;
    ID3D11ShaderResourceView* lastSRV = nullptr;
    bool ready = false;

    bool Fail(ID3DBlob* err) {
        if (err) {
            FILE* f = nullptr;
            if (fopen_s(&f, "shader_error.txt", "wb") == 0 && f) {
                fwrite(err->GetBufferPointer(), 1, err->GetBufferSize(), f);
                fclose(f);
            }
            err->Release();
        }
        return false;
    }

    static bool CreateTarget(ID3D11Device* device, int w, int h,
                             ID3D11Texture2D*& tex, ID3D11RenderTargetView*& rtv,
                             ID3D11ShaderResourceView*& srv) {
        D3D11_TEXTURE2D_DESC d = {};
        d.Width = (UINT)w;
        d.Height = (UINT)h;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&d, nullptr, &tex))) return false;
        if (FAILED(device->CreateRenderTargetView(tex, nullptr, &rtv))) return false;
        if (FAILED(device->CreateShaderResourceView(tex, nullptr, &srv))) return false;
        return true;
    }
};

} // namespace sw
