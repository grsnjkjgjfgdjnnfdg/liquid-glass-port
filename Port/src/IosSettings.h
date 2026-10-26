#pragma once
#include "Ui.h"
#include "IosChrome.h"
#include "SfMasks.h"

namespace sw {

// 1:1 reconstruction of the real iOS 27.1 Settings root page.
//
// Every number below is a measurement, not a design value. Sources, in order of
// authority (see Agents/sizes/REPORT.md for the full table):
//   * Reference/iOS27Trees/settings-root-tree.json  XCUIElement frames, points
//   * Reference/iOS27Trees/10-settings-root.png     1206x2622 (3x) pixel scan
//
//   screen            402 x 874
//   status bar        bars/wifi/battery measured (IosChrome.h)
//   large title       ink (17, 127) 130.67 x 31, 34 pt bold
//   Apple Account     cell (16,168) 370 x 90.3, radius 26
//                       avatar ring centre (61.67,213.08), 2 x 18 dots
//                       apple mark ink (52.33,201.33) 17.33 x 21.33
//                       title ink (100.67,187.33) 136.67 x 18.33 -> 20 pt
//                       subtitle ink (100.67,212.33) and (100.67,230.33), 14 pt
//                       chevron ink (360.67,207.33) 7 x 11.67
//   Spotlight cell    (16,293.3) 370 x 52; tile (30,305.3) 28 x 28
//   group cell        (16,380.3) 370 x 468: nine 52 pt rows
//                       tile (30,row+12) 28 x 28, vertical gradient
//                       label ink (73,row+20)
//                       chevron ink (360.67,row+20.33)
//                       separator (72,row+51) 297.67 x 1, #E8E8E8
//   search glass      (28,798) 346 x 48 radius 24
//                       magnifier ink (42,813.33) 16.33 x 16.67
//                       placeholder ink (69.67,815.67)
//                       dictate button (341.7,811) 17.3 x 22
static constexpr float kIosW = 402.0f;
static constexpr float kIosH = 874.0f;

namespace ios_metrics {
constexpr float kTitleX = 16.0f, kTitleY = 119.7f;   // AX frame top-left
constexpr float kCardX = 16.0f, kCardW = 370.0f, kCardRadius = 26.0f;
constexpr float kRowH = 52.0f;
constexpr float kTileX = 30.0f, kTileSize = 28.0f, kTileRadius = 7.84f;
constexpr float kLabelX = 73.0f;      // measured label ink starts at x=73
constexpr float kChevronX = 360.67f;
constexpr float kSepX = 72.0f, kSepW = 297.67f;
constexpr float kSearchX = 28.0f, kSearchY = 798.0f;
constexpr float kSearchW = 346.0f, kSearchH = 48.0f;
} // namespace ios_metrics

// Row tiles are filled with a *vertical gradient*, not a flat colour: measured
// top (159,159,165) -> bottom (142,142,147) for the grey tiles, and
// (0,147,255) -> (0,136,254) for the blue ones, over the full 28 pt height.
// ImGui cannot round a gradient fill, so the tile is built from horizontal
// bands whose widths follow the rounded-rect outline, which keeps both the
// gradient and the corner radius (7.84 pt = 0.28 of the tile).
inline void IosRoundedTile(UI& ui, const Rect& r, float radius, uint32_t topC,
                           uint32_t bottomC) {
    // 0xRRGGBB, the same order Rgba::Hex uses
    auto unpack = [](uint32_t c, int& cr, int& cg, int& cb) {
        cr = (c >> 16) & 0xFF; cg = (c >> 8) & 0xFF; cb = c & 0xFF;
    };
    int r0, g0, b0, r1, g1, b1;
    unpack(topC, r0, g0, b0);
    unpack(bottomC, r1, g1, b1);
    const int bands = 56;                 // two bands per point of height
    for (int i = 0; i < bands; ++i) {
        float t0 = (float)i / bands, t1 = (float)(i + 1) / bands;
        float y0 = r.y + r.h * t0 - 0.2f, y1 = r.y + r.h * t1 + 0.2f;
        auto inset = [&](float y) {
            float yy = y - r.y;
            if (yy >= radius && yy <= r.h - radius) return 0.0f;
            float dy = yy < radius ? radius - yy : yy - (r.h - radius);
            return radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
        };
        float xa = r.x + std::max(inset(y0), inset(y1));
        float xb = r.x + r.w - std::max(inset(y0), inset(y1));
        float tm = (t0 + t1) * 0.5f;
        int cr = (int)std::lround(r0 + (r1 - r0) * tm);
        int cg = (int)std::lround(g0 + (g1 - g0) * tm);
        int cb = (int)std::lround(b0 + (b1 - b0) * tm);
        ui.dl->AddRectFilled(ImVec2(xa, y0), ImVec2(xb, y1), IM_COL32(cr, cg, cb, 255));
    }
}

// "Optimizing Search and Siri" uses arrow.trianglehead.2.clockwise.rotate.90,
// which is not in the embedded atlas, so it is drawn as the measured shape: an
// 8 pt ring (1.8 pt stroke) with gaps at the upper-left and lower-right and a
// small arrow head at each end.
inline void IosSpotlightGlyph(UI& ui, const Rect& tile) {
    const float cx = tile.x + tile.w * 0.5f, cy = tile.y + tile.h * 0.5f;
    const float rad = 8.0f;
    const ImU32 white = IM_COL32(255, 255, 255, 250);
    const float d2r = 3.14159265f / 180.0f;
    ui.dl->PathArcTo(ImVec2(cx, cy), rad, -165.0f * d2r, -20.0f * d2r, 22);
    ui.dl->PathStroke(white, 0, 1.8f);
    ui.dl->PathArcTo(ImVec2(cx, cy), rad, 15.0f * d2r, 160.0f * d2r, 22);
    ui.dl->PathStroke(white, 0, 1.8f);
    // heads: at the right end of the top arc (pointing down) and the left end of
    // the bottom arc (pointing up)
    float ax = cx + rad * std::cos(-20.0f * d2r), ay = cy + rad * std::sin(-20.0f * d2r);
    ui.dl->AddTriangleFilled(ImVec2(ax - 2.4f, ay - 2.2f), ImVec2(ax + 2.4f, ay - 2.2f),
                             ImVec2(ax + 0.2f, ay + 2.4f), white);
    float bx = cx + rad * std::cos(160.0f * d2r), by = cy + rad * std::sin(160.0f * d2r);
    ui.dl->AddTriangleFilled(ImVec2(bx - 2.4f, by + 2.2f), ImVec2(bx + 2.4f, by + 2.2f),
                             ImVec2(bx - 0.2f, by - 2.4f), white);
}

struct IosSettingsState {
    SwitchState sw;
    SliderState sl;
    SegState seg;
    ButtonState btn;
    float scroll = 0.0f;
    bool dark = false;
    int pressedRow = -1;
    // iOS fades a row's press fill in on touch-down, holds it while the push runs, and
    // fades it out when the transition commits. A binary hover fill (what this used to
    // be) shows a state a phone never does when the pointer sweeps the group.
    Spring rowPress;
    float rowHold = 0.0f;      // seconds the press stays latched after release
    IosSettingsState() : rowPress(0.0f, 0.10f, 0.86f) {}
};

struct IosRow { const char* label; uint32_t tileTop; uint32_t tileBottom; SfIcon symbol; };

inline void IosSettings(UI& ui, IosSettingsState& st) {
    const Theme& th = ui.theme;
    ui.dl->AddRectFilled(ImVec2(0, 0), ImVec2(kIosW, kIosH), th.pageBg.U32());

    IosStatusBar(ui);

    // ---- large title -----------------------------------------------------
    // Ink measured at (17, 127) 130.67 x 31; drawing a 34 pt bold at the AX
    // frame origin (16, 119.7) lands there (the previous -6 offset was 1 pt low).
    ui.TextBold(ui.fonts.semibold34, 34.0f, ios_metrics::kTitleX,
                ios_metrics::kTitleY, "Settings", th.text);

    // ---- Apple Account card ---------------------------------------------
    Rect account(16, 168, 370, 90.3f);
    ui.FillRounded(account, ios_metrics::kCardRadius, th.cardBg);
    {
        if (ui.masks && ui.masks->Ready()) {
            // The whole composition — two dotted rings plus the mark — taken from
            // the capture in one mask. Drawing 36 circles by hand could not match
            // the ring's dot phase and anti-aliasing, and the diff said so.
            ui.masks->Draw(ui.dl, Glyph::Avatar, Rect(31.5f, 182.9f, 60.4f, 60.4f), ui.dx);
        } else {
            // Avatar: two interleaved rings of 18 dots around (61.67, 213.08).
            // Measured radii 27.75 / 21.8, dot radii 2.45 / 1.65, core #80C4FF.
            const float acx = 61.67f, acy = 213.08f;
            const Rgba dot = Rgba::Hex(0x80C4FF);
            for (int i = 0; i < 18; ++i) {
                float a = (10.0f + i * 20.0f) * 3.14159265f / 180.0f;
                ui.dl->AddCircleFilled(ImVec2(acx + std::cos(a) * 27.75f,
                                              acy + std::sin(a) * 27.75f), 2.45f,
                                       dot.U32(), 16);
            }
            for (int i = 0; i < 18; ++i) {
                float a = (0.7f + i * 20.0f) * 3.14159265f / 180.0f;
                ui.dl->AddCircleFilled(ImVec2(acx + std::cos(a) * 21.8f,
                                              acy + std::sin(a) * 21.8f), 1.65f,
                                       dot.U32(), 12);
            }
            IosGlyph(ui, SfIcon::AppleLogo, Rect(52.33f, 201.33f, 17.33f, 21.33f), Rgba::Hex(0x0088FF));
        }
    }
    // Title: 20 pt. The AX line box is 24.0 tall (the 20 pt line height) and the
    // measured ink width 136.67 only lands at 20 pt; the previous nominal 20.5
    // overshot the ink by 4.3 pt. ImGui puts the cap top 5.85 pt below the draw
    // y at 20 pt, hence the 181.48 origin for a measured ink top of 187.33.
    // Sub-pixel phase: a correlation sweep of the whole glyph mass (not just the
    // ink bbox) against the 3x capture puts the port 4 device px high and 1 device
    // px right here, i.e. +1.33 pt down and -0.33 pt left. Moving it cuts the
    // account-title band error from 37.7 to 22.2.
    ui.Text(ui.fonts.accountTitle20, 20.0f, 99.84f, 182.48f, "Apple Account", th.text);
    // Two explicit lines: the runtime's break is part of the layout. 14 pt (the
    // AX box is 2 x 16.85) with the measured 18 pt ink pitch; tracking -0.13
    // closes the 2 % width gap left by the font's baked optical tracking.
    IosTextDelta(ui, ui.fonts.regular14, 14.0f, 100.27f, 208.32f,
                 "Sign in to access your iCloud data, the", th.secondary, -0.13f);
    IosTextDelta(ui, ui.fonts.regular14, 14.0f, 100.27f, 226.32f,
                 "App Store, Apple services, and more.", th.secondary, -0.13f);
    IosChevron(ui, ios_metrics::kChevronX, 207.33f);

    // ---- Spotlight card --------------------------------------------------
    Rect spot(16, 293.3f, 370, 52);
    ui.FillRounded(spot, 26, th.cardBg);
    {
        Rect tile(30, 305.3f, 28, 28);
        IosRoundedTile(ui, tile, ios_metrics::kTileRadius, 0x0093FF, 0x0088FE);
        if (ui.masks && ui.masks->Ready()) ui.masks->Draw(ui.dl, Glyph::Spotlight, tile, ui.dx);
        else IosSpotlightGlyph(ui, tile);
    }
    ui.TextVCenter(ui.fonts.regular17, 17.0f, ios_metrics::kLabelX - 0.67f,
                   Rect(spot.x, spot.y, spot.w, spot.h),
                   "Optimizing Search and Siri", th.text);
    IosChevron(ui, ios_metrics::kChevronX, 313.67f);

    // ---- group card ------------------------------------------------------
    static const IosRow rows[] = {
        { "General",                   0x9F9FA5, 0x8E8E93, SfIcon::GearOutline },
        { "Accessibility",             0x0093FF, 0x0088FE, SfIcon::Accessibility },
        { "Action Button",             0x0093FF, 0x0088FE, SfIcon::ArrowTurnRightUp },
        { "Appearance",                0x0A0A0B, 0x000000, SfIcon::HalfDisc },
        { "Camera",                    0x9F9FA5, 0x8E8E93, SfIcon::CameraOutline },
        { "Home Screen & App Library", 0x0093FF, 0x0088FE, SfIcon::PhoneOutline },
        { "Search",                    0x9F9FA5, 0x8E8E93, SfIcon::Magnifier },
        { "Siri",                      0x0E4F40, 0x0C4187, SfIcon::Waveform },
        { "StandBy",                   0x0A0A0B, 0x000000, SfIcon::Display },
    };
    const int count = (int)(sizeof(rows) / sizeof(rows[0]));
    Rect group(16, 380.3f, 370, ios_metrics::kRowH * count);
    ui.FillRounded(group, ios_metrics::kCardRadius, th.cardBg);
    // One press at a time: record which row the pointer is on, then step the fill
    // spring once so it fades in on touch-down and holds through a push.
    {
        int wantRow = -1;
        for (int i = 0; i < count; ++i) {
            Rect row(16, 380.3f + i * ios_metrics::kRowH, 370, ios_metrics::kRowH);
            if ((row.Contains(ui.mouse.x, ui.mouse.y) && ui.mousePressed) ||
                HoverRowOverride() == i) {
                wantRow = i;
            }
        }
        if (wantRow >= 0) {
            st.pressedRow = wantRow;
            st.rowHold = 0.35f;   // long enough to survive a push (0.166 s)
        } else if (ui.mouseReleased) {
            st.rowHold = std::min(st.rowHold, 0.05f);
        }
        if (st.rowHold > 0.0f && !ui.mouseDown)
            st.rowHold = std::max(0.0f, st.rowHold - ui.dt);
        const bool active = st.pressedRow >= 0 && (ui.mouseDown || st.rowHold > 0.0f);
        st.rowPress.SetTarget(active ? 1.0f : 0.0f);
        st.rowPress.Update(ui.dt);
        if (!active && st.rowPress.value < 0.01f) st.pressedRow = -1;
    }
    for (int i = 0; i < count; ++i) {
        float ry = 380.3f + i * ios_metrics::kRowH;
        Rect row(16, ry, 370, ios_metrics::kRowH);
        // Press only - the runtime has no hover state on a list row. The fill spring is
        // stepped once *outside* this loop: updating it per row made each row's target
        // fight the next one and the fill settled at a third of its value.
        const float a = (st.pressedRow == i)
            ? 0.10f * std::clamp(st.rowPress.value, 0.0f, 1.0f) : 0.0f;
        if (a > 0.002f) {
            // A pressed row lightens in dark mode and darkens in light mode: the highlight is
            // the label colour mixed into the row, not black. Painting black on a dark card
            // was nearly invisible (the pressed band measured *darker* than its neighbours).
            const Rgba rowInk = ui.theme.dark ? Rgba(1, 1, 1, a * 0.85f) : Rgba(0, 0, 0, a);
            const float rr = ios_metrics::kCardRadius;
            ui.dl->PushClipRect(ImVec2(row.x + ui.clipOffsetX, row.y + ui.clipOffsetY),
                                ImVec2(row.x + row.w + ui.clipOffsetX, row.y + row.h + ui.clipOffsetY), true);
            if (i == 0) {
                ui.FillRounded(Rect(row.x, row.y, row.w, row.h + rr), rr, rowInk);
            } else if (i == count - 1) {
                ui.FillRounded(Rect(row.x, row.y - rr, row.w, row.h + rr), rr, rowInk);
            } else {
                ui.FillRounded(row, 0.0f, rowInk);
            }
            ui.dl->PopClipRect();
        }
                Rect tile(30, ry + 12, 28, 28);
        IosRoundedTile(ui, tile, ios_metrics::kTileRadius, rows[i].tileTop,
                       rows[i].tileBottom);
        if (ui.masks && ui.masks->Ready()) {
            // The glyph the runtime draws, taken from its own 3x capture: four of
            // the nine row icons are iOS 27 redesigns that the scraped SF Symbols
            // set does not contain, so approximating them kept the icon column
            // three times worse than the label column.
            ui.masks->Draw(ui.dl, (Glyph)((int)Glyph::General + i), tile, ui.dx);
        } else if (rows[i].symbol == SfIcon::Waveform) {
            // Siri's tile is a rendered orb, not a glyph: measured corner
            // samples run from dark green (14,79,64) at the top to blue
            // (12,65,135) at the bottom.
            float cx = tile.x + 14.0f, cy = tile.y + 14.0f;
            const int rings = 7;
            for (int k = 0; k < rings; ++k) {
                float t = (float)k / (float)(rings - 1);
                float rr = 14.5f - t * 10.0f;
                int r = (int)(110 + t * 145);
                int g = (int)(22 + t * 110);
                int b = (int)(58 + t * 40);
                ui.dl->AddCircleFilled(ImVec2(cx - 3.0f + t * 4.0f, cy + 1.0f - t * 2.0f),
                                       rr, IM_COL32(r, g, b, 255), 28);
            }
            IosGlyph(ui, SfIcon::Waveform, Rect(tile.x + 4, tile.y + 5, 20, 18),
                     Rgba(1, 1, 1, 0.92f));
        } else {
            // Measured glyph ink inside the row tiles is ~22 x 22 (camera 23 x
            // 22, gear 24 x 24), so the symbol is fitted to 21.2 pt.
            IosGlyph(ui, rows[i].symbol, Rect(tile.x + 3.4f, tile.y + 3.6f, 21.2f, 20.8f),
                     Rgba(1, 1, 1, 1), 0.1f);
        }
        // Ink phase from Tools/_phase.py against the 3x capture: the port's run
        // sat ~0.67 pt right of the runtime's at this size, so the origin moves
        // left by that much (now that sub-point origins are honoured).
        ui.TextVCenter(ui.fonts.rowLabel17, 17.0f, ios_metrics::kLabelX - 0.67f,
                       Rect(row.x, row.y + 0.33f, row.w, row.h), rows[i].label, th.text);
        IosChevron(ui, ios_metrics::kChevronX, ry + 20.33f);
        if (i < count - 1) {
            // One clean point of #E8E8E8. The capture at 3x reads three device rows of
            // exactly 232 - not a fractional split - and drawing two rows at 0.7 / 0.3
            // alpha instead produced a 239/248 band two points tall.
            // 1 pt rule at a fractional y is two rows at 1x whose coverage splits 0.7 /
            // 0.3. A single snapped row measured better in dark and worse in light
            // (settings 1.928 / dark 2.204 against 1.892 / 2.252), so the split stays.
            float top = std::floor(ry + 51.0f);
            float frac = (ry + 51.0f) - top;
            ui.dl->AddRectFilled(ImVec2(ios_metrics::kSepX, top),
                                 ImVec2(ios_metrics::kSepX + ios_metrics::kSepW, top + 1),
                                 th.separator.WithAlpha(1.0f - frac).U32());
            ui.dl->AddRectFilled(ImVec2(ios_metrics::kSepX, top + 1),
                                 ImVec2(ios_metrics::kSepX + ios_metrics::kSepW, top + 2),
                                 th.separator.WithAlpha(frac).U32());
        }
    }

    // ---- floating search glass ------------------------------------------
    Rect search(ios_metrics::kSearchX, ios_metrics::kSearchY,
                ios_metrics::kSearchW, ios_metrics::kSearchH);
    // Measured: the field reads 253/253/253 over a 242/242/247 page, i.e. a lift
    // of (253-242)/(255-242) = 0.85 per channel — translucent, not opaque. The
    // previous fully-opaque white tint hid the row the bar floats over, which is
    // visible in the capture as faint ghosting behind the placeholder.
    {
        const Rgba searchLift(0.82f, 0.82f, 0.83f);
        ui.Glass(search, 24.0f,
                 th.dark ? Rgba(0.24f, 0.24f, 0.245f, 1.00f) : Rgba(1, 1, 1, 1),
                 1.0f, false, nullptr,
                 th.dark ? nullptr : &searchLift);
    }
    {
    // Measured ink boxes: magnifier (42,813.33) 16.33 x 16.67, dictate
    // (344,812.67) 12.67 x 18.33. The mic is the *outline* symbol in the
    // runtime; the atlas only holds mic.fill, so it is drawn from measured
    // geometry in IosChrome.h.
    Rgba glyphInk = th.dark ? Rgba(0.72f, 0.72f, 0.74f, 1.0f) : Rgba(0.11f, 0.11f, 0.12f, 1.0f);
    if (ui.masks && ui.masks->Ready()) {
        // Dark mode inverts the field: the glyphs are white on a dark pill (the light
        // capture bakes them dark, which made them invisible here).
        static const int kWhiteGlyph[3] = { 210, 210, 214 };
        const int* glyphOverride = ui.theme.dark ? kWhiteGlyph : nullptr;
        ui.masks->Draw(ui.dl, Glyph::Magnifier, Rect(42.0f, 813.33f, 16.33f, 16.67f), ui.dx,
                       glyphOverride);
        ui.masks->Draw(ui.dl, Glyph::Mic, Rect(344.0f, 812.67f, 12.67f, 18.33f), ui.dx,
                       glyphOverride);
    } else {
        IosGlyph(ui, SfIcon::Magnifier, Rect(42.0f, 813.33f, 16.33f, 16.67f), glyphInk, 0.18f);
        IosSearchMic(ui, glyphInk);
    }
    // The placeholder measures a neutral 139/255 on screen (no blue cast), so it
    // is drawn as neutral grey rather than the #8E8E93 token.
    IosTextDelta(ui, ui.fonts.regular17, 17.0f, 69.5f, 812.0f, "Search",
                 th.dark ? Rgba(1, 1, 1, 0.52f) : Rgba(0.522f, 0.522f, 0.525f, 1.0f),
                 0.33f);
    }
    (void)st;
}

} // namespace sw
