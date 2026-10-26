#pragma once
#include "Ui.h"

namespace sw {

// Reproduction of the SwiftUI 27 control gallery that was captured from the
// real runtime (ReferenceLab, iOS 27.0, iPhone 18 Pro). The point of this page
// is that it can be pixel-diffed against the simulator screenshot: same frames,
// same font sizes, same surfaces.
//
// Measured reference (frames-*.json + 3x pixels):
//   page background      #F2F2F7
//   large title ink      93 .. 119      (34 pt bold, x 25)
//   subtitle ink         135.7 / 155.7  (15 pt, line height 20)
//   card x/w             24 / 354, radius 16, spacing 26
//   card tops            196.7, 315, 462.7, 584, 708.3
//   card title ink       card_top + 19.3 (17 pt semibold, secondary, x 40)
//   row                 card_top + 48.33
//   switch row          (40, 245, 322, 28)   capsule 301..364
//   slider row          (40, 363.33, 322, 31)
//   segmented           (40, 511, 322, 31)
//   text field          (40, 632.33, 322, 34)
//   buttons             (40, 756.67, h 34.33): 92.67 / 76.67 / 86.33 wide
constexpr float kGalCardX = 24.0f;
constexpr float kGalCardW = 354.0f;
constexpr float kGalCardRadius = 16.0f;

struct IosGalleryState {
    SwitchState toggle;
    SliderState slider;
    SegState segment;
    TextFieldState field;
    ButtonState btnA, btnB, btnC;
    bool on = true;
    float value = 0.62f;
    int segmentIndex = 1;
    bool segmentSnapped = false;
};

inline void IosControls(UI& ui, IosGalleryState& st) {
    const Theme& th = ui.theme;
    ui.dl->AddRectFilled(ImVec2(0, 0), ImVec2(kIosW, kIosH), th.pageBg.U32());

    // Status bar is part of the reference screenshot, so it is part of the page.
    Rgba ink(0, 0, 0, 0.95f);
    if (th.dark) ink = Rgba(1, 1, 1, 0.95f);
    ui.Text(ui.fonts.semibold17, 17.0f, 63.0f, 22.5f, "9:41", ink);
    for (int i = 0; i < 4; ++i) {
        float bh = 3.5f + i * 1.9f;
        ui.dl->AddRectFilled(ImVec2(277.0f + i * 5.6f, 33.0f - bh),
                             ImVec2(279.4f + i * 5.6f, 33.0f), ink.U32(), 1.0f);
    }
    ui.dl->PathArcTo(ImVec2(315.4f, 33.0f), 9.6f, -2.42f, -0.72f, 18);
    ui.dl->PathStroke(ink.U32(), 0, 2.0f);
    ui.dl->PathArcTo(ImVec2(315.4f, 33.0f), 5.4f, -2.30f, -0.84f, 14);
    ui.dl->PathStroke(ink.U32(), 0, 2.0f);
    ui.dl->AddCircleFilled(ImVec2(315.4f, 32.0f), 1.8f, ink.U32(), 12);
    ui.dl->AddRect(ImVec2(333.0f, 24.5f), ImVec2(356.6f, 37.5f),
                   (ink.r > 0.5f) ? IM_COL32(255, 255, 255, 110) : IM_COL32(0, 0, 0, 110),
                   4.2f, 0, 1.2f);
    ui.FillRounded(Rect(335.2f, 26.7f, 20.4f, 8.6f), 2.4f, Rgba::Hex(0x34C759));
    ui.dl->AddRectFilled(ImVec2(357.6f, 28.6f), ImVec2(359.6f, 33.4f),
                         (ink.r > 0.5f) ? IM_COL32(255, 255, 255, 110) : IM_COL32(0, 0, 0, 110),
                         1.0f);
    ui.Line(340.0f, 27.6f, 348.0f, 34.4f, Rgba(1, 1, 1, 0.92f), 1.6f);

    // Title block. Measured against the 3x capture: the port's title ink sat 1 pt
    // right and 0.67 pt high, and both subtitle lines were ~2.8 % narrower than the
    // runtime's (290.0 vs 281.67 over the first line, 75.67 vs 73.67 over the
    // second â€” the same 0.2 pt per glyph), so the block uses tracked text.
    ui.TextBold(ui.fonts.semibold34, 34.0f, kGalCardX, 93.0f - 8.0f + 0.99f, "SwiftUI 27", th.text);
    IosTextDelta(ui, ui.fonts.regular15, 15.0f, kGalCardX + 0.67f, 135.7f - 3.2f - 0.33f,
                 "Native controls, captured from the real iOS", th.secondary, 0.20f);
    IosTextDelta(ui, ui.fonts.regular15, 15.0f, kGalCardX + 0.67f, 155.7f - 3.2f - 0.33f,
                 "27 runtime.", th.secondary, 0.20f);

    auto card = [&](float top, float h) {
        Rect r(kGalCardX, top, kGalCardW, h);
        ui.FillRounded(r, kGalCardRadius, th.cardBg);
        return r;
    };
    auto cardTitle = [&](float top, const char* label) {
        ui.Text(ui.fonts.semibold17, 17.0f, 40.0f, top + 19.3f - 3.07f, label, th.secondary);
    };

    // 1. Switch
    card(196.7f, 92.3f);
    cardTitle(196.7f, "Switch");
    ui.TextVCenter(ui.fonts.rowLabel17, 17.0f, 40.0f, Rect(40, 245, 200, 28), "Wi-Fi", th.text);
    {
        Rect sw(301.0f, 245.0f, 63.0f, 28.0f);
        DrawSwitch(ui, sw, st.on, st.toggle, ui.dt);
    }

    // 2. Slider
    card(315.0f, 121.7f);
    cardTitle(315.0f, "Slider");
    DrawSlider(ui, Rect(40, 363.33f, 322, 31), st.value, st.slider, ui.dt);
    ui.Text(ui.fonts.regular12, 12.0f, 40.0f, 403.0f, "0%", th.secondary);
    ui.TextRight(ui.fonts.regular12, 12.0f, Rect(40, 403.0f, 322, 14), "100%", th.secondary);

    // 3. Segmented control
    card(462.7f, 95.3f);
    cardTitle(462.7f, "Segmented control");
    static const char* segLabels[] = { "Light", "Dark", "Auto" };
    // A presented control is already in place, so the pill is snapped to the initial
    // selection on the first frame rather than sliding in from index 0.
    if (!st.segmentSnapped) { st.segment.SnapTo(st.segmentIndex); st.segmentSnapped = true; }
    DrawSegmented(ui, Rect(40, 511, 322, 31), st.segmentIndex, 3, st.segment, segLabels, ui.dt);

    // 4. Text field
    card(584.0f, 98.3f);
    cardTitle(584.0f, "Text field");
    DrawTextField(ui, Rect(40, 632.33f, 322, 34), st.field, "Name");

    // 5. Buttons
    card(708.3f, 98.7f);
    cardTitle(708.3f, "Buttons");
    DrawButton(ui, Rect(40, 756.67f, 92.67f, 34.33f), "Continue", ButtonKind::Prominent, st.btnA, ui.dt);
    DrawButton(ui, Rect(144.67f, 756.67f, 76.67f, 34.33f), "Cancel", ButtonKind::Bordered, st.btnB, ui.dt);
    DrawButton(ui, Rect(233.33f, 763.67f, 86.33f, 20.33f), "Learn More", ButtonKind::Plain, st.btnC, ui.dt);

    // 6. Control sizes. Found by comparing the card boundaries of the two dark captures:
    // the port matched the reference to within 1.3 pt for every group and then stopped at
    // y=807 where the reference has another card from 833. Its absence was the worst band
    // in either controls capture - 19.0 mean over 810..874 - because 41 pt of a card is
    // visible below the fold and the port had page there instead.
    //
    // The rows themselves sit below the fold, so their internals are laid out from the
    // reference source (a 70 pt caption, a slider, a hidden-label switch, row heights
    // 20/24/28/32/40) rather than measured. Only the card and its caption are visible.
    static const char* sizeIds[5] = { "mini", "small", "regular", "large", "extraLarge" };
    static const float sizeH[5] = { 20.0f, 24.0f, 28.0f, 32.0f, 40.0f };
    static bool sizeOn[5] = { true, true, false, true, false };
    static float sizeValue[5] = { 0.35f, 0.55f, 0.7f, 0.45f, 0.8f };
    static SwitchState sizeSwitch[5];
    static SliderState sizeSlider[5];
    card(833.0f, 258.0f);
    cardTitle(833.0f, "Control sizes");
    float rowTop = 833.0f + 50.0f;
    for (int i = 0; i < 5; ++i) {
        const float rowH = sizeH[i];
        const float cy = rowTop + rowH * 0.5f;
        ui.TextVCenter(ui.fonts.regular12, 12.0f, 40.0f,
                       Rect(40, cy - 10.0f, 70, 20), sizeIds[i], th.secondary);
        DrawSlider(ui, Rect(124.0f, cy - 15.5f, 163.0f, 31), sizeValue[i],
                   sizeSlider[i], ui.dt);
        DrawSwitch(ui, Rect(301.0f, cy - 14.0f, 63.0f, 28.0f), sizeOn[i],
                   sizeSwitch[i], ui.dt);
        rowTop += rowH + 12.0f;
    }
}


// ---------------------------------------------------------------------------
// Liquid-glass demo page. Apple's material cannot be judged on a white card, so
// this page puts the same panels over a saturated gradient: the blur, the rim
// and the top sheen all become measurable (and visibly wrong when they are).
// ---------------------------------------------------------------------------
inline bool g__demoSwitch = true;
inline SwitchState g__demoSwitchState;
inline float g__demoValue = 0.55f;
inline SliderState g__demoSlider;

// The glass page lives in GlassDemo.inc: measured layout and measured fills
// in one place. It used to be duplicated here as well, which meant edits to
// the .inc silently changed nothing.
#include "GlassDemo.inc"

// ---------------------------------------------------------------------------
// Refraction verification page:
//
// The same 20 pt grid that ReferenceLab draws (App/RefractionChart.swift) with the
// same clear-glass panel at the same place, so the port's lens can be diffed
// against the real capture pixel for pixel.
// ---------------------------------------------------------------------------
struct IosRefractionState {
    int unused = 0;
};

inline void IosRefractionChart(UI& ui, IosRefractionState& st) {
    (void)st;
    ui.dl->AddRectFilled(ImVec2(0, 0), ImVec2(kIosW, kIosH), IM_COL32(255, 255, 255, 255));
    for (float x = 0.0f; x <= kIosW; x += 20.0f) {
        bool heavy = std::fmod(x, 100.0f) < 0.01f;
        float w = heavy ? 3.0f : 1.5f;
        ui.dl->AddRectFilled(ImVec2(x - w * 0.5f, 0), ImVec2(x + w * 0.5f, kIosH),
                             heavy ? IM_COL32(0, 0, 0, 255) : IM_COL32(90, 90, 90, 255));
    }
    for (float y = 0.0f; y <= kIosH; y += 20.0f) {
        bool heavy = std::fmod(y, 100.0f) < 0.01f;
        float h = heavy ? 3.0f : 1.5f;
        ui.dl->AddRectFilled(ImVec2(0, y - h * 0.5f), ImVec2(kIosW, y + h * 0.5f),
                             heavy ? IM_COL32(0, 0, 0, 255) : IM_COL32(90, 90, 90, 255));
    }

    // Regular material: essentially opaque over a bright backdrop.
    // Geometry measured from the capture (iOS27Chart/12-refraction-light) by removing
    // the grid: a row has ~9 % dark pixels where the grid is visible and ~0 % inside a
    // panel, so the panel's extent is the run of rows where the grid disappears. That
    // gives x 31..371 (340 wide) and y 200..420 for the regular panel, 444..664 for the
    // clear one - the SwiftUI source's 340 x 220 at the same place. The port used to
    // draw 348 x 204 at y 208/457, i.e. the wrong size *and* 8 pt low, which put every
    // grid line inside the panel in the wrong place and made the chart score useless as
    // a measure of the material (chart 9.206 was mostly geometry error).
    // The .regular material inverts in dark appearance: measured 128/255 over the
    // white chart page, i.e. black at 50 % rather than white at 50 %.
    // Dark: the measured panel is a flat 128/255 with no grid structure through it, so
    // it reads as an opaque half-grey rather than a translucent black (a 50 % black
    // over the blurred grid left the grid visible at 91..118).
    const Rgba regularTint = ui.theme.dark ? Rgba(0.502f, 0.502f, 0.502f, 1.0f)
                                           : Rgba(1, 1, 1, 0.50f);
    // The edge strength is a per-panel material property, and this page's panels sit on
    // a white page over a sharp grid rather than on the glass page's gradient. Measured
    // there, the strong hairline cost the chart 0.06/0.35 (light/dark) while it earned
    // the glass page 0.06 and cut its edge-band error by 7 %, so this page keeps a third
    // of it until the two regimes are reconciled.
    ui.Glass(Rect(31, 200, 340, 220), 40.0f, regularTint, 1.0f, true, nullptr, nullptr,
             1.0f, false, 1.0f, 1.0f, 0.0f, 1.90f, 0.0f, 0.35f);
    // Clear material: the grid must stay visible and bend near the rim.
    // .clear: the backdrop stays sharp through it. The scene capture is 3x now, so this
    // samples real grid pixels instead of an upscaled 1x blur.
    // `.clear` is not sharp: inside the capture the grid lines are 2-3x wider and
    // lower contrast than outside, which is a small frost rather than none. It binds the
    // sharp scene and frosts it in the shader, like the slider thumb.
    ui.Glass(Rect(31, 444, 340, 220), 40.0f, Rgba(1, 1, 1, 0.06f), 1.0f, true, nullptr,
             nullptr, 1.0f, true, 1.0f, 1.0f, 0.0f, 1.90f, 1.2f, 0.35f);

    // The grid runs behind the status bar in the capture (the bar has no background of
    // its own there), and in dark appearance the bar's ink is white on a page that
    // stays white - only the battery is visible, which is what the capture shows.
    IosStatusBar(ui);

}
} // namespace sw
