#pragma once
#include "anim_core.h"
#include "../opal_types.h"
#include <vector>
#include <string>
#include <cstring>

namespace opal::anim {

using ::opal::Clamp;

class TextReveal {
public:
    enum Mode { Typewriter, Popup, BlurIn, FadeIn };

    void Start(const char* text, Mode mode = Typewriter, float stagger = 0.010f);
    void Update(float dt);
    bool IsDone() const;
    float CharScale(int index) const;
    float CharBlur(int index) const;
    float OverallAlpha() const { return m_globalAlpha.value; }
    float YOffset()      const { return m_yOffset.value; }
    float FrostAlpha()   const { return m_frostAlpha; }
    size_t CharCount()   const { return m_charScale.size(); }

private:
    Mode m_mode = Typewriter;
    Spring m_globalAlpha { 0.0f };
    Spring m_blockScale  { 1.0f };
    Spring m_blockBlur   { 0.0f };
    Spring m_yOffset     { 0.0f };

    std::vector<Spring> m_charScale;
    std::vector<Spring> m_charBlur;
    std::vector<float>  m_charStartTime;
    std::vector<bool>   m_charTriggered;

    float m_elapsed     = 0.0f;
    float m_stagger     = 0.010f;
    float m_duration    = 0.0f;
    float m_frostAlpha  = 0.0f;
    bool  m_frostTriggered = false;
};

inline void TextReveal::Start(const char* text, Mode mode, float stagger) {
    m_mode = mode;
    m_elapsed = 0.0f;
    m_stagger = stagger;
    m_frostAlpha = 0.0f;
    m_frostTriggered = false;

    int n = (int)strlen(text);
    m_charScale.resize(n);
    m_charBlur.resize(n);
    m_charStartTime.resize(n);
    m_charTriggered.assign(n, false);

    m_globalAlpha.Snap(mode == Typewriter ? 1.0f : 0.0f);
    m_globalAlpha.stiffness = Presets::FastStiff;
    m_globalAlpha.damping   = Presets::FastDamp;
    m_globalAlpha.SetTarget(1.0f);

    m_blockScale.Snap(1.0f);
    m_blockScale.stiffness = Presets::FastStiff;
    m_blockScale.damping   = Presets::FastDamp;

    m_blockBlur.Snap(0.0f);
    m_blockBlur.stiffness = Presets::FastStiff;
    m_blockBlur.damping   = Presets::FastDamp;

    m_yOffset.Snap(0.0f);
    m_yOffset.stiffness = Presets::FastStiff;
    m_yOffset.damping   = Presets::FastDamp;

    for (int i = 0; i < n; ++i) {
        m_charStartTime[i] = (float)i * stagger;

        switch (mode) {
        case Typewriter:
            m_charScale[i].Snap(0.0f);
            m_charScale[i].stiffness = Presets::BouncyStiff;
            m_charScale[i].damping   = Presets::BouncyDamp;
            m_charBlur[i].Snap(2.0f);
            m_charBlur[i].stiffness = Presets::FastStiff;
            m_charBlur[i].damping   = Presets::FastDamp;
            break;
        case BlurIn:
            m_charScale[i].Snap(1.0f);
            m_charBlur[i].value = 4.0f;
            m_charBlur[i].target = 4.0f;
            m_charBlur[i].stiffness = Presets::SmoothStiff;
            m_charBlur[i].damping   = Presets::SmoothDamp;
            m_charStartTime[i] = (float)i * Clamp(stagger * 3.0f, 0.030f, 0.050f);
            break;
        default:
            m_charScale[i].Snap(1.0f);
            m_charBlur[i].Snap(0.0f);
            break;
        }
    }

    switch (mode) {
    case Popup:
        m_blockScale.value = 0.95f;
        m_blockScale.target = 1.0f;
        m_blockScale.stiffness = Presets::BouncyStiff;
        m_blockScale.damping   = Presets::BouncyDamp;
        m_blockBlur.value = 2.0f;
        m_blockBlur.target = 0.0f;
        m_blockBlur.SetVelocity(0.15f);
        break;
    case BlurIn:
        m_globalAlpha.stiffness = Presets::SmoothStiff;
        m_globalAlpha.damping   = Presets::SmoothDamp;
        m_duration = (float)n * stagger * 3.0f + 0.45f;
        return;
    case FadeIn:
        m_yOffset.value = -4.0f;
        m_yOffset.target = 0.0f;
        m_yOffset.SetVelocity(-0.5f);
        m_blockScale.value = 0.98f;
        m_blockScale.target = 1.0f;
        break;
    default: break;
    }

    m_duration = (float)n * stagger + 0.3f;
}

inline void TextReveal::Update(float dt) {
    m_elapsed += dt;
    m_globalAlpha.Update(dt);

    if (m_mode == Typewriter) {
        for (size_t i = 0; i < m_charScale.size(); ++i) {
            if (!m_charTriggered[i] && m_elapsed >= m_charStartTime[i]) {
                m_charTriggered[i] = true;
                m_charScale[i].SetVelocity(2.0f);
            }
            m_charScale[i].Update(dt);
            if (m_charTriggered[i])
                m_charBlur[i].SetTarget(0.0f);
            m_charBlur[i].Update(dt);
        }
    } else if (m_mode == Popup) {
        m_blockScale.Update(dt);
        m_blockBlur.Update(dt);
        if (!m_frostTriggered && m_elapsed > 0.01f) {
            m_frostAlpha = 1.0f;
            m_frostTriggered = true;
        }
        if (m_frostAlpha > 0.0f) {
            m_frostAlpha -= dt * 4.5f;
            if (m_frostAlpha < 0.0f) m_frostAlpha = 0.0f;
        }
    } else if (m_mode == BlurIn) {
        for (size_t i = 0; i < m_charBlur.size(); ++i) {
            if (!m_charTriggered[i] && m_elapsed >= m_charStartTime[i]) {
                m_charTriggered[i] = true;
                m_charBlur[i].SetTarget(0.0f);
            }
            m_charBlur[i].Update(dt);
        }
    } else if (m_mode == FadeIn) {
        m_yOffset.Update(dt);
        m_blockScale.Update(dt);
    }
}

inline bool TextReveal::IsDone() const { return m_elapsed >= m_duration; }

inline float TextReveal::CharScale(int index) const {
    if (m_mode == Typewriter) {
        if (index < 0 || index >= (int)m_charScale.size()) return 0.0f;
        return m_charScale[index].value;
    }
    return m_blockScale.value;
}

inline float TextReveal::CharBlur(int index) const {
    switch (m_mode) {
    case Typewriter:
    case BlurIn:
        if (index < 0 || index >= (int)m_charBlur.size()) return 0.0f;
        return m_charBlur[index].value;
    default:
        return m_blockBlur.value;
    }
}

} // namespace opal::anim
