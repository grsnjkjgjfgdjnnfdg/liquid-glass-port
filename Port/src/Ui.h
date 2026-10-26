#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Theme.h"
#include "Spring.h"
#include "SfIcons.h"

namespace sw {

// Screenshot automation hook: a row index to hold highlighted without a real
// pointer, because a capture run has no input device. Namespace scope rather than
// a function-local static in a loop, and never thread_local (this codebase forbids
// TLS in injected images).
inline int& HoverRowOverride() {
    static int v = -1;
    return v;
}

// ImGui sizes a font by its ascent+descent box, while Apple's type sizes are em
// sizes, so every nominal point size is multiplied by this to match the measured
// on-device glyph height (34 pt large title -> 26 px ink).
static constexpr float kTextScale = 1.18f;

// The frame is rasterised at 3x (see main.cpp), so a text origin may sit on a
// third of a point â€” the device pixel grid. Snapping to whole points instead
// throws away two thirds of the available resolution and quantises every glyph's
// phase, which is exactly the residual the phase sweeps keep finding.
static constexpr float kRenderScale = 3.0f;
inline float SnapPx(float v) { return std::round(v * kRenderScale) / kRenderScale; }

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool Contains(float px, float py) const { return px >= x && py >= y && px <= x + w && py <= y + h; }
    Rect Inset(float p) const { return Rect(x + p, y + p, w - p * 2, h - p * 2); }
    Rect Offset(float dx, float dy) const { return Rect(x + dx, y + dy, w, h); }
};

// A glass panel recorded during the scene pass and stamped by the lens shader
// between passes, then composited at its place in the draw order.
struct GlassPanel {
    Rect r;
    float radius = 24.0f;
    Rgba tint;
    float opacity = 1.0f;
    bool shadow = true;
    bool clear = false;   // .clear material: refracts a sharp backdrop, does not frost
    // Scale on the lens displacement. The measured chart profile peaks near 8 pt and is
    // gone by 14 pt, while a frosted panel wants the full radius-derived value.
    float refractScale = 1.0f;
    // Per-channel lift measured from the runtime: out = base + lift * (1 - base).
    // This is the form the real material takes (an affine lift toward white with
    // a different coefficient per channel, which is what its vibrant colour
    // matrix does). When set, it replaces the tint/alpha model entirely.
    Rgba lift;
    bool liftMode = false;
    // Scale on both edge terms. Measured: the tinted panel has no hairline at all
    // (a 3.3 pt soft ramp) and the horizontal line appears on only three of the six
    // materials, so one law cannot cover every panel.
    float edge = 1.0f;
    // How far inside the *authored* rect the visible silhouette sits. Panels laid out
    // by the SwiftUI geometry are nominal (0.48); rects fitted to a capture's visible
    // edge already carry it (0).
    float edgeInset = 0.0f;
    // Scale on the specular rim: the runtime puts a bright ridge on the glass-page
    // panels but not on the search field, whose edge only darkens.
    float rimScale = 1.0f;
    // How much of the shape the material actually covers, 0..1. Apple's controls
    // fade one material into another (an opaque thumb becoming a lens as it is
    // grabbed), and a panel whose tint alpha is 0 still covers its rect completely,
    // so the two cannot be expressed with the tint alone.
    float coverage = 1.0f;
    // 1 lights the specular rim on every edge instead of only the horizontal ones,
    // which is what a knob or a pill does (measured on the grabbed slider thumb).
    float ring = 0.0f;
    // Chroma multiplier. A panel puts chroma back on top of its white lift (1.9 is
    // the measured value); a knob takes it out (1.0).
    float vibrancy = 1.90f;
    // Frost radius in points for a control's lens. 0 uses the panel blur; a knob
    // binds the sharp scene and frosts it in the shader instead.
    float frost = 0.0f;
};

struct Fonts {
    ImFont* regular11 = nullptr;
    ImFont* regular12 = nullptr;
    ImFont* regular13 = nullptr;
    ImFont* regular14 = nullptr;
    ImFont* regular15 = nullptr;
    ImFont* regular17 = nullptr;
    ImFont* semibold11 = nullptr;
    ImFont* semibold13 = nullptr;
    ImFont* semibold15 = nullptr;
    ImFont* semibold17 = nullptr;
    ImFont* semibold20 = nullptr;   // SF Pro Text, the 20 pt title role
    ImFont* semibold22 = nullptr;
    ImFont* semibold22tight = nullptr;   // no optical tracking (large-text role)
    ImFont* semibold28 = nullptr;
    ImFont* semibold34 = nullptr;
    ImFont* bold34 = nullptr;       // real SF Pro Display Bold, for the large titles
    ImFont* rowLabel17 = nullptr;  // SF Pro Text, for the left-aligned row labels
    ImFont* subLabel14 = nullptr;  // SF Pro Text, for the left-aligned subtitles
    ImFont* titleSemibold34 = nullptr;  // SF Pro Display Semibold at 34, for experiments
    ImFont* titleBold34 = nullptr;      // SF Pro Display Bold 34, no tracking (identified)
    ImFont* segLabel13 = nullptr;       // SF Pro Text Semibold 13, segmented labels
    ImFont* accountTitle20 = nullptr;   // SF Pro Text Bold 20, the account block title
};

class UI {
public:
    // The unblurred scene capture: a .clear glass material keeps the backdrop crisp
    // (it refracts, it does not frost), so those panels sample this instead.
    ID3D11ShaderResourceView* sharpSRV = nullptr;
    ImDrawList* dl = nullptr;
    Fonts fonts;
    Theme theme = Theme::Light();
    ID3D11ShaderResourceView* blurSRV = nullptr;
    const SfIcons* icons = nullptr;
    // Glyphs taken from the runtime capture (see SfMasks.h); null until created.
    const class SfMasks* masks = nullptr;
    // Pass 1 of a glass frame renders the scene only (the blur source), so the
    // material itself must not be recorded into it.
    bool suppressGlass = false;
    // Pass 1 records every panel here; pass 2 composites the lens target instead
    // of drawing the material with draw lists.
    std::vector<GlassPanel>* panelSink = nullptr;
    ID3D11ShaderResourceView* lensSRV = nullptr;
    float lensMargin = 16.0f;
    float screenW = 1440.0f, screenH = 900.0f;
    float time = 0.0f, dt = 1.0f / 60.0f;
    ImVec2 mouse{ 0, 0 };
    // A page translated *after* it draws (the desktop host uses VtxTranslate) still
    // needs its manual clip rects to land in the same place, so they carry this.
    float clipOffsetX = 0.0f, clipOffsetY = 0.0f;
    bool mouseDown = false;
    bool mousePressed = false;
    bool mouseReleased = false;
    const void* active = nullptr;
    // Horizontal drawing offset, in points. A pushed navigation page is the same
    // drawing code at a different x (the outgoing page parallaxes by 30 % of the
    // width while the incoming one travels the full width), so every primitive
    // adds this to its x. The panel recorder and the glass lens use the same
    // offset, which keeps the material attached to the page it belongs to.
    float dx = 0.0f;
    float dy = 0.0f;
    // Page origin in world space: Glass() needs it because the lens target is
    // stamped between the two passes, before a page-level vertex translation has
    // run. Without it a hosted page's search bar is stamped in the wrong place.
    float originX = 0.0f;
    float originY = 0.0f;

    void PushClip(const Rect& r) { dl->PushClipRect(ImVec2(r.x + dx, r.y + dy), ImVec2(r.x + r.w + dx, r.y + r.h + dy), true); }
    void PopClip() { dl->PopClipRect(); }

    // Page-level translation for transitions.
    //
    // `dx` shifts the UI primitives, but a page also contains raw draw-list calls
    // and atlas blits (icons, the avatar, gradient fills) that bypass them. A push
    // moves a whole page, so the page is translated at the vertex level instead:
    // mark the vertex count before drawing, translate everything added after it.
    // This cannot miss an element, and it is exactly what the compositor does on
    // the device â€” the layer's position is a transform over its whole subtree.
    int VtxMark() const { return dl->VtxBuffer.Size; }
    void VtxTranslate(int from, float tx, float ty = 0.0f) {
        if (!dl || tx == 0.0f && ty == 0.0f) return;
        for (int i = from; i < dl->VtxBuffer.Size; ++i) {
            dl->VtxBuffer[i].pos.x += tx;
            dl->VtxBuffer[i].pos.y += ty;
        }
    }
    // Glass panels are recorded in *page* coordinates and stamped into the lens
    // target by the pass between the two UI passes, so a page that moves after it
    // is drawn must move its recorded panels too â€” otherwise the lens texture holds
    // the panel at its old place and the composite samples the wrong pixels (a
    // hosted page's search bar reappears in the canvas's top-left corner).
    int PanelMark() const { return panelSink ? (int)panelSink->size() : 0; }
    void TranslatePage(int vtxMark, int panelMark, float tx, float ty = 0.0f) {
        VtxTranslate(vtxMark, tx, ty);
        if (!panelSink) return;
        for (int i = panelMark; i < (int)panelSink->size(); ++i) {
            (*panelSink)[i].r.x += tx;
            (*panelSink)[i].r.y += ty;
        }
    }

    ImU32 Col(const Rgba& c) const { return c.U32(); }
    ImU32 Mix(const Rgba& a, const Rgba& b, float t) const { return a.Lerp(b, t).U32(); }

    // ImGui truncates a text origin to whole *points* before laying the glyph run
    // out, so asking for x = 73.67 draws exactly where x = 73 does. On a 3x frame
    // that throws away two thirds of the resolution and quantises every run's
    // phase â€” which is the residual the phase sweeps kept finding. Draw on the
    // integer grid and move the vertices produced by the fractional part, which
    // the projection then scales to a third of a point.
    void TextEx(ImFont* f, float size, float x, float y, const char* s, const Rgba& c) {
        const float fx = x + dx;
        const float bx = std::floor(fx);
        const float by = std::floor(y);
        const int mark = dl->VtxBuffer.Size;
        dl->AddText(f, size * kTextScale, ImVec2(bx, by), c.U32(), s);
        const float ox = fx - bx;
        const float oy = y - by;
        if (ox != 0.0f || oy != 0.0f) VtxTranslate(mark, ox, oy);
    }

    void Text(ImFont* f, float size, float x, float y, const char* s, const Rgba& c) {
        TextEx(f, size, x, y, s, c);
    }
    float TextWidth(ImFont* f, float size, const char* s) {
        return f->CalcTextSizeA(size * kTextScale, FLT_MAX, 0.0f, s).x;
    }
    // The faces at and above 20 pt are the calibrated ones, which have no Bold cut,
    // so weight is synthesised by a 0.42 pt sub-pixel double strike.
    void TextBold(ImFont* f, float size, float x, float y, const char* s, const Rgba& c) {
        TextEx(f, size, x, y, s, c);
        const float bx = std::floor(x + dx);
        const float by = std::floor(y);
        const int mark = dl->VtxBuffer.Size;
        // A white glyph on a dark surface reads heavier than the same glyph in ink, so
        // the synthetic strike is widened in dark mode; the measured dark title's ink
        // was thinner than the runtime's at the light-mode value.
        const float strike = 0.42f;
        dl->AddText(f, size * kTextScale, ImVec2(bx + strike, by), c.U32(), s);
        const float ox = (x + dx) - bx;
        const float oy = y - by;
        if (ox != 0.0f || oy != 0.0f) VtxTranslate(mark, ox, oy);
    }
    // Semibold + a 0.42 pt double strike, for text whose size has no real Bold face
    // in the embedded set (the account block's 20 pt title).
    void TextBoldStrike(ImFont* f, float size, float x, float y, const char* s, const Rgba& c) {
        TextBold(f, size, x, y, s, c);
        const float bx = std::floor(x + dx);
        const float by = std::floor(y);
        const int mark = dl->VtxBuffer.Size;
        dl->AddText(f, size * kTextScale, ImVec2(bx + 0.42f, by), c.U32(), s);
        const float ox = (x + dx) - bx;
        const float oy = y - by;
        if (ox != 0.0f || oy != 0.0f) VtxTranslate(mark, ox, oy);
    }
    void TextCentered(ImFont* f, float size, const Rect& r, const char* s, const Rgba& c) {
        ImVec2 m = f->CalcTextSizeA(size * kTextScale, FLT_MAX, 0.0f, s);
        TextEx(f, size, r.x + (r.w - m.x) * 0.5f, r.y + (r.h - m.y) * 0.5f, s, c);
    }
    void TextRight(ImFont* f, float size, const Rect& r, const char* s, const Rgba& c) {
        ImVec2 m = f->CalcTextSizeA(size * kTextScale, FLT_MAX, 0.0f, s);
        TextEx(f, size, r.x + r.w - m.x, r.y + (r.h - m.y) * 0.5f, s, c);
    }
    // Vertically centres text on a baseline-ish cap height instead of the whole
    // line box, which is what Apple's rows do.
    void TextVCenter(ImFont* f, float size, float x, const Rect& r, const char* s, const Rgba& c) {
        ImVec2 m = f->CalcTextSizeA(size * kTextScale, FLT_MAX, 0.0f, s);
        TextEx(f, size, x, r.y + (r.h - m.y) * 0.5f, s, c);
    }

    void FillRounded(const Rect& r, float radius, const Rgba& c) {
        dl->AddRectFilled(ImVec2(r.x + dx, r.y + dy), ImVec2(r.x + r.w + dx, r.y + r.h + dy), c.U32(), radius);
    }
    void StrokeRounded(const Rect& r, float radius, const Rgba& c, float thickness = 1.0f) {
        dl->AddRect(ImVec2(r.x + dx, r.y + dy), ImVec2(r.x + r.w + dx, r.y + r.h + dy), c.U32(), radius, 0, thickness);
    }
    void Line(float x1, float y1, float x2, float y2, const Rgba& c, float t = 1.0f) {
        dl->AddLine(ImVec2(x1 + dx, y1 + dy), ImVec2(x2 + dx, y2 + dy), c.U32(), t);
    }

    void Paragraph(ImFont* f, float size, float x, float y, float width,
                   const char* text, const Rgba& c, float lineHeight) {
        std::string word, line;
        const char* p = text;
        auto flush = [&](const std::string& s, float yy) {
            dl->AddText(f, size * kTextScale, ImVec2(x + dx, yy + dy), c.U32(), s.c_str());
        };
        float cy = y;
        while (*p) {
            while (*p == ' ') ++p;
            word.clear();
            while (*p && *p != ' ') word.push_back(*p++);
            std::string next = line.empty() ? word : line + " " + word;
            if (TextWidth(f, size, next.c_str()) > width && !line.empty()) {
                flush(line, cy);
                cy += lineHeight;
                line = word;
            } else {
                line = next;
            }
        }
        if (!line.empty()) flush(line, cy);
    }

    // Multi-layer soft shadow. Apple shadows are broad, low-alpha and slightly
    // biased downward; stacking rounded rects approximates the falloff well
    // enough that no hard edge is visible.
    void Shadow(const Rect& r, float radius, float spread, float alpha, float dy = 0.0f) {
        for (int i = 0; i < 6; ++i) {
            float e = spread * (1.0f - i * 0.14f);
            // Divided by the layer count: overlapping rounded rects must add up
            // to `alpha` at the edge, not to six times it (a hard halo).
            float a = alpha / 6.0f * (0.7f + i * 0.12f);
            Rect s(r.x - e, r.y - e + dy, r.w + e * 2, r.h + e * 2);
            FillRounded(s, radius + e, Rgba(0, 0, 0, a));
        }
    }

    // The shadow Apple puts under a switch knob / slider thumb: tight, centred,
    // roughly 22% black at the border fading out within ~5 pt.
    void ControlShadow(const Rect& r, float radius, float alpha = 0.10f) {
        for (int i = 3; i >= 1; --i) {
            float e = (float)i * 1.05f;
            Rect s(r.x - e, r.y - e + 0.8f, r.w + e * 2, r.h + e * 2);
            FillRounded(s, radius + e, Rgba(0, 0, 0, alpha * 0.30f / i));
        }
    }

    // Glass material: a real blurred desktop capture underneath, then a tint,
    // a single achromatic rim and a restrained top highlight.
    //
    // `originX/originY` place the page in world space for the *lens* only: a page
    // that is moved after drawing (host window, navigation push) has its vertices
    // translated at the end, but the lens target is stamped between the passes, so
    // the panel's world rectangle has to be known while it is being recorded.
    void Glass(const Rect& rIn, float radius, const Rgba& tint, float opacity,
               bool shadow = true, ImU32* outTint = nullptr, const Rgba* lift = nullptr,
               float rimScale = 1.0f, bool clear = false, float refractScale = 1.0f,
               float coverage = 1.0f, float ring = 0.0f,
               float vibrancy = 1.90f, float frost = 0.0f, float edge = 1.0f,
               float edgeInset = 0.0f) {
        const Rect rWorld(rIn.x + dx + originX, rIn.y + dy + originY, rIn.w, rIn.h);
        const Rect r(rIn.x + dx, rIn.y + dy, rIn.w, rIn.h);
        if (panelSink) {
            GlassPanel gp{ rWorld, radius, tint, opacity, shadow };
            gp.clear = clear;
            gp.refractScale = refractScale;
            gp.rimScale = rimScale;
            gp.coverage = coverage;
            gp.ring = ring;
            gp.vibrancy = vibrancy;
            gp.frost = frost;
            gp.edge = edge;
            gp.edgeInset = edgeInset;
            if (lift) { gp.lift = *lift; gp.liftMode = true; }
            panelSink->push_back(gp);
        }
        if (lensSRV) {

            if (shadow) {
                // Measured cast shadow: the runtime's panel throws a 2/255 dip over
                // about 6 pt below the bottom edge, not the 14 pt falloff the lens
                // shader used (which measured 1.07 worse when it was enabled). Drawn
                // here, in the UI pass, so it sits on the backdrop the panel floats over.
                const int bands = 8;
                for (int i = 0; i < bands; ++i) {
                    const float t = (float)i / (float)bands;
                    const float y0 = r.y + r.h + t * 6.0f;
                    const float a = 0.009f * (1.0f - t);
                    dl->AddRectFilled(ImVec2(r.x + 3.0f, y0),
                                      ImVec2(r.x + r.w - 3.0f, y0 + 0.8f),
                                      Rgba(0, 0, 0, a * opacity).U32());
                }
            }
            // The lens shader already produced this panel's pixels, shadow
            // included, so compositing is a straight blit at the recorded place â€”
            // drawn in page space (the caller translates afterwards) but sampling
            // the lens target at the panel's world position.
            float m = lensMargin;
            dl->AddImage((ImTextureID)lensSRV,
                         ImVec2(r.x - m, r.y - m), ImVec2(r.x + r.w + m, r.y + r.h + m),
                         ImVec2((rWorld.x - m) / screenW, (rWorld.y - m) / screenH),
                         ImVec2((rWorld.x + rWorld.w + m) / screenW,
                                (rWorld.y + rWorld.h + m) / screenH));
            if (outTint) *outTint = tint.U32();
            return;
        }
        if (suppressGlass) return;
        if (shadow) Shadow(r, radius, 10.0f, 0.13f * opacity, 2.0f);
        if (blurSRV) {
            // 1. Body. Sampling from slightly inside the panel magnifies the
            //    backdrop by ~1.5 %, which is what makes the material read as a
            //    lens rather than a hole in the page.
            float m = 0.006f;
            ImVec2 uv0(r.x / screenW + m * r.w / screenW, r.y / screenH + m * r.h / screenH);
            ImVec2 uv1((r.x + r.w) / screenW - m * r.w / screenW,
                       (r.y + r.h) / screenH - m * r.h / screenH);
            dl->AddImageRounded((ImTextureID)blurSRV, ImVec2(r.x, r.y),
                                ImVec2(r.x + r.w, r.y + r.h), uv0, uv1,
                                IM_COL32(255, 255, 255, 255), radius, ImDrawFlags_RoundCornersAll);

            // 2. Bevel. A band along the inside of the rim samples content from
            //    *deeper inside* the panel, so the backdrop appears compressed
            //    toward the edge: the SDF refraction of a real lens, done with
            //    displaced UV quads instead of a shader.
            // The strips live inside the straight sections of the rounded rect,
            // so they never cut across a corner. Two bands per edge (a strong
            // outer ring and a soft inner one) approximate a smooth SDF lens
            // without a shader.
            float bevel = std::min(radius * 0.55f, 10.0f);
            if (bevel > 1.0f) {
                auto band = [&](ImVec2 d0, ImVec2 d1, float dx, float dy, int alpha) {
                    dl->AddImage((ImTextureID)blurSRV, d0, d1,
                                 ImVec2((d0.x + dx) / screenW, (d0.y + dy) / screenH),
                                 ImVec2((d1.x + dx) / screenW, (d1.y + dy) / screenH),
                                 IM_COL32(255, 255, 255, alpha));
                };
                for (int pass = 0; pass < 2; ++pass) {
                    float k = (pass == 0) ? bevel * 0.85f : bevel * 0.5f;
                    float th = (pass == 0) ? bevel : bevel * 0.5f;
                    int a = (pass == 0) ? 255 : 120;
                    band(ImVec2(r.x + radius, r.y), ImVec2(r.x + r.w - radius, r.y + th), 0, k, a);
                    band(ImVec2(r.x + radius, r.y + r.h - th), ImVec2(r.x + r.w - radius, r.y + r.h), 0, -k, a);
                    band(ImVec2(r.x, r.y + radius), ImVec2(r.x + th, r.y + r.h - radius), k, 0, a);
                    band(ImVec2(r.x + r.w - th, r.y + radius), ImVec2(r.x + r.w, r.y + r.h - radius), -k, 0, a);
                }
            }
        }
        // Rim first, tint over it: Apple's edge highlight is a light inner line,
        // and a heavy tint must soften it rather than leave a grey ring where the
        // backdrop is darkest.
        StrokeRounded(Rect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f),
                      radius - 0.5f, Rgba(1, 1, 1, theme.dark ? 0.16f : 0.46f), 1.0f);
        Rgba t = tint;
        float op = opacity;
        if (lift && !lensSRV) {
            // No lens pass in this frame (an Opal host owns the backdrop): the measured
            // per-channel lift is approximated by lerping white at the mean lift, which is
            // the same family of transform and keeps the frost reading correctly.
            t = Rgba(1, 1, 1, 1);
            op = opacity * (lift->r + lift->g + lift->b) / 3.0f;
        }
        t.a *= op;
        FillRounded(r, radius, t);
        dl->AddRectFilledMultiColor(ImVec2(r.x + 2, r.y + 1),
                                    ImVec2(r.x + r.w - 2, r.y + std::min(r.h * 0.42f, 54.0f)),
                                    IM_COL32(255, 255, 255, (int)(11 * opacity)),
                                    IM_COL32(255, 255, 255, (int)(7 * opacity)),
                                    IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        if (outTint) *outTint = t.U32();
    }

    void CircleShadow(float cx, float cy, float radius, float alpha) {
        for (int i = 5; i >= 1; --i) {
            float rr = radius + (float)i * 1.8f;
            dl->AddCircleFilled(ImVec2(cx + dx, cy + i * 0.8f + dy), rr,
                                IM_COL32(0, 0, 0, (int)(alpha * 255 / (i + 2))), 32);
        }
    }
};

// ------------------------------- controls ---------------------------------
//
// Geometry and colour for every control below is measured from the iOS 27
// runtime (3x simulator pixels + accessibility frames). Do not "tidy" these
// numbers â€” they are the port.

// Apple's disclosure indicator: 7 x 12 pt at the trailing edge.
inline void DrawChevron(UI& ui, float x, float y, float h = 12.0f, const Rgba* colour = nullptr) {
    Rgba c = colour ? *colour : ui.theme.chevron;
    float w = h * 7.0f / 12.0f;
    ui.Line(x, y, x + w, y + h * 0.5f, c, 1.9f);
    ui.Line(x + w, y + h * 0.5f, x, y + h, c, 1.9f);
}

struct SwitchState {
    Spring t;        // 0 = off, 1 = on (track colour crossfade only)
    Spring pos;      // knob position 0..1, critically damped: the knob never overshoots
    Spring press;    // held state (its own spring, so a future squish has a source)
    bool held = false;
    bool seeded = false;   // first frame snaps instead of animating in from "off"
    // Last drawn knob geometry, so the animation can be sampled and diffed
    // against the runtime the same way layout is.
    float lastW = 0.0f, lastX = 0.0f, lastT = 0.0f, lastLead = 0.0f, lastTrail = 0.0f;
    float lastSwell = 0.0f;
    // Position: measured from the recorded flip (120 Hz layer samples + the 60 fps
    // recording). The travel has **no overshoot** - the previous model drove two edge
    // springs, the leading one at damping 0.28, which pushed the knob 8.8 pt past its
    // rest position and is what read as "too bouncy". The liquid feel comes from the
    // swell below, not from the travel.
    // Travel 0.20 -> 0.15 s: measured, the capture's knob is within 0.33 pt of its
    // final centre 133 ms after the flip, where a 0.20 s spring is still 2.17 pt out.
    SwitchState() : t(0, 0.30f, 0.85f), pos(0, 0.15f, 1.0f), press(0, 0.18f, 0.9f) {}
};

inline void DrawSwitch(UI& ui, const Rect& r, bool& on, SwitchState& st, float dt) {
    const float h = ios::kSwitchH;
    const float w = ios::kSwitchW;
    Rect track(r.x, r.y + (r.h - h) * 0.5f, w, h);
    Rect hot(track.x - 6, track.y - 6, track.w + 12, track.h + 12);

    // UISwitch commits on touch-up, and only if the touch is still inside: flipping on
    // touch-down meant a press-and-drag-off still toggled.
    const bool inside = hot.Contains(ui.mouse.x, ui.mouse.y);

    if (inside && ui.mousePressed) st.held = true;
    if (ui.mouseReleased) {
        if (st.held && inside) on = !on;
        st.held = false;
    }
    if (!inside && !ui.mouseDown) st.held = false;

    if (!st.seeded) {
        st.seeded = true;
        st.pos.Snap(on ? 1.0f : 0.0f);
        st.t.Snap(on ? 1.0f : 0.0f);
    }
    st.press.SetTarget(st.held ? 1.0f : 0.0f);
    st.press.Update(dt);
    st.t.SetTarget(on ? 1.0f : 0.0f);
    st.t.Update(dt);
    st.pos.SetTarget(on ? 1.0f : 0.0f);
    st.pos.Update(dt);
    const float t = std::clamp(st.t.value, 0.0f, 1.0f);
    const float p = std::clamp(st.pos.value, 0.0f, 1.0f);

    // The knob does not simply slide: measured from the runtime it swells 37 x 24 ->
    // 47.03 x 30.84 about its own centre while it travels, and *the track spreads with
    // it* - the control's bounding box goes 63.00 x 28.33 -> 71.00 x 39.33 at the peak,
    // which a 47 pt knob inside a 63 pt pill cannot produce. Driving both off the
    // spring's speed reproduces the amplitude and the 210 ms return.
    const float knobW = ios::kSwitchKnobW;
    const float knobH = ios::kSwitchKnobH;
    const float inset = ios::kSwitchInset;
    const float travel = w - inset * 2.0f - knobW;
    const float speed = std::fabs(st.pos.velocity) * travel;      // pt/s
    const float swell = std::min(ios::kSwitchSwell, speed * ios::kSwitchSwellK);
    const float swellN = swell / ios::kSwitchSwell;

    // Track: colour crossfades; the off fill is measurably darker than a plain
    // systemFill so it is its own token. It also blooms during the flip, about its own
    // centre, so the whole control breathes instead of just the knob.
    Rgba trackCol = ui.theme.switchOff.Lerp(ui.theme.switchOn, t);
    Rect bloomed(track.x - ios::kSwitchBloomW * swellN,
                 track.y - ios::kSwitchBloomH * swellN,
                 track.w + ios::kSwitchBloomW * swellN * 2.0f,
                 track.h + ios::kSwitchBloomH * swellN * 2.0f);
    ui.FillRounded(bloomed, bloomed.h * 0.5f, trackCol);

    const float kw = knobW * (1.0f + swell);
    const float kh = knobH * (1.0f + swell);
    const float cx = track.x + inset + knobW * 0.5f + p * travel;
    Rect knob(cx - kw * 0.5f, bloomed.y + (bloomed.h - kh) * 0.5f, kw, kh);
    st.lastW = kw;
    st.lastX = knob.x - track.x;
    st.lastT = t;
    st.lastSwell = swell;
    st.lastLead = knob.x;
    st.lastTrail = knob.x + kw;

    // The knob is opaque at rest and a frosted lens while it morphs: measured, the
    // track reads through it at the peak of the swell (~24 % white in dark, where the
    // arithmetic is not polluted by the white card behind it). The two halves of the
    // cross-fade need the passes separated, because the lens samples the scene
    // capture and a knob painted into that capture would be sampled by the lens that
    // sits on it - the knob would refract itself and stay white. At rest nothing at
    // all changes: no panel is recorded and the knob is painted into the capture
    // exactly as before, so a still frame of a switch is the image it always was.
    const bool recPass = (ui.panelSink != nullptr);
    const bool visPass = (ui.lensSRV != nullptr);
    const bool still = (swellN < 0.01f);
    if (!recPass && !visPass) {                 // host window: one pass, no lens
        ui.ControlShadow(knob, knob.h * 0.5f, 0.20f);
        ui.FillRounded(knob, knob.h * 0.5f, ui.theme.knob);
        return;
    }
    if (recPass) {
        if (still) {
            ui.ControlShadow(knob, knob.h * 0.5f, 0.20f);
            ui.FillRounded(knob, knob.h * 0.5f, ui.theme.knob);
        }
    } else {
        ui.ControlShadow(knob, knob.h * 0.5f, 0.20f + 0.10f * swellN);
        if (swellN < 0.999f) {
            const Rgba& k = ui.theme.knob;
            ui.FillRounded(knob, knob.h * 0.5f, Rgba(k.r, k.g, k.b, k.a * (1.0f - swellN)));
        }
    }
    // Recorded by pass 1, composited by pass 2.
    if (!still) {
        ui.Glass(knob, knob.h * 0.5f, Rgba(1.0f, 1.0f, 1.0f, ios::kKnobGlassTint),
                 1.0f, false, nullptr, nullptr, ios::kKnobGlassRim, true,
                 ios::kKnobGlassRefract, swellN, 1.0f, ios::kKnobGlassVibrancy,
                 ios::kKnobGlassFrost);
    }
}

struct SliderState {
    Spring pos;
    Spring hover;
    Spring grab;      // thumb scale while dragging
    bool dragging = false;
    float grabDX = 0.0f;   // touch offset inside the thumb, preserved while dragging
    SliderState() : pos(0.62f, 0.30f, 1.0f), hover(0, 0.18f, 0.9f), grab(0, 0.16f, 0.9f) {}
};

inline float DrawSlider(UI& ui, const Rect& r, float& value, SliderState& st, float dt) {
    const float thumbW = ios::kSliderThumbW;
    const float thumbH = ios::kSliderThumbH;
    const float trackH = ios::kTrackH;
    float travel = std::max(1.0f, r.w - thumbW);
    float cy = r.y + r.h * 0.5f;
    Rect hot(r.x, cy - thumbH * 0.5f - 6, r.w, thumbH + 12);
    bool hit = hot.Contains(ui.mouse.x, ui.mouse.y);

    auto valueFor = [&](float mx) {
        return std::clamp((mx - r.x - thumbW * 0.5f) / travel, 0.0f, 1.0f);
    };
    // A UISlider does nothing when the track is touched away from the thumb, and a grab
    // keeps the touch's offset inside the thumb instead of recentring it under the cursor
    // (which used to jump the value by up to half a thumb width on touch-down).
    const float knobCentre = r.x + thumbW * 0.5f + travel * value;
    const bool onThumb = hit && std::fabs(ui.mouse.x - knobCentre) <= thumbW * 0.5f + 4.0f;
    if (onThumb && ui.mousePressed) {
        st.dragging = true;
        st.grabDX = ui.mouse.x - knobCentre;
    }
    if (st.dragging && ui.mouseDown) value = valueFor(ui.mouse.x - st.grabDX);
    if (ui.mouseReleased) st.dragging = false;

    st.hover.SetTarget(hit ? 1.0f : 0.0f);
    st.hover.Update(dt);
    st.grab.SetTarget(st.dragging ? 1.0f : 0.0f);
    st.grab.Update(dt);
    // Measured: no layer moves during a drag on the real UISlider â€” the control's
    // contents are redrawn by a drawing layer, not animated â€” so the thumb is
    // glued to the cursor with no settle at all. Only programmatic changes ease,
    // and only briefly.
    if (st.dragging) {
        st.pos.Snap(value);
    } else {
        st.pos.response = 0.18f;
        st.pos.dampingFraction = 1.0f;
        st.pos.SetTarget(value);
        st.pos.Update(dt);
    }

    float t = std::clamp(st.pos.value, 0.0f, 1.0f);
    float knobC = r.x + thumbW * 0.5f + t * travel;

    Rect track(r.x, cy - trackH * 0.5f, r.w, trackH);
    ui.FillRounded(track, trackH * 0.5f, ui.theme.trackBg);
    if (knobC - r.x > 0.5f) {
        Rect fill(r.x, track.y, std::min(knobC - r.x, r.w), trackH);
        ui.FillRounded(fill, trackH * 0.5f, ui.theme.trackFill);
    }

    float grab = std::clamp(st.grab.value, 0.0f, 1.0f);
    // Measured from the runtime (60 fps recording of a real drag, mid-drag frame at
    // 46.193 s): grabbing the thumb balloons it from 37 x 24 to roughly 58 x 35 and
    // turns it into frosted glass - the track reads through it at about 50 % white,
    // the backdrop behind it is smeared, and there is a bright rim on *every* edge,
    // not just the horizontal ones. It contracts again while it is dragged and is
    // opaque once released, which is why both axes ramp on `grab`.
    // The measured held size is 57.3 x 37.0 pt (the probe's focus stream caught the
    // glass stack on a 57.3 x 37.0 layer), i.e. 1.55x on both axes from 37 x 24 - a
    // uniform swell, not a stretch.
    float scaleX = 1.0f + grab * 0.55f;
    float scaleY = 1.0f + grab * 0.54f;
    Rect thumb(knobC - thumbW * scaleX * 0.5f, cy - thumbH * scaleY * 0.5f,
               thumbW * scaleX, thumbH * scaleY);

    const bool recPass = (ui.panelSink != nullptr);
    const bool visPass = (ui.lensSRV != nullptr);
    const bool still = (grab < 0.01f);
    if (!recPass && !visPass) {                      // host window: one pass, no lens
        ui.ControlShadow(thumb, thumb.h * 0.5f, 0.22f + grab * 0.06f);
        ui.FillRounded(thumb, thumb.h * 0.5f, ui.theme.knob);
        return t;
    }
    if (recPass) {
        if (still) {
            ui.ControlShadow(thumb, thumb.h * 0.5f, 0.22f + grab * 0.06f);
            ui.FillRounded(thumb, thumb.h * 0.5f, ui.theme.knob);
        }
    } else {
        ui.ControlShadow(thumb, thumb.h * 0.5f, 0.22f + grab * 0.06f);
        if (grab < 0.999f) {
            const Rgba& k = ui.theme.knob;
            ui.FillRounded(thumb, thumb.h * 0.5f, Rgba(k.r, k.g, k.b, k.a * (1.0f - grab)));
        }
    }
    // Held: a frosted lens over the track. Recorded by pass 1, composited by pass 2,
    // so the opaque half above is only ever painted in the visible pass - otherwise
    // the lens would sample the thumb it is replacing and stay white.
    if (!still) {
        ui.Glass(thumb, thumb.h * 0.5f, Rgba(1.0f, 1.0f, 1.0f, ios::kThumbGlassTint), grab,
                 false, nullptr, nullptr, ios::kThumbGlassRim, true,
                 ios::kThumbGlassRefract, grab, 1.0f, ios::kThumbGlassVibrancy,
                 ios::kThumbGlassFrost);
    }
    return t;
}

struct SegState {
    Spring index;
    SegState() : index(0, 0.30f, 0.86f) {}
    // Both demos start with "Dark" selected; without this the pill animates in from
    // "Light" on the first frames of every run.
    void SnapTo(int i) { index.Snap((float)i); }
};

inline void DrawSegmented(UI& ui, const Rect& r, int& selected, int count, SegState& st,
                          const char* const* labels, float dt) {
    const float h = std::min(ios::kSegH, r.h);
    Rect track(r.x, r.y + (r.h - h) * 0.5f, r.w, h);
    ui.FillRounded(track, h * 0.5f, ui.theme.segTrack);

    if (ui.mousePressed && track.Contains(ui.mouse.x, ui.mouse.y)) {
        int idx = (int)((ui.mouse.x - track.x) / (track.w / (float)count));
        selected = std::clamp(idx, 0, count - 1);
    }
    st.index.SetTarget((float)selected);
    st.index.Update(dt);

    float segW = track.w / (float)count;
    float x = track.x + segW * std::clamp(st.index.value, 0.0f, (float)count - 1.0f);
    const float inset = ios::kSegPillInset;
    Rect pill(x + inset, track.y + inset, segW - inset * 2.0f, track.h - inset * 2.0f);
    ui.Shadow(pill, pill.h * 0.5f, 2.5f, 0.10f, 0.5f);
    ui.FillRounded(pill, pill.h * 0.5f, ui.theme.segPill);

    for (int i = 0; i < count; ++i) {
        // The segmented labels sit a third of a point lower in the runtime, measured
        // in both appearances, like the row labels.
        Rect cell(track.x + segW * i, track.y + 0.33f, segW, track.h);
        ui.TextCentered(ui.fonts.semibold13, 13.0f, cell, labels[i], ui.theme.text);
    }
}

struct ButtonState {
    Spring press;
    ButtonState() : press(0, 0.18f, 0.85f) {}
};

enum class ButtonKind { Prominent, Bordered, Plain };

inline void DrawButton(UI& ui, const Rect& r, const char* label, ButtonKind kind,
                       ButtonState& st, float dt) {
    bool hit = r.Contains(ui.mouse.x, ui.mouse.y);
    st.press.SetTarget((hit && ui.mouseDown) ? 1.0f : 0.0f);
    st.press.Update(dt);
    float p = std::clamp(st.press.value, 0.0f, 1.0f);
    // `.plain`/`.system` titles dim on press; only the filled and bordered styles shrink.
    // Scaling a bare title read as a zoom, not a highlight.
    float scale = (kind == ButtonKind::Plain) ? 1.0f : (1.0f - p * 0.03f);
    Rect b(r.x + r.w * (1 - scale) * 0.5f, r.y + r.h * (1 - scale) * 0.5f,
           r.w * scale, r.h * scale);
    float radius = b.h * 0.5f;   // iOS 27 buttons are full capsules

    switch (kind) {
        case ButtonKind::Prominent: {
            Rgba fill = ui.theme.accent;
            fill = fill.Lerp(Rgba(0, 0, 0, 1), p * 0.06f);
            ui.FillRounded(b, radius, fill);
            ui.TextCentered(ui.fonts.rowLabel17, 17.0f, b, label, ui.theme.accentText);
            break;
        }
        case ButtonKind::Bordered: {
            Rgba fill = ui.theme.borderedFill;
            fill = fill.Lerp(ui.theme.accent, p * 0.12f);
            ui.FillRounded(b, radius, fill);
            ui.TextCentered(ui.fonts.rowLabel17, 17.0f, b, label, ui.theme.accent);
            break;
        }
        case ButtonKind::Plain: {
            Rgba ink = ui.theme.accent;
            ink.a *= (1.0f - 0.40f * p);      // dims to 60 % while held
            ui.TextCentered(ui.fonts.rowLabel17, 17.0f, b, label, ink);
            break;
        }
    }
}

// A text field that is actually a field: it takes focus on a press, shows the insertion
// caret against the text's advance with the runtime's half-second blink, and accepts typed
// characters. Before this the `focused` argument was unused and the control was inert - the
// only input on the page that was visibly not an input.
struct TextFieldState {
    bool focused = false;
    // The reference app has a value typed in already (Glass), so the idle field must
    // show it rather than the placeholder - the capture is the specification.
    // The reference app has a value typed in already ("Glass"), so the idle
    // field must show it rather than the placeholder - the capture is the specification.
    char text[64] = "Glass";
    float caretPhase = 0.0f;   // seconds since the caret was last reset by an edit
};

inline void DrawTextField(UI& ui, const Rect& r, TextFieldState& st, const char* placeholder) {
    Rect box(r.x, r.y + (r.h - ios::kFieldH) * 0.5f, r.w, ios::kFieldH);
    const bool hit = box.Contains(ui.mouse.x, ui.mouse.y);
    if (ui.mousePressed) {
        const bool was = st.focused;
        st.focused = hit;
        if (!was && st.focused) st.caretPhase = 0.0f;
    }
    // Typed characters. ImGui queues them per frame; the field takes them while focused.
    if (st.focused) {
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
            const ImWchar c = io.InputQueueCharacters[i];
            const int n = (int)strlen(st.text);
            if (c == 8 || c == 127) {                       // backspace
                if (n > 0) st.text[n - 1] = '\0';
                st.caretPhase = 0.0f;
            } else if (c >= 32 && n < (int)sizeof(st.text) - 1) {
                st.text[n] = (char)c;
                st.text[n + 1] = '\0';
                st.caretPhase = 0.0f;
            }
        }
        io.InputQueueCharacters.resize(0);
    }

    ui.FillRounded(box, ios::kFieldRadius, ui.theme.fieldBg);
    // A focused field takes the accent colour on its border, like the runtime's.
    ui.StrokeRounded(Rect(box.x + 0.5f, box.y + 0.5f, box.w - 1, box.h - 1),
                     ios::kFieldRadius - 0.5f,
                     st.focused ? ui.theme.accent : ui.theme.fieldBorder, 1.0f / 3.0f);
    // ^ one *device* pixel: measured 204 then 255, not a three-pixel halo
    const bool empty = st.text[0] == '\0';
    ui.TextVCenter(ui.fonts.regular17, 17.0f, box.x + 8, box,
                   empty ? placeholder : st.text, empty ? ui.theme.tertiary : ui.theme.text);
    if (st.focused) {
        // The caret sits at the text's advance and blinks at 1 Hz (0.53 s on, 0.53 off),
        // restarting solid after every edit so it is visible where you typed.
        st.caretPhase += ui.dt;
        if (std::fmod(st.caretPhase, 1.06f) < 0.53f) {
            const float adv = empty ? 0.0f
                                    : ui.TextWidth(ui.fonts.regular17, 17.0f, st.text);
            const float cx = box.x + 8.0f + adv + 0.5f;
            ui.dl->AddRectFilled(ImVec2(cx + ui.dx, box.y + 8.0f + ui.dy),
                                 ImVec2(cx + 2.0f + ui.dx, box.y + box.h - 8.0f + ui.dy),
                                 ui.theme.accent.U32());
        }
    }
}

// A rotary control is not an Apple system control. It is included as an
// extension built from the same material, radius and type rules.
struct KnobState { Spring angle; Spring hover; KnobState() : angle(0.68f, 0.28f, 0.8f), hover(0, 0.2f, 0.9f) {} };

inline void DrawKnob(UI& ui, const Rect& area, float& value, KnobState& st, float dt) {
    float cx = area.x + area.w * 0.5f;
    float cy = area.y + area.h * 0.5f;
    float radius = std::min(area.w, area.h) * 0.42f;
    bool hit = (ui.mouse.x - cx) * (ui.mouse.x - cx) + (ui.mouse.y - cy) * (ui.mouse.y - cy) <
               (radius + 16) * (radius + 16);
    if (hit && ui.mousePressed) st.hover.Snap(1.0f);
    if (hit && ui.mouseDown) {
        float a = std::atan2(ui.mouse.y - cy, ui.mouse.x - cx);
        if (a < 0) a += 6.2831853f;
        value = std::clamp((a - 2.3561945f) / 4.712389f, 0.0f, 1.0f);
    }
    st.hover.SetTarget(hit ? 1.0f : 0.0f);
    st.hover.Update(dt);
    st.angle.SetTarget(value);
    st.angle.Update(dt);
    float v = std::clamp(st.angle.value, 0.0f, 1.0f);
    float start = 2.3561945f, sweep = 4.712389f;
    ui.dl->PathArcTo(ImVec2(cx, cy), radius, start, start + sweep, 64);
    ui.dl->PathStroke(ui.theme.dark ? IM_COL32(255,255,255,38) : IM_COL32(0,0,0,28), 0, 7.0f);
    ui.dl->PathArcTo(ImVec2(cx, cy), radius, start, start + sweep * v, 64);
    ui.dl->PathStroke(ui.theme.accent.U32(), 0, 7.0f);
    ui.CircleShadow(cx, cy, radius * 0.72f, 0.25f);
    ui.dl->AddCircleFilled(ImVec2(cx, cy), radius * 0.70f, IM_COL32(250, 250, 252, 255), 64);
    ui.dl->AddCircleFilled(ImVec2(cx, cy - radius * 0.15f), radius * 0.60f, IM_COL32(255, 255, 255, 255), 64);
    ui.dl->AddCircle(ImVec2(cx, cy), radius * 0.70f - 0.5f, IM_COL32(0, 0, 0, 28), 64, 1.0f);
    float a = start + sweep * v;
    float ix = cx + std::cos(a) * radius * 0.34f;
    float iy = cy + std::sin(a) * radius * 0.34f;
    float ox = cx + std::cos(a) * radius * 0.60f;
    float oy = cy + std::sin(a) * radius * 0.60f;
    ui.Line(ix, iy, ox, oy, Rgba(0.38f, 0.38f, 0.42f, 1), 2.5f);
    char buf[16]; snprintf(buf, sizeof(buf), "%d", (int)std::lround(v * 100));
    ui.TextCentered(ui.fonts.semibold22, 22.0f, Rect(cx - 40, cy + 10, 80, 26), buf, ui.theme.text);
    ui.TextCentered(ui.fonts.regular11, 11.0f, Rect(cx - 40, cy + 34, 80, 16), "Volume", ui.theme.secondary);
}

// minimal monochrome symbols
enum class Icon { Grid, Sliders, Brush, Wave, Info, Star };
inline void DrawIcon(UI& ui, Icon icon, Rect r, const Rgba& c) {
    float x = r.x, y = r.y, w = r.w, h = r.h;
    ImU32 col = c.U32();
    float t = 1.5f;
    switch (icon) {
        case Icon::Grid:
            for (int i = 0; i < 4; ++i) {
                float ox = x + (i % 2) * (w * 0.52f), oy = y + (i / 2) * (h * 0.52f);
                ui.dl->AddRect(ImVec2(ox, oy), ImVec2(ox + w * 0.40f, oy + h * 0.40f), col, 2.0f, 0, t);
            }
            break;
        case Icon::Sliders:
            ui.Line(x, y + h * 0.25f, x + w, y + h * 0.25f, c, t);
            ui.Line(x, y + h * 0.75f, x + w, y + h * 0.75f, c, t);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.35f, y + h * 0.25f), 2.5f, col);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.65f, y + h * 0.75f), 2.5f, col);
            break;
        case Icon::Brush:
            ui.dl->AddCircle(ImVec2(x + w * 0.45f, y + h * 0.55f), w * 0.32f, col, 24, t);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.30f, y + h * 0.34f), 2.0f, col);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.62f, y + h * 0.34f), 2.0f, col);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.30f, y + h * 0.66f), 2.0f, col);
            break;
        case Icon::Wave:
            for (int i = 0; i < 5; ++i) {
                float bx = x + w * (0.12f + i * 0.19f);
                float bh = h * (0.25f + 0.55f * std::fabs(std::sin(i * 1.7f)));
                ui.dl->AddLine(ImVec2(bx, y + h * 0.5f - bh * 0.5f), ImVec2(bx, y + h * 0.5f + bh * 0.5f), col, 1.8f);
            }
            break;
        case Icon::Info:
            ui.dl->AddCircle(ImVec2(x + w * 0.5f, y + h * 0.5f), w * 0.42f, col, 24, t);
            ui.dl->AddCircleFilled(ImVec2(x + w * 0.5f, y + h * 0.30f), 1.4f, col);
            ui.Line(x + w * 0.5f, y + h * 0.44f, x + w * 0.5f, y + h * 0.70f, c, t);
            break;
        case Icon::Star:
            break;
    }
}

} // namespace sw

#include "Symbols.h"
