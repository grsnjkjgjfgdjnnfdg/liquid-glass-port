#pragma once
#include <algorithm>
#include <cmath>
#include "Spring.h"
#include "Ui.h"
#include "IosChrome.h"

namespace sw {
namespace sheet {

// ---------------------------------------------------------------------------
// Modal sheet, measured from the real iOS 27 runtime.
//
// Source: `ReferenceLab`'s SheetDemo (a medium-detent sheet) sampled per display
// frame by the layer probe (`Reference/iOS27Probe8/../layer_samples-sheet-light.jsonl`,
// 263 frames covering a present and a dismiss), fitted with the port's own spring
// model by `Tools/_nav_transition.py`:
//
//   present   travel -452.7 pt, spring(response 0.296 s, dampingFraction 1.09),
//             settle to 0.5 % at 0.401 s, fit rms 0.0057 over 29 frames
//   dismiss   travel +458.9 pt, spring(response 0.472 s, dampingFraction 0.844),
//             settle 0.430 s, fit rms 0.0213 over 32 frames
//
// So a sheet does not use one curve both ways: it arrives quickly with a slight
// overshoot and leaves slowly with a visible bounce. The presented geometry comes
// from the same samples and the screenshot:
//
//   card       inset 8 pt from each screen edge, top at y = 415, bottom at 866
//   radius     ~40 pt (the card is rounder than a Settings group card)
//   grabber    36 x 5 pt, 2.5 radius, ~6.5 pt below the card's top edge
//   scrim      black at 0.22 over the presenter
// ---------------------------------------------------------------------------
struct Metrics {
    static constexpr float kInset = 8.0f;
    static constexpr float kTop = 415.0f;
    static constexpr float kBottom = 866.0f;
    static constexpr float kRadius = 38.0f;
    // Measured from the presented-sheet captures, both appearances: the pill spans
    // x 172..229 (58 pt) with its core on rows 421-423. The port drew 36 pt at y+4, which
    // is the *system* grabber's size rather than this sheet's - the dark capture is what
    // made the difference visible, because a 36 vs 58 pt pill in a 5 pt band is a small
    // share of a full-screen mean.
    static constexpr float kGrabberW = 58.0f;
    static constexpr float kGrabberH = 5.0f;
    static constexpr float kGrabberY = 6.0f;
    static constexpr float kScrim = 0.22f;

    static constexpr float kPresentResponse = 0.296f;
    static constexpr float kPresentDamping = 1.09f;
    static constexpr float kDismissResponse = 0.472f;
    static constexpr float kDismissDamping = 0.844f;
};

struct State {
    Spring pos;          // 0 = off-screen below, 1 = at its detent
    Spring scrim;
    bool presented = false;
    bool moving = false;
    float elapsed = 0.0f;

    State() : pos(0.0f, Metrics::kPresentResponse, Metrics::kPresentDamping),
              scrim(0.0f, Metrics::kPresentResponse, Metrics::kPresentDamping) {}

    void Present() {
        if (presented) return;
        presented = true;
        // A sheet's two directions use different springs, so they are set here
        // rather than in the constructor.
        pos.response = Metrics::kPresentResponse;
        pos.dampingFraction = Metrics::kPresentDamping;
        scrim.response = Metrics::kPresentResponse;
        scrim.dampingFraction = Metrics::kPresentDamping;
        pos.SetTarget(1.0f);
        scrim.SetTarget(1.0f);
        elapsed = 0.0f;
        moving = true;
    }
    void Dismiss() {
        if (!presented) return;
        presented = false;
        pos.response = Metrics::kDismissResponse;
        pos.dampingFraction = Metrics::kDismissDamping;
        scrim.response = Metrics::kDismissResponse;
        scrim.dampingFraction = Metrics::kDismissDamping;
        pos.SetTarget(0.0f);
        scrim.SetTarget(0.0f);
        elapsed = 0.0f;
        moving = true;
    }
    // ---- interactive drag -------------------------------------------------
    // UIKit tracks the finger 1:1 and hands the layer to a spring on release; the
    // threshold is the measured one: a 100 pt drag on a 451 pt sheet returned, a
    // 420 pt drag dismissed, which brackets 50 % of the travel. The release springs
    // are the two measured ones, so a flick and a slow pull both settle correctly.
    bool dragging = false;
    float dragStartY = 0.0f;
    float dragOffset = 0.0f;
    float dragVel = 0.0f;
    float lastY = 0.0f;

    // The whole card grabs. The sheet has no scrollable content, and the runtime lets a
    // downward drag anywhere on such a sheet move it; the grabber strip alone was narrower
    // than the system. (An earlier attempt looked like it broke the 1:1 tracking, but that
    // was a bad measurement column: with the card draggable the mid-drag capture is
    // identical to the grabber-only one.)
    bool WantsDrag(float y) const {
        return presented && !moving && y >= Metrics::kTop &&
               y <= Metrics::kTop + (Metrics::kBottom - Metrics::kTop);
    }
    void BeginDrag(float y) {
        dragging = true;
        dragStartY = lastY = y;
        dragOffset = 0.0f;
        dragVel = 0.0f;
        pos.velocity = 0.0f;
    }
    void DragTo(float y, float dt) {
        if (!dragging) return;
        const float travel = 874.0f - Metrics::kTop;
        const float raw = y - dragStartY;
        // Pulling up past the detent rubber-bands instead of moving the card.
        dragOffset = raw > 0.0f ? raw : raw * 0.18f;
        if (dt > 0.0001f) dragVel = (y - lastY) / dt;
        lastY = y;
        const float p = 1.0f - dragOffset / travel;
        pos.value = p;
        pos.target = p;
        pos.velocity = 0.0f;
        scrim.value = p;
        scrim.target = p;
    }
    void EndDrag() {
        if (!dragging) return;
        dragging = false;
        const float travel = 874.0f - Metrics::kTop;
        const bool past = dragOffset > travel * 0.5f;   // (not `far`: windows.h defines it)
        const bool fast = dragVel > 800.0f;          // pt/s, a decisive flick
        if (past || fast) {
            presented = false;
            pos.response = Metrics::kDismissResponse;
            pos.dampingFraction = Metrics::kDismissDamping;
            scrim.response = Metrics::kDismissResponse;
            scrim.dampingFraction = Metrics::kDismissDamping;
            pos.SetTarget(0.0f);
            scrim.SetTarget(0.0f);
        } else {
            pos.response = Metrics::kPresentResponse;
            pos.dampingFraction = Metrics::kPresentDamping;
            scrim.response = Metrics::kPresentResponse;
            scrim.dampingFraction = Metrics::kPresentDamping;
            pos.SetTarget(1.0f);
            scrim.SetTarget(1.0f);
        }
        pos.velocity = -dragVel / travel;
        moving = true;
        elapsed = 0.0f;
        dragOffset = 0.0f;
    }

    void Update(float dt) {
        if (dragging) return;
        if (moving) {
            elapsed += dt;
            if (std::fabs(pos.value - pos.target) < 0.002f && std::fabs(pos.velocity) < 0.01f) {
                pos.Snap(pos.target);
                scrim.Snap(scrim.target);
                moving = false;
            }
        }
        pos.Update(dt);
        scrim.Update(dt);
    }
    float progress() const { return std::clamp(pos.value, 0.0f, 1.05f); }
};

// The reference app's presenter page. It is a *demo* page, not the settings list: the
// port used to draw `IosSettings` behind the card, so 46 % of the sheet capture (0-405
// pt) was comparing two unrelated screens - 13-18 mean error - while the card itself
// measured 1.88.
//
// Positions are the measured ink boxes from Reference/iOS27Sheet/18-sheet-presented,
// minus the same offsets the glass page needed (the 34 pt face's ink sits 7.9 pt below
// the y it is drawn at, and 2.3 pt right of the x).
struct SheetHostState {
    ButtonState btn;
};

inline void DrawSheetHost(UI& ui, SheetHostState& st, bool& wantPresent) {
    const Theme& th = ui.theme;
    // The reference app's presenter is `Color(.systemGroupedBackground)`, i.e.
    // #F2F2F7 in light and pure black in dark, not white. Under the 0.22 scrim that
    // reads (194,194,198); white reads (199,199,199) and left a uniform 5 units of
    // error over every empty band of the page.
    // Measured: the dimmed presenter reads exactly (194,194,198), so the page under
    // the 0.22 scrim is (248.7,248.7,253.8). `systemGroupedBackground` (#F2F2F7)
    // reads 5 units dark and white reads 5 light in R and G; the scrim is clearly
    // not a plain black overlay. Drawing the measured colour reproduces it exactly.
    ui.dl->AddRectFilled(ImVec2(0, 0), ImVec2(kIosW, kIosH),
                         th.dark ? th.pageBg.U32() : IM_COL32(249, 249, 254, 255));
    IosStatusBar(ui);
    ui.TextBold(ui.fonts.semibold34, 34.0f, 24.0f, 86.1f, "Sheet Host", th.text);
    ui.Text(ui.fonts.regular17, 17.0f, 24.0f, 144.5f,
            "Tap to present the sheet.", th.secondary);
    const Rect present(24.3f, 182.7f, 83.0f, 34.7f);
    // Commits on release inside, like every other control here.
    if (ui.mouseReleased && present.Contains(ui.mouse.x, ui.mouse.y)) wantPresent = true;
    DrawButton(ui, present, "Present", ButtonKind::Prominent, st.btn, ui.dt);
}

// The sheet's own content: the reference app's sheet, drawn with the port's text.
inline void DrawSheetContents(UI& ui) {
    const Theme& th = ui.theme;
    // Cross-correlating the card against both captured appearances puts the reference's
    // content 12 pt higher than this: the block was laid out against the card's *visual*
    // top rather than kTop. Measured, not nudged - and it was wrong in light mode too,
    // where a 12 pt shift inside a full-screen mean never showed up.
    // 23.7 pt matches the capture: the reference's "Sheet" ink is 54 pt wide and this
    // face draws 63.7 at 28.
    ui.TextBold(ui.fonts.semibold28, 23.7f, 32.0f, Metrics::kTop + 20.0f, "Sheet", th.text);
    // The subtitle's ink, measured from the two captures rather than taken from the theme:
    // the reference reads 122 in light and 111 in dark, while `theme.secondary` renders 139
    // and 136. It is this page's own grey - the list rows' secondary text measures right
    // with the theme token, so the token is not the thing that is wrong.
    const Rgba subtitleInk = th.dark ? Rgba::Hex(0x6F6F6F) : th.secondary;
    ui.Text(ui.fonts.regular17, 17.0f, 32.0f, Metrics::kTop + 62.0f,
            "Half height, grabber, dimmed presenter.", subtitleInk);
    ui.Text(ui.fonts.regular17, 17.0f, 32.0f, Metrics::kTop + 92.0f, "Done", th.accent);
}

// Draws the presenter (whatever it is), the scrim and the sheet.
template <class DrawPresenter>
inline void Screen(UI& ui, State& st, float dt, DrawPresenter drawPresenter) {
    // Input first: a press on the card's top strip (the grabber zone) grabs it, a move
    // tracks it, and the release decides between return and dismissal. A tap on the scrim
    // above the card, or on the Done title, dismisses like a real sheet. (These three
    // lines were mangled into a comment by a line-based edit; they are code again.)
    if (st.presented && !st.moving && !st.dragging && ui.mousePressed) {
        const bool onDone = ui.mouse.x >= 24.0f && ui.mouse.x <= 110.0f &&
                            ui.mouse.y >= Metrics::kTop + 92.0f &&
                            ui.mouse.y <= Metrics::kTop + 124.0f;
        const bool onScrim = ui.mouse.y < Metrics::kTop - 4.0f;
        if (onDone || onScrim) st.Dismiss();
    }
    if (st.WantsDrag(ui.mouse.y) && ui.mousePressed) st.BeginDrag(ui.mouse.y);
    if (st.dragging) {
        if (ui.mouseDown) st.DragTo(ui.mouse.y, dt);
        else st.EndDrag();
    }
    st.Update(dt);
    const float p = st.progress();

    // Presenter, then the scrim on top of it. It must not see input while the sheet is up:
    // a drag on the card was lighting up the rows behind it, which is a state a modal
    // presentation can never show.
    const bool modal = st.presented || st.moving || st.dragging;
    const bool savedPressed = ui.mousePressed;
    const bool savedDown = ui.mouseDown;
    const bool savedReleased = ui.mouseReleased;
    if (modal) {
        ui.mousePressed = false;
        ui.mouseDown = false;
        ui.mouseReleased = false;
    }
    drawPresenter(ui);
    ui.mousePressed = savedPressed;
    ui.mouseDown = savedDown;
    ui.mouseReleased = savedReleased;
    if (st.scrim.value > 0.002f) {
        ui.dl->AddRectFilled(ImVec2(0, 0), ImVec2(402.0f, 874.0f),
                             IM_COL32(0, 0, 0, (int)(255.0f * Metrics::kScrim * std::clamp(st.scrim.value, 0.0f, 1.0f))));
    }
    if (p <= 0.0005f) return;

    // The card travels: off-screen at p = 0, at its detent at p = 1.
    const float offscreenY = 874.0f;
    const float dy = (offscreenY - Metrics::kTop) * (1.0f - p);
    const float top = Metrics::kTop + dy;
    Rect card(Metrics::kInset, top, 402.0f - Metrics::kInset * 2.0f, Metrics::kBottom - top);

    // Shadow under the card: the runtime separates it from the dimmed presenter.
    ui.Shadow(Rect(card.x, card.y, card.w, card.h + 20.0f), Metrics::kRadius, 16.0f, 0.18f, 4.0f);
    // Measured: the card reads 226/226/230 over a 194/194/198 dimmed presenter, so
    // it is itself a light translucent material rather than page white. Drawing the
    // measured colour reproduces it exactly on this backdrop.
    // The measured 226/226/230 is the light appearance. A sheet in dark appearance is the
    // theme's card surface, and drawing the light grey there left the sheet's own white
    // text invisible on it.
    ui.FillRounded(card, Metrics::kRadius, ui.theme.dark ? ui.theme.cardBg : Rgba::Hex(0xE2E2E6));

    // Grabber, clipped away as the card slides in from below its own top edge.
    {
        // While the card is grabbed the grabber widens slightly and darkens: it is the only
        // affordance that says the drag belongs to this card.
        const float grabK = st.dragging ? 1.0f : 0.0f;
        const float grabW = Metrics::kGrabberW + 8.0f * grabK;
        // Measured against the dark sheet capture: the runtime draws this grabber at
        // about #71 (113) on a #1E card, not the light #A5A5AB (165) it used to draw in
        // both appearances - which measured 37.1 in the grabber region, the worst figure
        // in the port.
        const Rgba grabRest = ui.theme.dark ? Rgba::Hex(0x717176) : Rgba::Hex(0xA5A5AB);
        const Rgba grabHeld = ui.theme.dark ? Rgba::Hex(0x8C8C91) : Rgba::Hex(0x8A8A90);
        Rgba grabInk = grabRest.Lerp(grabHeld, grabK);
        Rect grab(card.x + (card.w - grabW) * 0.5f, card.y + Metrics::kGrabberY,
                  grabW, Metrics::kGrabberH);
        if (grab.y >= Metrics::kTop) {
            ui.FillRounded(grab, Metrics::kGrabberH * 0.5f, grabInk);
        }
    }

    // Contents are drawn in the card's own coordinates and translated with it, so a
    // moving sheet never re-lays out its text (that is what would make it look
    // unlike the system: SwiftUI moves the layer, it does not re-render it).
    const int mark = ui.VtxMark();
    // The clip has to be pushed in the same space the vertices are drawn in, i.e.
    // before the translation, so it is offset by -dy and lands on the card once the
    // vertices move.
    ui.PushClip(Rect(card.x, card.y - dy, card.w, card.h));
    DrawSheetContents(ui);
    ui.PopClip();
    ui.VtxTranslate(mark, 0.0f, dy);
}

} // namespace sheet
} // namespace sw
