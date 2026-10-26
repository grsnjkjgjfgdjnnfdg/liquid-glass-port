#pragma once
#include <algorithm>
#include "Spring.h"
#include "Ui.h"
#include "IosSettings.h"
#include "NavTransition.h"
#include "SheetTransition.h"

namespace sw {
namespace flow {

// ---------------------------------------------------------------------------
// The three surfaces wired together, the way they are on the device: the ported
// Settings page is the root, tapping a row pushes a page with the measured push
// spring, and tapping the account row presents the sheet with the measured sheet
// springs. Neither motion is re-implemented here - `nav::Screen` and
// `sheet::Screen` own them; this only decides what is on top and where input goes.
//
// Why it matters beyond the demo: a push and a presentation have to *coexist*. The
// pushed page keeps its position while the sheet is up (the runtime does not unwind
// the stack), input stops reaching it, and dismissing the sheet returns to the same
// page rather than the root. That interaction is the part "everything feels
// connected" actually means, and it is invisible in a still.
// ---------------------------------------------------------------------------
struct State {
    int stage = 0;          // automation state, for the screenshot runs
    float t = 0.0f;
};

inline void Screen(UI& ui, nav::State& navState, sheet::State& sheetState,
                   IosSettingsState& settings, float dt) {
    const bool sheetUp = sheetState.presented || sheetState.moving;

    // The account row presents the sheet; it is checked before the navigation view
    // sees the mouse so a tap on it cannot also push.
    // Only when the root page is the one on screen: this rect is the root's account row,
    // and without the depth test a tap on the *pushed* page's rows at the same y presented
    // the sheet from a page the user was not looking at.
    if (navState.progress() < 0.5f && !sheetUp && !navState.moving && ui.mousePressed &&
        ui.mouse.x >= 16.0f && ui.mouse.x <= 386.0f &&
        ui.mouse.y >= 168.0f && ui.mouse.y <= 258.3f) {
        sheetState.Present();
    }

    // The presenter is the navigation flow. While the sheet is up it is frozen and
    // receives no input - the same as a real modal presentation.
    const bool modal = sheetState.presented || sheetState.moving;
    const bool savedPressed = ui.mousePressed;
    const bool savedDown = ui.mouseDown;
    const bool savedReleased = ui.mouseReleased;
    if (modal) {
        ui.mousePressed = false;
        ui.mouseDown = false;
        ui.mouseReleased = false;
    }
    sheet::Screen(ui, sheetState, modal ? dt : dt,
                  [&](UI& u) { nav::Screen(u, navState, settings, modal ? 0.0f : dt); });
    ui.mousePressed = savedPressed;
    ui.mouseDown = savedDown;
    ui.mouseReleased = savedReleased;

    // Done, and a tap anywhere on the scrim, dismiss.
    if (modal && !sheetState.moving && ui.mousePressed) {
        const float top = sheet::Metrics::kTop;
        const bool onDone = ui.mouse.x >= 24.0f && ui.mouse.x <= 120.0f &&
                            ui.mouse.y >= top + 92.0f && ui.mouse.y <= top + 124.0f;
        const bool onScrim = ui.mouse.y < top - 4.0f;
        if (onDone || onScrim) sheetState.Dismiss();
    }
}

// Automation for the capture runs: push, pop, present, dismiss on a fixed timeline
// so each state can be screenshotted and each transition sampled.
inline void Automate(float elapsed, nav::State& navState, sheet::State& sheetState,
                     State& st, bool sheetOnly) {
    if (sheetOnly) {
        if (st.stage == 0 && elapsed >= 1.0f) { sheetState.Present(); st.stage = 1; }
        else if (st.stage == 1 && elapsed >= 2.6f) { sheetState.Dismiss(); st.stage = 2; }
        return;
    }
    switch (st.stage) {
        case 0: if (elapsed >= 1.0f) { navState.Push(); st.stage = 1; } break;
        case 1: if (elapsed >= 2.2f) { navState.Pop(); st.stage = 2; } break;
        case 2: if (elapsed >= 2.8f) { sheetState.Present(); st.stage = 3; } break;
        case 3: if (elapsed >= 4.4f) { sheetState.Dismiss(); st.stage = 4; } break;
        default: break;
    }
}

} // namespace flow
} // namespace sw
