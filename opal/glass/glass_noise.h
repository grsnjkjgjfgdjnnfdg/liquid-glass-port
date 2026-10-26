#pragma once
#include <cstdint>
#include <cmath>

namespace opal::glass {

inline float FbmHash(uint32_t state) {
    state ^= 2747636419u;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    state ^= state >> 16;
    return (float)(state & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

inline float FbmNoise(float x, float y, int octaves) {
    float value = 0.0f;
    float amp   = 1.0f;
    float freq  = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        int ix = (int)floorf(x * freq);
        int iy = (int)floorf(y * freq);
        float fx = x * freq - (float)ix;
        float fy = y * freq - (float)iy;
        float sx = fx * fx * (3.0f - 2.0f * fx);
        float sy = fy * fy * (3.0f - 2.0f * fy);
        float n00 = FbmHash((uint32_t)(ix + iy * 1319));
        float n10 = FbmHash((uint32_t)(ix + 1 + iy * 1319));
        float n01 = FbmHash((uint32_t)(ix + (iy + 1) * 1319));
        float n11 = FbmHash((uint32_t)(ix + 1 + (iy + 1) * 1319));
        float nx0 = n00 + (n10 - n00) * sx;
        float nx1 = n01 + (n11 - n01) * sx;
        value += (nx0 + (nx1 - nx0) * sy) * amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return value;
}

inline float CurlNoiseX(float x, float y, float time) {
    float eps = 0.001f;
    float dy = FbmNoise(x + eps, y, 3) - FbmNoise(x - eps, y, 3);
    return dy / (2.0f * eps);
}

inline float CurlNoiseY(float x, float y, float time) {
    float eps = 0.001f;
    float dx = FbmNoise(x, y + eps, 3) - FbmNoise(x, y - eps, 3);
    return -dx / (2.0f * eps);
}

} // namespace opal::glass
