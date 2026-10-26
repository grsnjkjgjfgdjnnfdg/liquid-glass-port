#pragma once
#include "../opal_types.h"
#include "../imgui/imgui.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace opal::anim {

struct Particle {
    Vec2  pos, vel;
    float life, maxLife, size, alpha;
    Color color;
    bool  alive() const { return life > 0.0f && alpha > 0.005f; }
};

class ParticleSystem {
public:
    void Emit(int count, const Vec2& center, float spread = 50.0f,
              float lifeMin = 0.5f, float lifeMax = 1.5f);
    void Update(float dt);
    void Render(ImDrawList* dl);
    void Clear()                { m_particles.clear(); }
    bool IsEmpty() const        { return m_particles.empty(); }
    void SetColors(const Color& c1, const Color& c2) { m_color1 = c1; m_color2 = c2; }
    int  Count() const          { return (int)m_particles.size(); }
    void SetGravity(const Vec2& g) { m_gravity = g; }
    void SetDrag(float d)          { m_drag = d; }

private:
    std::vector<Particle> m_particles;
    Color  m_color1  = Color(1.0f, 0.37f, 0.0f, 1.0f);
    Color  m_color2  = Color(1.0f, 0.65f, 0.0f, 1.0f);
    Vec2   m_gravity = Vec2(0.0f, 30.0f);
    float  m_drag    = 0.98f;
    uint32_t m_rng   = 0xDEADBEEF;

    inline float RandFloat() {
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        return (float)(m_rng & 0x7FFFFFFFu) / 2147483647.0f;
    }
};

inline void ParticleSystem::Emit(int count, const Vec2& center, float spread,
                                  float lifeMin, float lifeMax) {
    for (int i = 0; i < count; ++i) {
        float angle = RandFloat() * 6.2831853f;
        float speed = 20.0f + RandFloat() * 280.0f;

        Particle p;
        p.pos     = center;
        p.vel     = Vec2(std::cos(angle) * speed + (RandFloat() - 0.5f) * spread,
                          std::sin(angle) * speed + (RandFloat() - 0.5f) * spread);
        p.maxLife = lifeMin + RandFloat() * (lifeMax - lifeMin);
        p.life    = p.maxLife;
        p.size    = 2.0f + RandFloat() * 5.0f;
        p.alpha   = 1.0f;
        float t   = RandFloat();
        p.color   = Color(m_color1.r + (m_color2.r - m_color1.r) * t,
                          m_color1.g + (m_color2.g - m_color1.g) * t,
                          m_color1.b + (m_color2.b - m_color1.b) * t,
                          1.0f);
        m_particles.push_back(p);
    }
}

inline void ParticleSystem::Update(float dt) {
    for (auto& p : m_particles) {
        p.life -= dt;
        if (p.life <= 0.0f) continue;

        p.vel.x *= m_drag;
        p.vel.y *= m_drag;
        p.vel.x += m_gravity.x * dt;
        p.vel.y += m_gravity.y * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        float lifeFrac = p.life / p.maxLife;
        p.alpha = lifeFrac > 0.5f ? 1.0f : lifeFrac * 2.0f;
        if (lifeFrac < 0.2f)
            p.alpha *= lifeFrac * 5.0f;
    }

    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
            [](const Particle& p) { return p.life <= 0.0f || p.alpha < 0.005f; }),
        m_particles.end());
}

inline void ParticleSystem::Render(ImDrawList* dl) {
    if (!dl) return;
    for (const auto& p : m_particles) {
        if (!p.alive()) continue;
        ImU32 col = ((ImU32)(p.alpha * 255.0f) << 24) |
                    ((ImU32)(p.color.b * 255.0f) << 16) |
                    ((ImU32)(p.color.g * 255.0f) << 8) |
                    ((ImU32)(p.color.r * 255.0f));
        dl->AddCircleFilled(ImVec2(p.pos.x, p.pos.y), p.size * p.alpha, col);
    }
}

} // namespace opal::anim
