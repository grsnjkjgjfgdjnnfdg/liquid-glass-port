#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace sw {

struct Rgba {
    float r = 0, g = 0, b = 0, a = 1;
    Rgba() = default;
    Rgba(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    Rgba Lerp(const Rgba& o, float t) const {
        return Rgba(r + (o.r - r) * t, g + (o.g - g) * t,
                    b + (o.b - b) * t, a + (o.a - a) * t);
    }
    Rgba WithAlpha(float v) const { return Rgba(r, g, b, v); }
    uint32_t U32() const {
        auto c = [](float v) { return (uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
        return (c(a) << 24) | (c(b) << 16) | (c(g) << 8) | c(r);
    }
    // Convenience for the measured sRGB hex values in the reference notes.
    static Rgba Hex(uint32_t rgb, float a = 1.0f) {
        return Rgba(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f,
                    (rgb & 0xFF) / 255.0f, a);
    }
};

// Every value below is measured from the real iOS 27 / macOS 27 runtimes,
// not from a design mock. Source of truth: PORT_SPEC.md.
struct Theme {
    bool dark = false;

    // Labels
    Rgba text;
    Rgba secondary;
    Rgba tertiary;
    Rgba separator;

    // Surfaces
    Rgba pageBg;        // iOS systemGroupedBackground
    Rgba cardBg;        // iOS secondarySystemGroupedBackground
    Rgba windowBg;      // macOS window background

    // Fills
    Rgba fill;          // systemFill (120,120,128,0.20)
    Rgba fill2;
    Rgba segTrack;      // tertiarySystemFill (118,118,128,0.12 / 0.24)
    Rgba segPill;

    // Controls
    Rgba switchOn;      // measured #0088FF / #0091FF
    Rgba switchOff;     // measured #C5C5C7 / #5A5A5E
    Rgba knob;          // switch knob / slider thumb
    Rgba trackFill;     // slider filled part
    Rgba trackBg;       // slider unfilled part (systemFill over card)
    Rgba fieldBorder;
    Rgba fieldBg;
    Rgba borderedFill;
    Rgba chevron;

    Rgba glassTint;
    Rgba cardTint;
    Rgba sidebarTint;
    Rgba accent;
    Rgba accentText;
    Rgba shadow;

    static Theme Light() {
        Theme t;
        t.dark = false;
        t.text        = Rgba(0, 0, 0, 1.0f);
        t.secondary   = Rgba(0.235f, 0.235f, 0.263f, 0.60f);   // #3C3C43 60%
        t.tertiary    = Rgba(0.235f, 0.235f, 0.263f, 0.30f);
        t.separator   = Rgba::Hex(0xE8E8E8);   // measured 1 pt rule under a Settings row
        t.pageBg      = Rgba::Hex(0xF2F2F7);
        t.cardBg      = Rgba::Hex(0xFFFFFF);
        t.windowBg    = Rgba::Hex(0xFFFFFF);
        t.fill        = Rgba(120 / 255.0f, 120 / 255.0f, 128 / 255.0f, 0.20f);
        t.fill2       = Rgba(120 / 255.0f, 120 / 255.0f, 128 / 255.0f, 0.12f);
        t.segTrack    = Rgba(118 / 255.0f, 118 / 255.0f, 128 / 255.0f, 0.12f);
        t.segPill     = Rgba(1, 1, 1, 1);
        t.switchOn    = Rgba::Hex(0x0088FF);
        t.switchOff   = Rgba::Hex(0xC5C5C7);
        t.knob        = Rgba(1, 1, 1, 1);
        t.trackFill   = Rgba::Hex(0x0088FF);
        t.trackBg     = Rgba::Hex(0xE4E4E4);
        t.fieldBorder = Rgba::Hex(0xCCCCCC);
        t.fieldBg     = Rgba(1, 1, 1, 1);
        t.borderedFill= Rgba::Hex(0xD1EAFF);
        t.chevron     = Rgba(0.235f, 0.235f, 0.263f, 0.30f);
        t.glassTint   = Rgba(1, 1, 1, 0.58f);
        t.cardTint    = Rgba(1, 1, 1, 0.34f);
        t.sidebarTint = Rgba(1, 1, 1, 0.18f);
        t.accent      = Rgba::Hex(0x0088FF);
        t.accentText  = Rgba(1, 1, 1, 1);
        t.shadow      = Rgba(0, 0, 0, 0.30f);
        return t;
    }

    static Theme Dark() {
        Theme t;
        t.dark = true;
        t.text        = Rgba(1, 1, 1, 0.98f);
        // Dark secondary: #EBEBF5 at 60 % measures 143/255 on the capture.
        t.secondary   = Rgba(0.922f, 0.922f, 0.961f, 0.60f);   // #EBEBF5 60%
        t.tertiary    = Rgba(0.922f, 0.922f, 0.961f, 0.30f);
        t.separator   = Rgba(84 / 255.0f, 84 / 255.0f, 88 / 255.0f, 0.60f);
        t.pageBg      = Rgba(0, 0, 0, 1);
        t.cardBg      = Rgba::Hex(0x1C1C1E);
        t.windowBg    = Rgba::Hex(0x1E1E1E);
        t.fill        = Rgba(120 / 255.0f, 120 / 255.0f, 128 / 255.0f, 0.36f);
        t.fill2       = Rgba(120 / 255.0f, 120 / 255.0f, 128 / 255.0f, 0.24f);
        t.segTrack    = Rgba(118 / 255.0f, 118 / 255.0f, 128 / 255.0f, 0.24f);
        t.segPill     = Rgba::Hex(0x69696F);
        t.switchOn    = Rgba::Hex(0x0091FF);
        t.switchOff   = Rgba::Hex(0x5A5A5E);
        t.knob        = Rgba(1, 1, 1, 1);
        t.trackFill   = Rgba::Hex(0x0091FF);
        t.trackBg     = Rgba::Hex(0x313133);
        t.fieldBorder = Rgba::Hex(0x333333);
        t.fieldBg     = Rgba(0, 0, 0, 1);
        t.borderedFill= Rgba::Hex(0x153956);
        t.chevron     = Rgba(0.922f, 0.922f, 0.961f, 0.30f);
        t.glassTint   = Rgba(0.06f, 0.06f, 0.08f, 0.52f);
        t.cardTint    = Rgba(0.07f, 0.07f, 0.09f, 0.36f);
        t.sidebarTint = Rgba(0.03f, 0.03f, 0.04f, 0.20f);
        t.accent      = Rgba::Hex(0x0091FF);
        t.accentText  = Rgba(1, 1, 1, 1);
        t.shadow      = Rgba(0, 0, 0, 0.55f);
        return t;
    }
};

// ---------------------------------------------------------------------------
// Measured control metrics. iOS 27 values come from 3x simulator pixels; the
// macOS 27 values come from the Apple UI kit layer tree.
// ---------------------------------------------------------------------------
namespace ios {
constexpr float kSwitchW       = 63.0f;   // capsule, measured 301..364
constexpr float kSwitchH       = 28.0f;
constexpr float kSwitchRadius  = 14.0f;
constexpr float kSwitchKnobW   = 37.0f;   // the pill knob, NOT a circle (layer tree [2,2,37,24])
constexpr float kSwitchKnobH   = 24.0f;
constexpr float kSwitchInset   = 2.0f;    // top/bottom/trailing

constexpr float kSliderThumbW  = 36.3f;   // same pill family as the switch knob
constexpr float kSliderThumbH  = 24.0f;
constexpr float kTrackH        = 6.0f;
constexpr float kSliderRowH    = 31.0f;   // accessibility height of UISlider
// While a control is being touched it stops being an opaque shape and becomes a
// frosted lens: measured from the 60 fps recording of a real drag and a real flip,
// the slider thumb reads the track through it at ~50 % white, and the switch knob at
// ~24 % white at the peak of its morph. Both get a bright rim on every edge, which
// the large panels do not have (they are lit on the horizontal edges only).
constexpr float kThumbGlassTint     = 0.16f;
constexpr float kThumbGlassRim      = 1.0f;
constexpr float kThumbGlassRefract  = 0.45f;
constexpr float kThumbGlassVibrancy = 1.0f;
constexpr float kThumbGlassFrost    = 0.6f;   // measured: the track edge softens ~1 px
// Solved from the measured peak pixel (172,192,213) over a (0,134,254) track:
// 0.55 of white and 0.38 of the base chroma, i.e. the knob both lifts and desaturates.
constexpr float kKnobGlassTint      = 0.55f;
constexpr float kKnobGlassRim       = 0.9f;
constexpr float kKnobGlassRefract   = 0.35f;
constexpr float kKnobGlassVibrancy  = 0.38f;
constexpr float kKnobGlassFrost     = 1.2f;
// Peak uniform swell of the switch knob during a flip: measured 37 x 24 -> 47.03 x
// 30.84, i.e. +27 %. It is driven by speed, so this is the cap and `...SwellK`
// converts pt/s of knob speed into it (peak speed is ~250 pt/s on a 22 pt travel).
constexpr float kSwitchSwell       = 0.30f;
constexpr float kSwitchSwellK      = 0.00118f;

constexpr float kSegH          = 32.0f;   // measured 511..543
constexpr float kSegPillInset  = 2.0f;
constexpr float kSegRadius     = 16.0f;

constexpr float kFieldH        = 34.0f;
constexpr float kFieldRadius   = 5.7f;

constexpr float kButtonH       = 34.3f;
constexpr float kRowH          = 52.0f;
constexpr float kGroupRadius   = 26.0f;
constexpr float kCardInset     = 16.0f;   // row x inset inside a card
} // namespace ios

enum class ControlSize { Mini, Small, Regular, Large, ExtraLarge };

inline float ControlHeight(ControlSize s) {
    switch (s) {
        case ControlSize::Mini:       return 16.0f;
        case ControlSize::Small:      return 20.0f;
        case ControlSize::Regular:    return 24.0f;
        case ControlSize::Large:      return 28.0f;
        case ControlSize::ExtraLarge: return 36.0f;
    }
    return 24.0f;
}

constexpr float kSliderWidth = 140.0f;
constexpr float kSliderInset = 10.0f;
constexpr float kTrackHeightMin = 4.0f;
constexpr float kTrackHeightRegular = 6.0f;
constexpr float kSwitchRegularW = ios::kSwitchW;
constexpr float kSwitchRegularH = ios::kSwitchH;
constexpr float kSwitchInset = ios::kSwitchInset;
constexpr float kSegmentedRegularH = ios::kSegH;
constexpr float kSegmentedTrioW = 90.0f;
constexpr float kButtonRegularH = ios::kButtonH;

} // namespace sw
