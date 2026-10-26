#pragma once
#include "../imgui/imgui.h"
#include "opal_types.h"
#include "glass/glass_backdrop_blur.h"

//
// opal_effects.h — soft visual effects composited onto the UI.
//
// AccentGlow: a very tight, subtle coloured halo that bleeds from accent-
//             bearing elements into adjacent widgets.  It extends only a
//             handful of pixels outward so the glow lands on the closest
//             neighbour, not on empty glass or the background.
//
//             Concentric rounded rects, quadratic alpha falloff.
//             Peak ~18 alpha, total spread ~8 px — barely there on its
//             own, but builds cohesion between neighbouring controls.
//

namespace opal::fx {

inline void AccentGlow(ImDrawList* dl, Rect bounds, float radius,
                       ImU32 accentCol, float intensity = 1.0f)
{
    if (!dl) return;
    constexpr int layers  = 4;
    float         baseA   = 18.0f * intensity;
    if (baseA < 0.5f) return;

    int r = (int)((accentCol >> IM_COL32_R_SHIFT) & 0xFF);
    int g = (int)((accentCol >> IM_COL32_G_SHIFT) & 0xFF);
    int b = (int)((accentCol >> IM_COL32_B_SHIFT) & 0xFF);

    for (int i = 0; i < layers; ++i) {
        float t      = (float)i / (float)(layers - 1);
        float expand = t * 8.0f;                         // max spread 8 px
        float alpha  = baseA * (1.0f - t) * (1.0f - t); // quadratic decay
        if (alpha < 0.5f) continue;
        int a = (int)(alpha + 0.5f);
        dl->AddRectFilled(
            ImVec2(bounds.x - expand, bounds.y - expand),
            ImVec2(bounds.x + bounds.w + expand, bounds.y + bounds.h + expand),
            IM_COL32(r, g, b, a), radius + expand);
    }
}

// Even tighter variant for small elements (toggles, checkboxes) where the
// default 8 px spread would bleed past the next label.
inline void AccentGlowTight(ImDrawList* dl, Rect bounds, float radius,
                            ImU32 accentCol, float intensity = 1.0f)
{
    if (!dl) return;
    constexpr int layers = 3;
    float baseA = 16.0f * intensity;
    if (baseA < 0.5f) return;

    int r = (int)((accentCol >> IM_COL32_R_SHIFT) & 0xFF);
    int g = (int)((accentCol >> IM_COL32_G_SHIFT) & 0xFF);
    int b = (int)((accentCol >> IM_COL32_B_SHIFT) & 0xFF);

    for (int i = 0; i < layers; ++i) {
        float t      = (float)i / (float)(layers - 1);
        float expand = t * 5.0f;                         // max spread 5 px
        float alpha  = baseA * (1.0f - t) * (1.0f - t);
        if (alpha < 0.5f) continue;
        int a = (int)(alpha + 0.5f);
        dl->AddRectFilled(
            ImVec2(bounds.x - expand, bounds.y - expand),
            ImVec2(bounds.x + bounds.w + expand, bounds.y + bounds.h + expand),
            IM_COL32(r, g, b, a), radius + expand);
    }
}

} // namespace opal::fx

// ---------------------------------------------------------------------------
// RevealBlur — a true Gaussian reveal for a single region.
//
// The region is covered with a real blur of the live frame (which already
// contains whatever was drawn there) and that veil crossfades away as `reveal`
// goes 0 -> 1. The element therefore materialises out of genuine blur instead
// of a ghost smear.
//
// The radius stays constant and the ALPHA crossfades. That is deliberate:
// perceived Gaussian softness saturates at high radii and collapses at low
// ones, so animating the radius holds the element blurry and then snaps it
// sharp. Crossfading against the sharp element underneath resolves at a
// constant rate and reads as a smooth focus pull.
//
// Call this immediately AFTER drawing the element, passing its own rect.
// `pad` grows the region so the blurred edge does not seam against the sharp
// surround. Every menu that renders through the framework gets this wherever it
// is wired (menu-open stages, tab transitions, group rows).
// ---------------------------------------------------------------------------
namespace opal::fx {
inline void RevealBlur(ImDrawList* dl, Rect bounds, float reveal,
                       float radius = 8.0f, float pad = 0.0f)
{
    if (!dl) return;
    float r = reveal < 0.0f ? 0.0f : (reveal > 1.0f ? 1.0f : reveal);
    if (r >= 0.999f) return;
    float w = 1.0f - (r * r * (3.0f - 2.0f * r));   // smoothstep crossfade
    if (w <= 0.004f) return;
    glass::BackdropBlurComposite(dl, bounds, radius, w, pad);
}
} // namespace opal::fx
