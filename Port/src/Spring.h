#pragma once
#include <cmath>

namespace sw {

// SwiftUI's spring model, expressed the way SwiftUI's API expresses it.
//
//   .spring(response: 0.5, dampingFraction: 0.825)
//
// The analytic solution is used instead of Euler integration so a dropped
// frame cannot change the shape of the motion.
struct Spring {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;
    float response = 0.35f;
    float dampingFraction = 0.86f;

    Spring() = default;
    explicit Spring(float v) : value(v), target(v) {}
    Spring(float v, float response_, float damping_)
        : value(v), target(v), response(response_), dampingFraction(damping_) {}

    void Snap(float v) { value = target = v; velocity = 0.0f; }
    void SetTarget(float v) { target = v; }
    void Update(float dt) {
        if (dt <= 0.0f) return;
        if (response <= 0.0001f) { value = target; velocity = 0.0f; return; }

        // SwiftUI states the spring as a duration (the undamped period) and a
        // damping fraction, not as stiffness/damping. Convert exactly.
        const float w0 = 6.28318530718f / response;
        const float z  = dampingFraction;
        const float x0 = value - target;
        const float v0 = velocity;
        const float t  = dt;

        if (z < 0.9999f) {
            const float wd = w0 * std::sqrt(1.0f - z * z);
            const float e  = std::exp(-z * w0 * t);
            const float c  = std::cos(wd * t);
            const float s  = std::sin(wd * t);
            const float A  = x0;
            const float B  = (v0 + z * w0 * x0) / wd;
            value = target + e * (A * c + B * s);
            velocity = e * ((-z * w0) * (A * c + B * s) + wd * (-A * s + B * c));
        } else if (z <= 1.0001f) {
            const float e = std::exp(-w0 * t);
            value = target + (x0 + (v0 + w0 * x0) * t) * e;
            velocity = (v0 - w0 * (v0 + w0 * x0) * t) * e;
        } else {
            const float r  = std::sqrt(z * z - 1.0f);
            const float r1 = -w0 * (z - r);
            const float r2 = -w0 * (z + r);
            const float c1 = (v0 - r2 * x0) / (r1 - r2);
            const float c2 = x0 - c1;
            value = target + c1 * std::exp(r1 * t) + c2 * std::exp(r2 * t);
            velocity = c1 * r1 * std::exp(r1 * t) + c2 * r2 * std::exp(r2 * t);
        }

        if (std::fabs(target - value) < 0.00003f && std::fabs(velocity) < 0.0001f) {
            value = target;
            velocity = 0.0f;
        }
    }
};

struct Spring2 {
    Spring x, y;
    Spring2() = default;
    Spring2(float ix, float iy, float response, float damping) : x(ix, response, damping), y(iy, response, damping) {}
    void Snap(float vx, float vy) { x.Snap(vx); y.Snap(vy); }
    void Update(float dt) { x.Update(dt); y.Update(dt); }
};

} // namespace sw
