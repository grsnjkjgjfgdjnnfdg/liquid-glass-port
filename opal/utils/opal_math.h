#pragma once
#include "../opal_types.h"

namespace opal::utils {

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline float  Smoothstep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float  InverseLerp(float a, float b, float v) { return (v - a) / (b - a); }
inline float  Remap(float v, float inLo, float inHi, float outLo, float outHi) {
    return Lerp(outLo, outHi, InverseLerp(inLo, inHi, v));
}

inline float EaseOutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
inline float EaseOutQuart(float t) { float u = 1.0f - t; return 1.0f - u * u * u * u; }
inline float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) + c1 * (t - 1.0f) * (t - 1.0f);
}
inline float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
}

inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t) { return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) }; }
inline Vec2 Clamp(const Vec2& v, const Vec2& lo, const Vec2& hi) { return { Clamp(v.x, lo.x, hi.x), Clamp(v.y, lo.y, hi.y) }; }

} // namespace opal::utils
