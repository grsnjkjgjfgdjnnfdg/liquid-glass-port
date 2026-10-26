#pragma once
#include <cmath>

namespace opal::anim {

namespace Presets {
    constexpr float NormalStiff   = 120.0f;
    constexpr float NormalDamp    = 14.0f;
    constexpr float FastStiff     = 200.0f;
    constexpr float FastDamp      = 18.0f;
    constexpr float HeavyStiff    = 80.0f;
    constexpr float HeavyDamp     = 22.0f;
    constexpr float BouncyStiff   = 90.0f;
    constexpr float BouncyDamp    = 8.0f;
    constexpr float InstantStiff  = 300.0f;
    constexpr float InstantDamp   = 28.0f;
    constexpr float SmoothStiff   = 100.0f;
    constexpr float SmoothDamp    = 16.0f;
}

struct Spring {
    float value     = 0.0f;
    float velocity  = 0.0f;
    float target    = 0.0f;
    float stiffness = Presets::NormalStiff;
    float damping   = Presets::NormalDamp;

    Spring() = default;
    Spring(float initial) : value(initial), target(initial) {}

    void SetTarget(float t)        { target = t; }
    void SetVelocity(float v)      { velocity = v; }
    void Snap(float v)             { value = v; target = v; velocity = 0.0f; }
    void Reset()                   { value = target; velocity = 0.0f; }
    float Target() const           { return target; }

    void Update(float dt) {
        if (dt <= 0.0f) return;
        float force = (target - value) * stiffness;
        float accel = force - velocity * damping;
        velocity += accel * dt;
        value    += velocity * dt;
    }

    bool IsResting(float threshold = 0.001f) const {
        return fabsf(target - value) < threshold && fabsf(velocity) < threshold;
    }
};

struct Spring2D {
    float x, y;
    float vx, vy;
    float tx, ty;
    float stiffness = Presets::NormalStiff;
    float damping   = Presets::NormalDamp;

    Spring2D() : x(0), y(0), vx(0), vy(0), tx(0), ty(0) {}
    Spring2D(float ix, float iy) : x(ix), y(iy), vx(0), vy(0), tx(ix), ty(iy) {}

    void SetTarget(float nx, float ny) { tx = nx; ty = ny; }
    void SetVelocity(float vxN, float vyN) { vx = vxN; vy = vyN; }
    void Snap(float nx, float ny) { x = nx; y = ny; tx = nx; ty = ny; vx = 0; vy = 0; }
    void Reset() { x = tx; y = ty; vx = 0; vy = 0; }

    void Update(float dt) {
        if (dt <= 0.0f) return;
        float fx = (tx - x) * stiffness;
        float fy = (ty - y) * stiffness;
        float ax = fx - vx * damping;
        float ay = fy - vy * damping;
        vx += ax * dt;
        vy += ay * dt;
        x  += vx * dt;
        y  += vy * dt;
    }

    bool IsResting(float threshold = 0.001f) const {
        float dx = tx - x, dy = ty - y;
        return dx * dx + dy * dy < threshold * threshold &&
               vx * vx + vy * vy < threshold * threshold;
    }

    float X() const { return x; }
    float Y() const { return y; }
};

} // namespace opal::anim
