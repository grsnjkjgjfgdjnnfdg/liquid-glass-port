#pragma once
// ---------------------------------------------------------------------------
// SwiftUi -> host bridge.
//
// This header is additive: it does not modify anything under the framework/. It lets
// an the framework host (the loader EXE, or any host that owns an
// opal::backend::D3D11Context) render pages built from the *ported* SwiftUI
// components — the ones whose geometry, colour and motion are measured from the
// real iOS 27 runtime.
//
// Usage from a host frame (this is the whole API - there is no DrawPortedPage):
//
//     static swui::Bridge bridge;
//     if (!bridge.Init(device, width, height)) { /* ported UI unavailable */ }
//     ...
//     bridge.BeginFrame(ctx, &g_glass, width, height, dt, io.MousePos,
//                       ImGui::IsMouseDown(0), ImGui::IsMouseClicked(0),
//                       ImGui::IsMouseReleased(0));
//     IosSettings(bridge.ui(), myState);        // any ported page, unchanged
//     bridge.EndFrame();                        // lens pass, stamped from the panels
//
// `BeginFrame` points `ui().blurSRV` at the framework's own blurred backdrop, so a ported
// glass panel frosts with the host's material rather than a second blur of its own.
// The bridge owns only its offscreen targets and coexists with Opal's components: a page
// can mix them because both draw into the same ImGui draw list.
//
// `--bridge-selftest` in the port constructs one against the real device and reports
// whether Init succeeded, so this header cannot rot without the build and the run saying
// so.
// ---------------------------------------------------------------------------
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <vector>

#include "../Port/src/Ui.h"
#include "../Port/src/Symbols.h"
#include "../Port/src/GlassPass.h"
#include "../opal/backend/dx11_backend.h"
#include "../opal/glass/glass_renderer.h"

namespace swui {

// Bridges an the framework frame into the port's immediate-mode UI.
class Bridge {
public:
    bool Init(ID3D11Device* device, int width, int height) {
        width_ = width;
        height_ = height;
        return glass_.Init(device, width, height);
    }

    void Shutdown() { glass_.Shutdown(); }

    // Call once per host frame, before drawing ported components.
    void BeginFrame(ID3D11DeviceContext* ctx, opal::glass::GlassRenderer* hostGlass,
                    int width, int height, float dt, ImVec2 mouse,
                    bool down, bool pressed, bool released) {
        ctx_ = ctx;
        hostGlass_ = hostGlass;
        if (width != width_ || height != height_) {
            width_ = width;
            height_ = height;
        }
        panels_.clear();
        ui_ = sw::UI{};
        ui_.dl = ImGui::GetBackgroundDrawList();
        ui_.theme = theme_;
        // sw::UI::fonts is a value (a small struct of ImFont*), not a pointer.
        ui_.fonts = fonts_ ? *fonts_ : sw::Fonts{};
        ui_.icons = icons_;
        ui_.screenW = (float)width_;
        ui_.screenH = (float)height_;
        ui_.dt = dt;
        ui_.mouse = mouse;
        ui_.mouseDown = down;
        ui_.mousePressed = pressed;
        ui_.mouseReleased = released;
        ui_.panelSink = &panels_;
        // An the framework host already has a blurred backdrop for its own glass; the
        // ported components can sample it before the lens pass runs.
        ui_.blurSRV = hostGlass ? hostGlass->GetBlurTexture() : nullptr;
    }

    // Runs the port's lens pass over the panels a page recorded, so ported glass
    // composites with its measured refraction, rim and lift inside the host's frame.
    // Call it after the page has drawn, before the host composites its own overlays.
    void EndFrame(const float clear[4]) {
        if (!ctx_ || panels_.empty()) return;
        glass_.BeginLens(ctx_);
        for (const sw::GlassPanel& p : panels_) {
            if (p.radius < 1.0f) continue;
            glass_.BindLensSource(ctx_, p.clear);
            float tint[4] = { p.tint.r, p.tint.g, p.tint.b, p.tint.a * p.opacity };
            float lift[4] = { p.lift.r, p.lift.g, p.lift.b, 1.0f };
            const float refract = std::min(p.radius * 0.55f, 14.0f) * p.refractScale;
            const float rim = 0.145f * p.rimScale;
            const float shadow = p.shadow ? 0.20f : 0.0f;
            glass_.DrawPanel(ctx_, p.r.x, p.r.y, p.r.w, p.r.h, p.radius, tint, refract,
                             rim, shadow, 16.0f, 0.012f, p.liftMode ? lift : nullptr);
        }
        (void)clear;
    }

    // A ported page draws text, so it needs the host's loaded faces and its theme.
    // Without them `ui().fonts` is null and the first Text() call is undefined.
    void UseFonts(const sw::Fonts* fonts) { fonts_ = fonts; }
    void UseTheme(const sw::Theme& theme) { theme_ = theme; }
    bool Ready() const { return fonts_ != nullptr; }

    sw::UI& ui() { return ui_; }
    const std::vector<sw::GlassPanel>& panels() const { return panels_; }
    sw::GlassPass& glass() { return glass_; }
    void UseIcons(const sw::SfIcons* icons) { icons_ = icons; }

private:
    sw::UI ui_{};
    sw::GlassPass glass_{};
    std::vector<sw::GlassPanel> panels_{};
    const sw::SfIcons* icons_ = nullptr;
    const sw::Fonts* fonts_ = nullptr;
    sw::Theme theme_ = sw::Theme::Dark();
    ID3D11DeviceContext* ctx_ = nullptr;
    opal::glass::GlassRenderer* hostGlass_ = nullptr;
    int width_ = 0, height_ = 0;
};

} // namespace swui
