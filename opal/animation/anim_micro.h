#pragma once
#include "anim_core.h"

namespace opal::anim {

struct MicroInteraction {
    Spring uniform { 1.0f };
    Spring scaleX  { 1.0f };
    Spring scaleY  { 1.0f };
    Spring offset  { 0.0f };
    Spring glow    { 0.0f };
    float  elapsed  = 0.0f;
    bool   active   = false;
    bool   squishPhase2 = false;

    MicroInteraction() {
        scaleX.stiffness = Presets::FastStiff;
        scaleX.damping   = Presets::FastDamp;
        scaleY.stiffness = Presets::FastStiff;
        scaleY.damping   = Presets::FastDamp;
        offset.stiffness = Presets::BouncyStiff;
        offset.damping   = Presets::BouncyDamp;
        glow.stiffness   = Presets::FastStiff;
        glow.damping     = Presets::FastDamp;
    }

    void PlayJiggle(float amount = 3.0f) {
        offset.SetVelocity(amount * 30.0f);
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }
    void PlayPress() {
        uniform.SetVelocity(-0.25f);
        uniform.stiffness = Presets::InstantStiff;
        uniform.damping   = Presets::FastDamp;
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }
    void PlayPop() {
        uniform.SetVelocity(0.30f);
        uniform.stiffness = Presets::BouncyStiff;
        uniform.damping   = Presets::BouncyDamp;
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }
    void PlayRelease() {
        uniform.SetVelocity(0.08f);
        uniform.stiffness = Presets::FastStiff;
        uniform.damping   = Presets::FastDamp;
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }
    void PlaySquish() {
        scaleX.SetTarget(1.08f);
        scaleY.SetTarget(0.96f);
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }
    void PlayPulse(float strength = 1.0f) {
        glow.SetTarget(strength);
        glow.SetVelocity(strength * 8.0f);
        active = true; elapsed = 0.0f; squishPhase2 = false;
    }

    void Update(float dt) {
        elapsed += dt;
        uniform.Update(dt);
        scaleX.Update(dt);
        scaleY.Update(dt);
        offset.Update(dt);
        glow.Update(dt);

        if (squishPhase2) {
            if (scaleX.IsResting() && scaleY.IsResting())
                active = false;
        }
        if (uniform.Target() != 1.0f) {
            if (uniform.value < 1.0f && uniform.Target() == 1.0f && uniform.IsResting())
                uniform.Snap(1.0f);
        }
        if (!squishPhase2 && (scaleX.Target() == 1.08f || scaleX.Target() > 1.0f)) {
            if (elapsed > 0.12f && scaleX.value > 1.01f) {
                scaleX.SetTarget(1.0f);
                scaleY.SetTarget(1.0f);
                squishPhase2 = true;
            }
        }
        if (glow.Target() > 0.0f && glow.IsResting() && elapsed > 0.25f) {
            glow.SetTarget(0.0f);
        }
        if (elapsed > 0.5f && !squishPhase2
            && uniform.IsResting() && offset.IsResting() && glow.IsResting()) {
            active = false;
        }
    }

    void Reset() {
        uniform.Snap(1.0f);
        scaleX.Snap(1.0f);
        scaleY.Snap(1.0f);
        offset.Snap(0.0f);
        glow.Snap(0.0f);
        active = false;
        elapsed = 0.0f;
        squishPhase2 = false;
    }

    float ScaleW() const { return uniform.value * scaleX.value; }
    float ScaleH() const { return uniform.value * scaleY.value; }
    float Offset()  const { return offset.value; }
    float Glow()    const { return glow.value; }
};

} // namespace opal::anim
