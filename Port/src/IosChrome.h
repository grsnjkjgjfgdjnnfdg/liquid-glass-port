#pragma once
#include "Ui.h"

namespace sw {

// ===========================================================================
// Shared iOS chrome: status bar, disclosure chevron, tracked text, glyphs
// placed by their ink box, and the outline microphone of the search field.
//
// Every constant here is a measurement, not a design value. Sources:
//   * Reference/iOS27Trees/10-settings-root.png            (1206x2622 = 3x)
//   * Reference/iOS27/.../named/01-light-controls_0.png    (3x)
//   * Reference/iOS27Trees/settings-root-tree.json         (XCUIElement frames)
// The 3x captures are measured at native resolution and divided by 3, so a
// third of a point of error is visible.
// ===========================================================================

// ImGui's font atlas bakes one optical-tracking value per size bucket
// (main.cpp: GlyphExtraAdvanceX = 0.5 * size/17 * kTextScale). The runtime's
// tracking is not linear in size, so a per-call delta is needed to land the
// *ink width* on the measured one. `delta` is in points per glyph.
inline void IosTextDelta(UI& ui, ImFont* f, float size, float x, float y,
                         const char* s, const Rgba& c, float delta) {
    if (!f || !s || !*s) return;
    const float px = size * kTextScale;
    std::string prefix;
    prefix.reserve(64);
    float prevW = 0.0f;
    // ImGui truncates a glyph origin to whole points, so the fractional part is
    // applied to the finished run's vertices instead (see UI::TextEx).
    const float bx = std::floor(x);
    const float by = std::floor(y);
    const int mark = ui.dl->VtxBuffer.Size;
    float cx = bx;
    for (const char* p = s; *p; ++p) {
        char buf[2] = { *p, 0 };
        ui.dl->AddText(f, px, ImVec2(cx, by), c.U32(), buf);
        prefix.push_back(*p);
        const float w = f->CalcTextSizeA(px, FLT_MAX, 0.0f, prefix.c_str()).x;
        cx += (w - prevW) + delta;
        prevW = w;
    }
    const float ox = x - bx;
    const float oy = y - by;
    if (ox != 0.0f || oy != 0.0f) ui.VtxTranslate(mark, ox, oy);
}

inline float IosTextWidth(ImFont* f, float size, const char* s, float delta) {
    if (!f || !s || !*s) return 0.0f;
    const float px = size * kTextScale;
    const float w = f->CalcTextSizeA(px, FLT_MAX, 0.0f, s).x;
    const size_t n = std::strlen(s);
    return w + delta * (float)(n > 0 ? n - 1 : 0);
}

// Horizontally centred tracked text inside `r` (integer-snapped like
// UI::TextCentered so the glyph phase matches the rest of the page).
inline void IosTextCentered(UI& ui, ImFont* f, float size, const Rect& r,
                            const char* s, const Rgba& c, float delta) {
    const float w = IosTextWidth(f, size, s, delta);
    IosTextDelta(ui, f, size, std::round(r.x + (r.w - w) * 0.5f), r.y, s, c, delta);
}

// ---------------------------------------------------------------------------
// SF Symbol placed by its ink box.
//
// The atlas bitmaps are already cropped to the glyph's ink (verified with
// tools/atlas_ink.py: every symbol's alpha bounding box equals its texture),
// so the texture can be fitted straight into the measured ink rectangle.
// `grow` dilates the glyph by that many points on each side for weight; the
// base quad is inset by the same amount so the *envelope* stays the ink box.
// ---------------------------------------------------------------------------
inline void IosGlyph(UI& ui, SfIcon id, const Rect& ink, const Rgba& c,
                     float grow = 0.0f) {
    if (!ui.icons || !ui.icons->Ready()) return;
    const SfTexture& t = ui.icons->Get(id);
    if (!t.srv) return;
    Rect base(ink.x + grow, ink.y + grow, ink.w - grow * 2.0f, ink.h - grow * 2.0f);
    if (base.w <= 0.0f || base.h <= 0.0f) base = ink;
    const float aspect = (float)t.w / (float)t.h;
    float w = base.w, h = base.h;
    if (w / h > aspect) w = h * aspect; else h = w / aspect;
    const float x = base.x + (base.w - w) * 0.5f;
    const float y = base.y + (base.h - h) * 0.5f;
    const float off[5][2] = { { 0, 0 }, { grow, 0 }, { -grow, 0 }, { 0, grow }, { 0, -grow } };
    const int n = grow > 0.0f ? 5 : 1;
    for (int i = 0; i < n; ++i) {
        ui.dl->AddImage((ImTextureID)t.srv,
                        ImVec2(x + off[i][0], y + off[i][1]),
                        ImVec2(x + off[i][0] + w, y + off[i][1] + h),
                        ImVec2(0, 0), ImVec2(1, 1), c.U32());
    }
}

// ---------------------------------------------------------------------------
// Disclosure indicator. Measured ink 7 x 11.67 pt, a round-capped 2 pt stroke
// whose darkest pixel is 188/255 (so the ink is #3C3C43 at 34 %, not the 22 %
// the first round guessed). (x, yTop) is the top-left of the ink box.
// ---------------------------------------------------------------------------
inline void IosChevron(UI& ui, float x, float yTop, float h = 11.67f) {
    const float th = 2.0f;
    Rgba ink = ui.theme.dark ? Rgba(1, 1, 1, 0.36f) : Rgba(0.235f, 0.235f, 0.263f, 0.34f);
    ui.dl->PathLineTo(ImVec2(x + th * 0.5f, yTop + th * 0.5f));
    ui.dl->PathLineTo(ImVec2(x + 7.0f - th * 0.5f, yTop + h * 0.5f));
    ui.dl->PathLineTo(ImVec2(x + th * 0.5f, yTop + h - th * 0.5f));
    ui.dl->PathStroke(ink.U32(), 0, th);
}

// ---------------------------------------------------------------------------
// Outline microphone for the search field. The runtime draws the *outline*
// mic (SF Symbol `mic`), not `mic.fill`, which is what the atlas holds; every
// element below is measured from the 3x capture:
//   capsule ring   outer 6.0 x 11.67 at (347, 812.67), stroke 1.6
//   holder U       outer radius 6.2 centred (350.2, 821.1), stroke 1.67,
//                  vertical stubs to y 819.3
//   stem           1.67 wide, y 827.5 .. 829.4
//   base           8.33 x 1.33 at (345.67, 829.33), radius 0.67
// ---------------------------------------------------------------------------
inline void IosSearchMic(UI& ui, const Rgba& ink) {
    const ImU32 col = ink.U32();
    // capsule (hollow)
    ui.dl->AddRect(ImVec2(347.8f, 813.47f), ImVec2(352.2f, 823.53f), col,
                   2.2f, 0, 1.6f);
    // holder: vertical stubs + bottom half circle
    const float cx = 350.2f, cy = 821.1f, r = 5.37f;
    ui.dl->PathLineTo(ImVec2(cx + r, 820.1f));
    ui.dl->PathArcTo(ImVec2(cx, cy), r, 0.0f, 3.14159265f, 24);
    ui.dl->PathLineTo(ImVec2(cx - r, 820.1f));
    ui.dl->PathStroke(col, 0, 1.67f);
    // stem + base
    ui.dl->AddRectFilled(ImVec2(349.33f, 827.5f), ImVec2(351.0f, 829.4f), col);
    ui.dl->AddRectFilled(ImVec2(345.67f, 829.33f), ImVec2(354.0f, 830.67f), col, 0.67f);
}

// ---------------------------------------------------------------------------
// Status bar. Measured from 10-settings-root.png (the controls page carries the
// same bar): 9:41 ink (63.33, 25.67) 14.67 x 13.33; four cellular bars 3.33 wide
// on a 5.67 pitch, all ending at y 39.67 with heights 6.33/8.67/11/13.67;
// Wi-Fi symbol ink 19 x 14.33 at (306, 25.33); charging battery: body
// 26.33 x 13.67 at (333, 25.33) r 3.7 in #34C759, nub 2 x 5 at (360, 29.67),
// white bolt ~8 x 11.5 inside.
// ---------------------------------------------------------------------------
inline void IosStatusBar(UI& ui, bool lightContent = false) {
    const Theme& th = ui.theme;
    const bool light = lightContent || th.dark;
    Rgba ink = light ? Rgba(1, 1, 1, 0.97f) : Rgba(0, 0, 0, 1.0f);
    IosTextDelta(ui, ui.fonts.semibold17, 17.0f, 63.0f, 22.4f, "9:41", ink, 0.0f);

    const float barH[4] = { 6.33f, 8.67f, 11.0f, 13.67f };
    for (int i = 0; i < 4; ++i) {
        float x = 277.0f + i * 5.67f;
        ui.dl->AddRectFilled(ImVec2(x, 39.67f - barH[i]), ImVec2(x + 3.33f, 39.67f),
                             ink.U32(), 1.67f);
    }
    IosGlyph(ui, SfIcon::Wifi, Rect(306.0f, 25.5f, 19.0f, 14.3f), ink);

    // Battery: solid green body, green nub, white bolt.
    const Rgba green = light ? Rgba::Hex(0x30D158) : Rgba::Hex(0x34C759);
    ui.dl->AddRectFilled(ImVec2(360.0f, 29.67f), ImVec2(362.0f, 34.67f), green.U32(), 1.0f);
    ui.dl->AddRectFilled(ImVec2(333.0f, 25.33f), ImVec2(359.33f, 39.0f), green.U32(), 3.7f);
    IosGlyph(ui, SfIcon::Bolt, Rect(342.0f, 26.9f, 8.3f, 11.5f),
             light ? Rgba(0, 0, 0, 1) : Rgba(1, 1, 1, 1));
}

} // namespace sw
