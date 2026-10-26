#pragma once
#include "../imgui/imgui.h"

//
// opal_draw.h — small draw helpers shared by accent-tinted widgets.
//
// Rounded-gradient rects: ImGui's AddRectFilledMultiColor produces sharp
// corners (no rounding), so instead we let ImGui tessellate a rounded rect
// (via AddRectFilled) and then recolor each emitted vertex by its position.
// CRITICAL: we preserve the original vertex alpha — AddRectFilled bakes
// anti-aliasing into the edge-vertex alpha channel; overwriting it would
// destroy the AA and produce jagged/pixelated edges.
//

namespace opal::draw {

// Horizontal gradient (colL at left edge -> colR at right edge), AA preserved.
inline void RoundedGradientRectH(ImDrawList* dl, ImVec2 tl, ImVec2 br,
                                 ImU32 colL, ImU32 colR, float rounding)
{
    const int vtx_start = dl->VtxBuffer.Size;
    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 255), rounding);
    float w = br.x - tl.x;
    if (w < 1.0f) w = 1.0f;
    for (int i = vtx_start; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float t = (v.pos.x - tl.x) / w;
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        const float i1 = 1.0f - t;
        int rr = (int)(((float)((colL >> IM_COL32_R_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_R_SHIFT) & 0xFF)) * t);
        int gg = (int)(((float)((colL >> IM_COL32_G_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_G_SHIFT) & 0xFF)) * t);
        int bb = (int)(((float)((colL >> IM_COL32_B_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_B_SHIFT) & 0xFF)) * t);
        // PRESERVE AA alpha — AddRectFilled set it per-vertex for smooth edges
        int aa = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
        v.col = IM_COL32(rr, gg, bb, aa);
    }
}

// Diagonal gradient (colL at bottom-left -> colR at top-right), AA preserved.
inline void RoundedGradientRectDiag(ImDrawList* dl, ImVec2 tl, ImVec2 br,
                                    ImU32 colL, ImU32 colR, float rounding)
{
    const int vtx_start = dl->VtxBuffer.Size;
    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 255), rounding);
    float w = br.x - tl.x; if (w < 1.0f) w = 1.0f;
    float h = br.y - tl.y; if (h < 1.0f) h = 1.0f;
    for (int i = vtx_start; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float tx = (v.pos.x - tl.x) / w;
        float ty = (v.pos.y - tl.y) / h;
        if (tx < 0.0f) tx = 0.0f; if (tx > 1.0f) tx = 1.0f;
        if (ty < 0.0f) ty = 0.0f; if (ty > 1.0f) ty = 1.0f;
        float t = (tx + (1.0f - ty)) * 0.5f;
        const float i1 = 1.0f - t;
        int rr = (int)(((float)((colL >> IM_COL32_R_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_R_SHIFT) & 0xFF)) * t);
        int gg = (int)(((float)((colL >> IM_COL32_G_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_G_SHIFT) & 0xFF)) * t);
        int bb = (int)(((float)((colL >> IM_COL32_B_SHIFT) & 0xFF)) * i1 + ((float)((colR >> IM_COL32_B_SHIFT) & 0xFF)) * t);
        // PRESERVE AA alpha
        int aa = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
        v.col = IM_COL32(rr, gg, bb, aa);
    }
}

} // namespace opal::draw
