#pragma once
#include "../opal_types.h"
#include "../glass/glass_noise.h"
#include <cmath>

namespace opal::anim {

using opal::Clamp;
using opal::Smoothstep;

class InkTransition {
public:
    bool  active      = false;
    bool  darkToLight = false;
    float progress    = 0.0f;
    float elapsed     = 0.0f;
    Vec2  origin      = Vec2(0.5f, 0.5f);

    void Start(bool toLight = false, float duration = 0.55f);
    void Update(float dt);
    bool  IsDone()     const { return progress >= 1.0f; }
    float MaskAt(const Vec2& uv, float time) const;

private:
    float m_duration  = 0.55f;
    float m_seed      = 0.0f;
    float m_flowBias  = 0.0f;

    static inline float EaseInkProgress(float t) {
        if (t < 0.40f) return Clamp(t / 0.40f, 0.0f, 1.0f) * 0.60f;
        if (t < 0.75f) return 0.60f + Clamp((t - 0.40f) / 0.35f, 0.0f, 1.0f) * 0.30f;
        float p = Clamp((t - 0.75f) / 0.25f, 0.0f, 1.0f);
        return 0.90f + p * p * p * 0.10f;
    }
};

inline void InkTransition::Start(bool toLight, float duration) {
    darkToLight = toLight;
    active      = true;
    progress    = 0.0f;
    elapsed     = 0.0f;
    m_duration  = duration;
    m_seed      = opal::glass::FbmHash((uint32_t)((uint64_t)this & 0xFFFFFFFFu) ^ 0x9E3779B9u) * 100.0f;
    m_flowBias  = opal::glass::FbmHash((uint32_t)(m_seed * 2654435761u)) * 0.3f - 0.15f;
    origin = Vec2(0.45f + opal::glass::FbmHash((uint32_t)(m_seed * 2747636419u)) * 0.1f,
                  0.45f + opal::glass::FbmHash((uint32_t)(m_seed * 3654435769u)) * 0.1f);
}

inline void InkTransition::Update(float dt) {
    if (!active) return;
    elapsed  += dt;
    progress  = Clamp(elapsed / m_duration, 0.0f, 1.0f);
    if (progress >= 1.0f) active = false;
}

inline float InkTransition::MaskAt(const Vec2& uv, float time) const {
    if (!active) return darkToLight ? 0.0f : 1.0f;

    float p = EaseInkProgress(progress);
    float ts = time + m_seed * 0.1f;

    float cx = opal::glass::CurlNoiseX(uv.x * 3.0f + m_flowBias, uv.y * 3.0f, ts * 0.15f) * 0.4f;
    float cy = opal::glass::CurlNoiseY(uv.x * 3.0f + m_flowBias, uv.y * 3.0f, ts * 0.15f) * 0.4f;

    float warpU = uv.x + cx + ts * 0.05f;
    float warpV = uv.y + cy + ts * 0.03f;
    float noise = opal::glass::FbmNoise(warpU * 4.0f, warpV * 4.0f, 4);
    noise += opal::glass::FbmNoise((warpU + cx * 1.5f) * 7.0f, (warpV + cy * 1.5f) * 7.0f, 3);

    float tendrils = Smoothstep(0.35f, 0.65f, noise);

    float dx = uv.x - origin.x;
    float dy = uv.y - origin.y;
    float d = std::sqrt(dx * dx + dy * dy);

    float ink = p - (d * 0.85f + tendrils * 0.25f + noise * 0.12f);
    float mask = Smoothstep(-0.08f, 0.04f, ink);

    float droplets = opal::glass::FbmNoise(uv.x * 20.0f + ts * 0.3f, uv.y * 20.0f, 2);
    droplets = Smoothstep(0.55f, 0.6f, droplets) * Smoothstep(-0.02f, 0.06f, ink);

    float result = Clamp(mask + droplets * 0.3f, 0.0f, 1.0f);
    return darkToLight ? 1.0f - result : result;
}

} // namespace opal::anim
