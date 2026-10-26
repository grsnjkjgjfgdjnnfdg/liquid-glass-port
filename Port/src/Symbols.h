#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "SfIcons.h"

namespace sw {

// Draws one real SF Symbol from the embedded atlas, fitted into `box` with its
// natural aspect ratio and centred, tinted by `colour`.
//
// `optical` is the fraction of the box the glyph should occupy; SF Symbols carry
// their own padding so a 28 pt row icon is drawn at ~0.78 of the tile.
inline void DrawSymbol(const SfIcons& icons, SfIcon id, ImDrawList* dl, const Rect& box,
                       const Rgba& colour, float optical = 0.74f) {
    const SfTexture& tex = icons.Get(id);
    if (!tex.srv) return;
    float aspect = (float)tex.w / (float)tex.h;
    float w = box.w * optical;
    float h = w / aspect;
    if (h > box.h * optical) {
        h = box.h * optical;
        w = h * aspect;
    }
    float x = box.x + (box.w - w) * 0.5f;
    float y = box.y + (box.h - h) * 0.5f;
    dl->AddImage((ImTextureID)tex.srv, ImVec2(x, y), ImVec2(x + w, y + h),
                 ImVec2(0, 0), ImVec2(1, 1), colour.U32());
}

// SF Symbols at small sizes read heavier than the outline artwork carries at 1x.
// Dilating the glyph by a fraction of a pixel in four directions matches the
// runtime's apparent stroke weight without shipping a second atlas.
inline void DrawSymbolBold(const SfIcons& icons, SfIcon id, ImDrawList* dl, const Rect& box,
                           const Rgba& colour, float optical = 0.74f, float grow = 0.3f) {
    DrawSymbol(icons, id, dl, box, colour, optical);
    const float off[4][2] = { { grow, 0.0f }, { -grow, 0.0f }, { 0.0f, grow }, { 0.0f, -grow } };
    for (int i = 0; i < 4; ++i) {
        Rect b(box.x + off[i][0], box.y + off[i][1], box.w, box.h);
        DrawSymbol(icons, id, dl, b, colour, optical);
    }
}

} // namespace sw
