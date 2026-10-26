#pragma once
#include <algorithm>
#include <cmath>
#include "Spring.h"
#include "Ui.h"
#include "Symbols.h"
#include "IosSettings.h"

namespace sw {
namespace nav {

// ---------------------------------------------------------------------------
// Navigation push, measured from the real iOS 27 runtime.
//
// Source: an instrumented SwiftUI `NavigationStack` in the reference app, whose
// layers were sampled from CADisplayLink every frame during a real push and pop
// (`Reference/iOS27Probe9/nav_transition.json`, raw data in
// `Reference/iOS27Probe9/.../layer_samples-nav-light.jsonl`). 26 presentation
// frames per moving layer, fitted with the port's own spring model:
//
//   incoming page   402 -> 0     travel -401.8 pt, spring(response 0.166 s,
//                                dampingFraction 1.44), settles to 0.5 % at 0.32 s
//   outgoing page   0 -> -121    travel -120.4 pt on the same spring: the page
//                                behind parallaxes by 121.0 / 402 = 0.301 of the
//                                width instead of following the new page
//   back button     opacity 0 -> 0.98, scale 0.833 -> 1.0 (layer 44 -> 52.8 pt),
//                   starting ~50 ms after the push begins
//   pop             same geometry mirrored, spring(response 0.178, damping 1.36)
//
// There is no overshoot anywhere in the samples (progress is monotonic: 0.973 at
// 0.234 s), which is why the fit lands on an overdamped spring rather than
// SwiftUI's usual `.smooth`. Both directions use one spring; only the page
// offsets differ, so they stay perfectly locked to each other.
// ---------------------------------------------------------------------------
struct Metrics {
    static constexpr float kWidth      = 402.0f;
    static constexpr float kResponse   = 0.166f;
    static constexpr float kDamping    = 1.44f;
    static constexpr float kParallax   = 0.301f;   // 121.0 / 402
    static constexpr float kBackDelay  = 0.05f;    // s after the push starts
    static constexpr float kBackScale  = 0.833f;   // 44 / 52.8
    static constexpr float kBackFade   = 0.98f;
    static constexpr float kSettle     = 0.33f;
};

struct State {
    // The interactive left-edge swipe: the runtime lets a pop be scrubbed with the finger
    // and cancelled by releasing near the start. Same pattern as the sheet's drag.
    bool dragging = false;
    bool dragMoved = false;
    float dragStartX = 0.0f;
    float dragStartP = 0.0f;
    // The back capsule dims while held and pops on release-inside.
    bool backHeld = false;
    Spring backPress{0.0f, 0.12f, 0.90f};
    // The pushed page scrolls: the detail list is taller than its clip, and the
    // runtime lets it rubber-band past both ends. Tracked 1:1 with the finger and
    // handed to a spring on release, like the sheet.
    Spring scroll{0.0f, 0.32f, 0.86f};
    float maxScroll = 96.0f;
    bool scrollDrag = false;
    float scrollDragY = 0.0f;
    float scrollDragStart = 0.0f;

    Spring pos;        // 0 = root, 1 = detail fully pushed in
    Spring back;       // back button fade/scale, armed after kBackDelay
    int depth = 0;     // 0 root, 1 detail
    float elapsed = 0.0f;
    bool moving = false;
    bool logNext = false;

    State()
        : pos(0.0f, Metrics::kResponse, Metrics::kDamping),
          back(0.0f, 0.17f, 1.0f) {}

    void Push() {
        if (depth != 0) return;
        depth = 1;
        pos.SetTarget(1.0f);
        elapsed = 0.0f;
        moving = true;
        logNext = true;
    }
    void Pop() {
        if (depth != 1) return;
        depth = 0;
        pos.SetTarget(0.0f);
        back.SetTarget(0.0f);
        elapsed = 0.0f;
        moving = true;
        logNext = true;
    }

    void Update(float dt) {
        if (moving) {
            elapsed += dt;
            // The back button is not part of the page transform: it fades in with
            // its own settle, a beat after the page starts moving.
            if (elapsed >= Metrics::kBackDelay) back.SetTarget(depth == 1 ? Metrics::kBackFade : 0.0f);
            if (std::fabs(pos.value - pos.target) < 0.002f && std::fabs(pos.velocity) < 0.01f) {
                pos.Snap(pos.target);
                moving = false;
            }
        }
        pos.Update(dt);
        back.Update(dt);
        if (!moving && depth == 0 && back.value < 0.01f) back.Snap(0.0f);
    }

    float progress() const { return std::clamp(pos.value, 0.0f, 1.0f); }
};

// ---------------------------------------------------------------------------
// Destination page. The *motion* above is measured; the page below is standard
// iOS Settings structure (a pushed General page with plain rows), drawn with the
// measured row metrics from PORT_SPEC: 52 pt rows, 26 pt group radius, 16 pt card
// inset, 32 pt label x, one hairline separator per row.
// ---------------------------------------------------------------------------
namespace dm {
constexpr float kCardX = 16.0f, kCardW = 370.0f, kRadius = 26.0f;
constexpr float kRowH = 52.0f;
constexpr float kLabelX = 32.0f;
constexpr float kSepX = 32.0f, kSepW = 354.0f;   // to the card's right edge
constexpr float kGroupGap = 35.0f;
constexpr float kContentTop = 160.0f;
constexpr float kLargeTitleY = ios_metrics::kTitleY + 1.0f;
constexpr float kNavBarMid = 76.0f;
} // namespace dm

inline void DetailBackChevron(UI& ui, float x, float cy, const Rgba& c, float scale) {
    // chevron.left: the IosChevron glyph mirrored, 7 x 12 pt at 1.0 scale.
    float h = 12.0f * scale;
    float w = h * 7.0f / 12.0f;
    ui.Line(x + w, cy - h * 0.5f, x, cy, c, 1.9f);
    ui.Line(x, cy, x + w, cy + h * 0.5f, c, 1.9f);
}

inline void DetailRow(UI& ui, float y, const char* label, const char* value, bool chevron) {
    Rect row(dm::kCardX, y, dm::kCardW, dm::kRowH);
    ui.TextVCenter(ui.fonts.regular17, 17.0f, dm::kLabelX, row, label, ui.theme.text);
    if (value && *value) {
        ui.TextRight(ui.fonts.regular17, 17.0f,
                     Rect(dm::kCardX, y, dm::kCardW - (chevron ? 34.0f : 16.0f), dm::kRowH),
                     value, ui.theme.secondary);
    }
    if (chevron) {
        IosChevron(ui, ios_metrics::kChevronX, y + 20.33f);   // measured ink top in a 52 pt row
    }
}

// One group card: `count` rows, separators between them (never under the last).
// The pushed list's row press. The root page has had one since round 51 and this page did
// not - the same tap highlighted a row on one screen and did nothing on the next, which is
// exactly the kind of inconsistency that makes a UI feel unfinished. State lives at
// namespace scope because DetailGroup has no state object of its own.
struct DetailPressState {
    int row = -1;
    Spring press{0.0f, 0.10f, 0.86f};
    float hold = 0.0f;
    int cursor = 0;          // row index for the current frame's draw order
};
inline DetailPressState& DetailPress() {
    static DetailPressState s;
    return s;
}

// The page's horizontal offset while it is being pushed or popped: the rows hit-test in
// page coordinates, so the pointer has to arrive in them.
inline float& DetailPageOffsetX() {
    static float v = 0.0f;
    return v;
}

inline void DetailGroup(UI& ui, float y, const char* const* labels, const char* const* values,
                        const bool* chevrons, int count) {
    Rect card(dm::kCardX, y, dm::kCardW, dm::kRowH * (float)count);
    ui.FillRounded(card, dm::kRadius, ui.theme.cardBg);
    DetailPressState& dp = DetailPress();
    for (int i = 0; i < count; ++i) {
        float ry = y + i * dm::kRowH;
        // Same press treatment as the root list: fades in on touch-down, latched for 0.35 s
        // so it survives the push a row can start, light in dark mode and dark in light.
        const int id = dp.cursor++;
        Rect rr(dm::kCardX, ry, dm::kCardW, dm::kRowH);
        if (rr.Contains(ui.mouse.x - DetailPageOffsetX(), ui.mouse.y) && ui.mousePressed) {
            dp.row = id;
            dp.hold = 0.35f;
        }
        const float press = std::clamp(dp.press.value, 0.0f, 1.0f);
        const float a = (dp.row == id) ? 0.10f * press : 0.0f;
        if (a > 0.002f) {
            const float r = dm::kRadius;
            const Rgba ink = ui.theme.dark ? Rgba(1, 1, 1, a * 0.85f) : Rgba(0, 0, 0, a);
            if (i == 0) {
                ui.FillRounded(Rect(rr.x, rr.y, rr.w, rr.h + r), r, ink);
            } else if (i == count - 1) {
                ui.FillRounded(Rect(rr.x, rr.y - r, rr.w, rr.h + r), r, ink);
            } else {
                ui.FillRounded(rr, 0.0f, ink);
            }
        }
        DetailRow(ui, ry, labels[i], values ? values[i] : nullptr, chevrons[i]);
        if (i + 1 < count) {
            // Measured in the Settings root: a 1 pt rect at row_y + 51, colour
            // 238 at the top pixel fading to 246 one pixel lower.
            ui.dl->AddRectFilled(ImVec2(dm::kSepX + ui.dx, ry + 51.0f),
                                 ImVec2(dm::kSepX + dm::kSepW + ui.dx, ry + 52.0f),
                                 ui.theme.separator.U32());
        }
    }
}

// The back capsule's press, published at namespace scope so the drawing
// function needs no extra parameter (and no thread_local: this codebase
// forbids TLS in injected images).
inline float& BackPressValue() {
    static float v = 0.0f;
    return v;
}

inline void DrawDetailPage(UI& ui, float backFade, float scroll = 0.0f) {
    // Page surface first: the pushed page is opaque, it does not show the root
    // through it (the parallaxed page is visible only in the sliver the new page
    // has not covered yet).
    ui.FillRounded(Rect(0, 0, Metrics::kWidth, kIosH), 0.0f, ui.theme.pageBg);
    // The list scrolls under the bar, so the bar is drawn before this mark and the
    // content is translated after it.
    static const char* const g1[] = { "About", "Software Update" };
    static const char* const g1v[] = { "", "Up to Date" };
    static const bool g1c[] = { true, true };
    static const char* const g2[] = { "AirPlay & Continuity", "Picture in Picture", "CarPlay" };
    static const char* const g2v[] = { "", "", "" };
    static const bool g2c[] = { true, true, true };
    static const char* const g3[] = { "Background App Refresh", "Date & Time", "Keyboard",
                                      "Fonts", "Language & Region", "Dictionary" };
    static const char* const g3v[] = { "", "", "", "", "", "" };
    static const bool g3c[] = { true, true, true, true, true, true };
    static const char* const g4[] = { "VPN & Device Management", "Legal & Regulatory" };
    static const char* const g4v[] = { "", "" };
    static const bool g4c[] = { true, true };
    // The list is clipped below the status bar: the runtime lets it pass under a bar
    // material, and without one the large title would paint over the clock.
    ui.PushClip(Rect(0.0f, 44.0f, Metrics::kWidth, kIosH - 44.0f));
    // One press at a time, stepped once - the round-51 lesson from the root list, where
    // updating the spring inside the row loop made every row's target fight the next.
    {
        DetailPressState& dp = DetailPress();
        dp.cursor = 0;
        const bool active = dp.row >= 0 && (ui.mouseDown || dp.hold > 0.0f);
        dp.press.SetTarget(active ? 1.0f : 0.0f);
        dp.press.Update(ui.dt);
        if (dp.hold > 0.0f && !ui.mouseDown) dp.hold = std::max(0.0f, dp.hold - ui.dt);
        if (ui.mouseReleased) dp.hold = std::min(dp.hold, 0.05f);
    }
    const int contentMark = ui.VtxMark();
    const int contentPanels = ui.PanelMark();
    // The large title scrolls away with the list (the runtime puts it inside the scroll
    // view and fades a compact bar title in as it goes); drawn at a fixed y it collided
    // with the scrolled rows.
    // The large title collapses: it slides up with the list and fades out as the compact
    // bar title takes over. Scrolling it off at a fixed size left it overlapping the
    // first row at a partial scroll.
    const float tShift = std::clamp(scroll, 0.0f, 62.0f);
    const float tAlpha = 1.0f - std::clamp((scroll - 28.0f) / 28.0f, 0.0f, 1.0f);
    if (tAlpha > 0.01f) {
        Rgba titleInk = ui.theme.text;
        titleInk.a *= tAlpha;
        ui.TextBold(ui.fonts.semibold34, 34.0f, ios_metrics::kTitleX,
                    dm::kLargeTitleY - tShift, "General", titleInk);
    }
    float y = dm::kContentTop;
    DetailGroup(ui, y, g1, g1v, g1c, 2);
    y += 2 * dm::kRowH + dm::kGroupGap;
    DetailGroup(ui, y, g2, g2v, g2c, 3);
    y += 3 * dm::kRowH + dm::kGroupGap;
    DetailGroup(ui, y, g3, g3v, g3c, 6);
    y += 6 * dm::kRowH + dm::kGroupGap;
    DetailGroup(ui, y, g4, g4v, g4c, 2);
    ui.TranslatePage(contentMark, contentPanels, 0.0f, -scroll);
    ui.PopClip();
    // The compact bar title: 17 pt semibold, centred, fading in as the large title leaves.
    {
        const float cAlpha = std::clamp((scroll - 34.0f) / 24.0f, 0.0f, 1.0f);
        if (cAlpha > 0.01f) {
            Rgba ink = ui.theme.text;
            ink.a *= cAlpha;
            const char* compact = "General";
            const float w = ui.TextWidth(ui.fonts.semibold17, 17.0f, compact);
            ui.Text(ui.fonts.semibold17, 17.0f, (Metrics::kWidth - w) * 0.5f,
                    dm::kNavBarMid - 8.0f, compact, ink);
        }
    }

    // The bar is drawn *after* the list: in the runtime the scrolled content passes
    // underneath it, so painting the bar first let a scrolled row cover the title and
    // the back capsule.
    IosStatusBar(ui);

    // Large title + back button.

    if (backFade > 0.004f) {
        float s = Metrics::kBackScale + (1.0f - Metrics::kBackScale) * backFade;
        Rgba ink = ui.theme.accent;
        ink.a *= std::min(1.0f, backFade / Metrics::kBackFade);
        float size = 17.0f * s;

        // The back button is a liquid-glass capsule, not a bare glyph: the probe
        // found a 52.8 pt layer with a 52.49 pt CABackdropLayer behind it, which is
        // the material giving itself away. That layer came from a nav bar whose
        // back button carried no label, so the capsule here is measured in height
        // and padding and sized to its content.
        const float capH = 44.0f * s;
        const float capPad = 11.0f * s;
        const float chevW = 7.0f * s;
        const float gap = 5.0f * s;
        float labelW = ui.TextWidth(ui.fonts.regular17, 17.0f, "Settings") * s;
        float capW = capPad + chevW + gap + labelW + capPad;
        float cy = dm::kNavBarMid;
        Rect capsule(8.0f, cy - capH * 0.5f, capW, capH);
        float a = std::min(1.0f, backFade / Metrics::kBackFade);
        // The capsule dims while it is held: a bar button acknowledges the touch before
        // it acts, and the port used to pop with no feedback at all.
        const float held = std::clamp(BackPressValue(), 0.0f, 1.0f);
        ui.Glass(capsule, capH * 0.5f,
                 ui.theme.dark ? Rgba(1, 1, 1, 0.14f) : Rgba(1, 1, 1, 0.34f),
                 a * (1.0f - 0.35f * held), false);

        float chevX = capsule.x + capPad;
        DetailBackChevron(ui, chevX, cy, ink, s);
        ui.Text(ui.fonts.regular17, size, chevX + chevW + gap, cy - size * 0.62f, "Settings", ink);
    }


}

// The whole screen: root page (the ported Settings, unchanged) parallaxing left,
// the destination page travelling the full width on top of it.
inline void Screen(UI& ui, State& st, IosSettingsState& settings, float dt) {
    st.Update(dt);
    const float p = st.progress();
    const float dxRoot = -Metrics::kWidth * Metrics::kParallax * p;
    const float dxDetail = Metrics::kWidth * (1.0f - p);
    DetailPageOffsetX() = dxDetail;

    // Input, in the page's own coordinates.
    const bool onBack = ui.mouse.y >= 54.0f && ui.mouse.y <= 98.0f && ui.mouse.x < 120.0f;
    if (!st.moving) {
        if (p < 0.5f) {
            // "General" is the first row of the first group (y 380.3, h 52).
            if (ui.mousePressed && ui.mouse.x >= 16.0f && ui.mouse.x <= 386.0f &&
                ui.mouse.y >= 380.3f && ui.mouse.y <= 432.3f) {
                st.Push();
            }
        } else {
            // The back capsule acknowledges the press while it is held and pops on release
            // inside it, like a real bar button.
            if (ui.mousePressed && onBack) st.backHeld = true;
            if (ui.mouseReleased) {
                if (st.backHeld && onBack) st.Pop();
                st.backHeld = false;
            }
            if (!ui.mouseDown && !onBack) st.backHeld = false;
        }
    }
    st.backPress.SetTarget(st.backHeld ? 1.0f : 0.0f);
    st.backPress.Update(dt);
    BackPressValue() = std::clamp(st.backPress.value, 0.0f, 1.0f);

    // Scroll: a vertical drag anywhere in the content area moves the list 1:1 with a
    // rubber band past both ends; the release hands it to the spring.
    {
        const float kTop = 100.0f;
        if (!st.scrollDrag && p > 0.99f && !st.dragging && ui.mousePressed &&
            ui.mouse.y > kTop && ui.mouse.x > 24.0f) {
            st.scrollDrag = true;
            st.scrollDragY = ui.mouse.y;
            st.scrollDragStart = st.scroll.value;
        }
        if (st.scrollDrag) {
            if (ui.mouseDown) {
                const float dy = ui.mouse.y - st.scrollDragY;
                float v = st.scrollDragStart - dy;
                if (v < 0.0f) v *= 0.30f;                              // rubber band, top
                else if (v > st.maxScroll) v = st.maxScroll + (v - st.maxScroll) * 0.30f;
                st.scroll.Snap(v);
            } else {
                st.scrollDrag = false;
                st.scroll.SetTarget(std::clamp(st.scroll.value, 0.0f, st.maxScroll));
            }
        } else {
            st.scroll.SetTarget(std::clamp(st.scroll.value, 0.0f, st.maxScroll));
        }
        st.scroll.Update(dt);
    }

    // The interactive edge-swipe pop: a drag that starts within 24 pt of the left edge
    // scrubs the pop by the finger and commits past a third of the width.
    {
        const float kEdge = 24.0f;
        const float kSlop = 8.0f;    // a press alone must not scrub or commit a pop
        if (!st.dragging && !st.moving && p > 0.5f && ui.mousePressed &&
            ui.mouse.x <= kEdge) {
            st.dragging = true;
            st.dragStartX = ui.mouse.x;
            st.dragStartP = p;
            st.dragMoved = false;
        }
        if (st.dragging) {
            const float travel = std::max(1.0f, Metrics::kWidth);
            const float dxTotal = ui.mouse.x - st.dragStartX;
            if (std::fabs(dxTotal) > kSlop) st.dragMoved = true;
            if (ui.mouseDown) {
                // Only past the slop does the page start following the finger.
                if (st.dragMoved) {
                    const float np = std::clamp(st.dragStartP - dxTotal / travel, 0.0f, 1.0f);
                    st.pos.value = np;
                    st.pos.target = np;
                    st.pos.velocity = 0.0f;
                }
            } else {
                st.dragging = false;
                if (!st.dragMoved) {
                    // A tap on the edge is not a swipe: settle back where we were.
                    st.pos.value = st.dragStartP;
                    st.pos.target = st.dragStartP;
                    st.pos.velocity = 0.0f;
                } else if (st.pos.value < 0.66f) {
                    st.Pop();
                } else {
                    st.Push();
                }
            }
        }
    }

    int mark = ui.VtxMark();
    int panels = ui.PanelMark();
    ui.PushClip(Rect(0, 0, Metrics::kWidth, kIosH));
    IosSettings(ui, settings);
    ui.PopClip();
    ui.TranslatePage(mark, panels, dxRoot);

    if (p > 0.0005f) {
        // The new page carries a soft edge shadow onto the page behind it. The
        // runtime has a 120.6 pt wide layer here; the visible falloff is a small
        // fraction of that (INFERRED from f120-style captures — the probe gives
        // the layer's frame, not its gradient).
        float edge = 26.0f;
        int a = (int)(72.0f * (1.0f - p * 0.55f));
        ImU32 c0 = IM_COL32(0, 0, 0, 0);
        ImU32 c1 = IM_COL32(0, 0, 0, a);
        ui.dl->AddRectFilledMultiColor(ImVec2(dxDetail - edge, 0), ImVec2(dxDetail, kIosH),
                                       c0, c1, c1, c0);
        mark = ui.VtxMark();
        panels = ui.PanelMark();
        ui.PushClip(Rect(0, 0, Metrics::kWidth, kIosH));
        DrawDetailPage(ui, std::clamp(st.back.value, 0.0f, 1.0f), st.scroll.value);
        ui.PopClip();
        ui.TranslatePage(mark, panels, dxDetail);
    }

    // The status bar belongs to the window, not to the page: on iOS it is its own
    // window above the app's hierarchy, so it stays put while pages slide under
    // it. Both pages draw their own copy (which is right for a static screen), so
    // during a transition the strip above y=54 is repainted with the page colour
    // and one bar is drawn on top. Nothing else ever occupies that strip: it is
    // blank page background on both sides, which is why a flat repaint is exact.
    if (p > 0.0005f && p < 0.9995f) {
        ui.FillRounded(Rect(0, 0, Metrics::kWidth, 54.0f), 0.0f, ui.theme.pageBg);
        IosStatusBar(ui);
    }
}

} // namespace nav
} // namespace sw
