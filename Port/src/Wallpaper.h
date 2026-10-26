#pragma once
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <random>

namespace sw {

static inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static inline float Smooth(float a, float b, float x) {
    float t = Clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
struct F3 { float r, g, b; };
static inline F3 MixC(F3 a, F3 b, float t) {
    return { Mix(a.r, b.r, t), Mix(a.g, b.g, t), Mix(a.b, b.b, t) };
}
static inline F3 Add(F3 a, F3 b) { return { a.r + b.r, a.g + b.g, a.b + b.b }; }
static inline F3 Mul(F3 a, float s) { return { a.r * s, a.g * s, a.b * s }; }

// An original desktop backdrop in the language of Apple's macOS 27 art:
// broad, soft ribbons of brushed stone and violet with one achromatic
// specular edge. It exists so the glass has real luminance and colour to
// refract, not a flat gradient.
inline std::vector<uint8_t> MakeWallpaper(int w, int h) {
    std::vector<uint8_t> out((size_t)w * h * 4);
    const F3 deep   { 0.035f, 0.030f, 0.055f };
    const F3 plum   { 0.160f, 0.105f, 0.185f };
    const F3 stone  { 0.520f, 0.405f, 0.365f };
    const F3 pearl  { 0.760f, 0.720f, 0.780f };
    const F3 violet { 0.345f, 0.275f, 0.510f };
    const F3 ice    { 0.520f, 0.640f, 0.720f };
    std::mt19937 rng(0x27A0C0DE);
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float u = (float)x / (float)std::max(1, w - 1);
            float v = (float)y / (float)std::max(1, h - 1);

            // Slow diagonal flow, with two frequencies so the bands never
            // read as a repeating wallpaper pattern.
            float ribbon = v * 0.90f + u * 0.12f
                         + 0.085f * std::sin(u * 5.2f + 0.35f)
                         + 0.030f * std::sin(u * 13.0f + 1.7f)
                         + 0.012f * std::sin(u * 31.0f + v * 2.0f);

            F3 c = MixC(deep, plum, Clamp01(0.15f + v * 0.75f));
            c = Add(c, Mul(stone, 0.18f * Smooth(0.0f, 0.55f, u * 0.65f + (1.0f - v) * 0.35f)));

            // Broad satin bands.
            float b1 = Smooth(0.26f, 0.35f, ribbon) * (1.0f - Smooth(0.49f, 0.60f, ribbon));
            float b2 = Smooth(0.61f, 0.69f, ribbon) * (1.0f - Smooth(0.77f, 0.86f, ribbon));
            float b3 = Smooth(0.00f, 0.06f, ribbon) * (1.0f - Smooth(0.11f, 0.20f, ribbon));
            c = MixC(c, MixC(stone, pearl, Clamp01((u - 0.10f) * 0.85f)), b1 * 0.82f);
            c = MixC(c, MixC(plum, violet, Clamp01(u * 1.2f)), b2 * 0.70f);
            c = MixC(c, ice, b3 * 0.32f);

            // A single achromatic specular seam on the leading edge.
            float seam1 = 1.0f - Smooth(0.0f, 0.009f, std::fabs(ribbon - 0.252f));
            float seam2 = 1.0f - Smooth(0.0f, 0.006f, std::fabs(ribbon - 0.612f));
            c = Add(c, Mul(F3{ 0.95f, 0.94f, 0.96f }, (seam1 * 0.55f + seam2 * 0.20f)));

            // Two soft coloured rooms, so the window lands across a colour
            // boundary rather than in a dead centre.
            float d1 = (u - 0.18f) * (u - 0.18f) + (v - 0.22f) * (v - 0.22f);
            float d2 = (u - 0.88f) * (u - 0.88f) + (v - 0.73f) * (v - 0.73f);
            c = Add(c, Mul(F3{ 0.22f, 0.12f, 0.08f }, std::exp(-d1 * 8.0f) * 0.55f));
            c = Add(c, Mul(F3{ 0.20f, 0.18f, 0.32f }, std::exp(-d2 * 7.0f) * 0.65f));

            // Gentle vignette and fine film grain (deterministic).
            float vx = (u - 0.5f) * 2.0f, vy = (v - 0.5f) * 2.0f;
            float vig = 1.0f - 0.18f * Clamp01(vx * vx + vy * vy);
            c = Mul(c, vig);
            float grain = jitter(rng) * 0.006f;
            c = Add(c, { grain, grain, grain });

            int idx = (y * w + x) * 4;
            out[idx + 0] = (uint8_t)std::lround(Clamp01(c.r) * 255.0f);
            out[idx + 1] = (uint8_t)std::lround(Clamp01(c.g) * 255.0f);
            out[idx + 2] = (uint8_t)std::lround(Clamp01(c.b) * 255.0f);
            out[idx + 3] = 255;
        }
    }
    return out;
}

inline ID3D11ShaderResourceView* CreateWallpaperSRV(ID3D11Device* device, int w, int h) {
    auto pixels = MakeWallpaper(w, h);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = (UINT)w;
    desc.Height = (UINT)h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = pixels.data();
    data.SysMemPitch = (UINT)w * 4;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &tex)) || !tex) return nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return srv;
}

} // namespace sw
