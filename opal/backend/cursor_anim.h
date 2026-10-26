#pragma once
// ============================================================================
// cursor_anim.h — one shared animated cursor (overlay + UI tests).
//
// Feel: velocity lean (tilts into fast moves, sprung), motion stretch,
// click squish, soft drop shadow and a thin dark rim for contrast. The graphic
// itself (mousev2) is a pure white arrow with an anti-aliased edge and no
// outline of its own, so the rim and the shadow are what keep it legible over
// a light background.
//
// Two things about the source art drive the geometry below:
//
//   * its INK does not start at the texture's top-left corner. The tip sits
//     at (0.337, 0.123) of a 512x512 sheet, so mapping the full sheet into a
//     quad anchored at the pointer put the visible tip ~10px down-right of
//     the point that was actually clicked. The quad is now built about the
//     tip instead, which also means the stretch and the lean pivot on the tip
//     rather than swinging it around.
//   * it is 100% white (RGB is 255 everywhere), so nothing but the rim and
//     shadow separates it from a bright surface.
// ============================================================================
#include "../animation/anim_core.h"
#include "../utils/opal_math.h"
// File-relative, two levels up: the framework/opal/backend -> the framework/imgui.
// The previous single-level form pointed at the framework/opal/imgui, which does
// not exist, so this header only ever compiled where an include path happened
// to paper over it.
#include "../../imgui/imgui.h"
#include <d3d11.h>
#include <cmath>

namespace opal {
namespace backend {

// Ink tip of mousev2.png, as a fraction of the 512x512 sheet.
inline constexpr float kCursorTipU = 0.33691406f;  // 172.5 / 512
inline constexpr float kCursorTipV = 0.12304688f;  // 63 / 512

inline void DrawAnimatedCursor(ImDrawList* dl, ID3D11ShaderResourceView* srv,
                               float mx, float my, bool down, float dt,
                               float alpha) {
    if (!dl || !srv) return;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    static anim::Spring s_click(1.0f);
    static anim::Spring s_sx(1.0f);
    static anim::Spring s_sy(1.0f);
    static anim::Spring s_tilt(0.0f);
    static float s_lastX = -1.0f, s_lastY = -1.0f;
    static bool s_tuned = false;
    if (!s_tuned) {
        s_tuned = true;
        s_click.stiffness = 500.0f; s_click.damping = 24.0f;
        s_sx.stiffness = 220.0f;    s_sx.damping = 12.0f;
        s_sy.stiffness = 220.0f;    s_sy.damping = 12.0f;
        s_tilt.stiffness = 180.0f;  s_tilt.damping = 16.0f;
        s_lastX = mx; s_lastY = my;
    }

    float vx = 0.0f, vy = 0.0f;
    if (s_lastX >= 0.0f) { vx = (mx - s_lastX) / dt; vy = (my - s_lastY) / dt; }
    s_lastX = mx; s_lastY = my;
    float speed = sqrtf(vx * vx + vy * vy);
    if (speed > 40000.0f) { vx = 0.0f; vy = 0.0f; speed = 0.0f; }

    // Click squish.
    s_click.SetTarget(down ? 0.80f : 1.0f);
    s_click.Update(dt);

    // Motion stretch along travel direction.
    const float kMaxStretch = 0.22f;
    const float kRefSpeed   = 2600.0f;
    const float along = utils::Clamp(speed / kRefSpeed, 0.0f, 1.0f) * kMaxStretch;
    float tx = 1.0f, ty = 1.0f;
    if (speed > 1.0f) {
        const float wx = fabsf(vx) / speed, wy = fabsf(vy) / speed;
        tx = 1.0f + along * wx - along * 0.5f * wy;
        ty = 1.0f + along * wy - along * 0.5f * wx;
    }
    s_sx.SetTarget(tx); s_sx.Update(dt);
    s_sy.SetTarget(ty); s_sy.Update(dt);

    // Lean into horizontal motion (sprung tilt about the tip).
    s_tilt.SetTarget(utils::Clamp(vx * 0.00004f, -0.22f, 0.22f));
    s_tilt.Update(dt);

    const float cs = s_click.value;
    const float base = 30.0f * cs;
    const int a = (int)(255.0f * utils::Clamp(alpha, 0.0f, 1.0f));
    const float th = s_tilt.value;
    const float c = cosf(th), s = sinf(th);

    // Quad corners in cursor space, with the ink tip at the origin so that
    // stretch and lean both pivot on the point being clicked.
    const float lx = -kCursorTipU * base, ty0 = -kCursorTipV * base;
    const float rx = base - kCursorTipU * base, by = base - kCursorTipV * base;

    ImVec2 quad[4];
    auto buildQuad = [&](float scale, float offX, float offY) {
        auto put = [&](float ex, float ey) -> ImVec2 {
            const float px = ex * scale * s_sx.value;
            const float py = ey * scale * s_sy.value;
            return ImVec2(mx + offX + px * c - py * s, my + offY + px * s + py * c);
        };
        quad[0] = put(lx, ty0);
        quad[1] = put(rx, ty0);
        quad[2] = put(rx, by);
        quad[3] = put(lx, by);
    };
    const ImVec2 uv0(0, 0), uv1(1, 0), uv2(1, 1), uv3(0, 1);

    // Soft drop shadow, then a thin dark rim: both scale about the tip, so the
    // separation stays exactly one pixel at the point the user is aiming with.
    buildQuad(1.16f, 2.5f, 3.5f);
    dl->AddImageQuad((ImTextureID)srv, quad[0], quad[1], quad[2], quad[3],
                     uv0, uv1, uv2, uv3, IM_COL32(0, 0, 0, (int)(a * 0.30f)));
    if (a > 6) {
        buildQuad(1.075f, 0.0f, 0.0f);
        dl->AddImageQuad((ImTextureID)srv, quad[0], quad[1], quad[2], quad[3],
                         uv0, uv1, uv2, uv3, IM_COL32(0, 0, 0, (int)(a * 0.52f)));
    }

    buildQuad(1.0f, 0.0f, 0.0f);
    dl->AddImageQuad((ImTextureID)srv, quad[0], quad[1], quad[2], quad[3],
                     uv0, uv1, uv2, uv3, IM_COL32(255, 255, 255, a));
}

} // namespace backend
} // namespace opal
