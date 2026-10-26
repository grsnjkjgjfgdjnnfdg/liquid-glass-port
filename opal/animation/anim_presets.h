#pragma once
#include "anim_core.h"
#include "anim_transition.h"
#include "anim_text.h"
#include "anim_micro.h"

namespace opal::anim {

inline void AnimateToggleOn(MicroInteraction& micro) {
    micro.Reset();
    micro.uniform.SetVelocity(0.15f);
    micro.active = true;
}
inline void AnimateToggleOff(MicroInteraction& micro) {
    micro.Reset();
    micro.uniform.SetVelocity(-0.08f);
    micro.active = true;
}
inline void AnimateButtonPress(MicroInteraction& micro)   { micro.PlayPress(); }
inline void AnimateButtonPop(MicroInteraction& micro)     { micro.PlayPop(); }
inline void AnimateCheckboxPop(MicroInteraction& micro)   { micro.PlayPop(); }
inline void AnimatePulse(MicroInteraction& micro, float s = 1.0f) { micro.PlayPulse(s); }
inline void AnimateSliderDrag(MicroInteraction& micro)    { micro.PlaySquish(); }
inline void AnimateSliderRelease(MicroInteraction& micro)  { micro.PlayRelease(); }
inline void AnimatePanelOpen(GlassBloomState& bloom, const Vec2& origin) { bloom.Open(origin); }
inline void AnimatePanelClose(GlassBloomState& bloom)     { bloom.Close(); }

inline void AnimateSpring(Spring& s, float target) {
    s.SetTarget(target);
    s.SetVelocity((target - s.value) * 3.0f);
}
inline void AnimateSlideTo(Spring& s, float target, float speed = 3.0f) {
    s.SetTarget(target);
    s.SetVelocity((target - s.value) * speed);
}
inline void AnimateBounce(Spring& s) {
    s.SetVelocity(0.5f);
    s.stiffness = Presets::BouncyStiff;
    s.damping   = Presets::BouncyDamp;
}
inline void AnimateBreathe(Spring& s, float min = 0.95f, float max = 1.05f, float period = 2.0f) {
    s.SetTarget(min);
    s.stiffness = Presets::HeavyStiff;
    s.damping   = Presets::HeavyDamp;
    (void)max; (void)period;
}

} // namespace opal::anim
