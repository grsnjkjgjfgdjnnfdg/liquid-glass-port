#pragma once
#include "anim_core.h"
#include "../opal_types.h"
#include "../utils/opal_math.h"
#include <cmath>
#include <algorithm>
#include "../utils/opal_debug_log.h"

namespace opal::anim {
using namespace opal::debug;

using opal::Clamp;
using opal::utils::EaseOutQuart;
using opal::utils::EaseInOutCubic;

struct PageTransition {
    float progress = 0.0f;
    float elapsed  = 0.0f;
    float duration = 0.4f;
    bool  active   = false;

    void Start(float dur = 0.4f) { elapsed = 0.0f; progress = 0.0f; active = true; duration = dur; }
    void Update(float dt) {
        if (!active) return;
        elapsed += dt;
        progress = Clamp(elapsed / duration, 0.0f, 1.0f);
        if (progress >= 1.0f) active = false;
    }
    bool IsDone() const { return progress >= 1.0f; }
};

struct GlassBloomState {
private:
    // Integrity cookie — must match kMagic. If corrupted, the struct was
    // zeroed by an external write (memory corruption). s_lastKnownOpen is
    // the recovery fallback.
    static constexpr uint32_t kMagic = 0x424C4F4D;  // 'BLOM'
    uint32_t m_cookie = kMagic;
    bool  isOpen       = false;
    bool  m_authorizedOpen = false;
    static bool s_lastKnownOpen;
public:
    float elapsed      = 0.0f;
    Vec2  bloomOrigin;
    bool  reducedMotion = false;

    enum State { Idle, Opening, Closing } state = Idle;

    bool IsCookieValid() const { return m_cookie == kMagic; }
    bool IsOpen() const {
        if (!IsCookieValid()) {
            debug::Log("BL-Corrupt", "cookie=0x%08X expected=0x%08X -> recovery=%d",
                       m_cookie, kMagic, (int)s_lastKnownOpen);
            return s_lastKnownOpen;
        }
        return m_authorizedOpen;
    }

    static constexpr float kOpenDur  = 0.380f;
    static constexpr float kCloseDur = 0.220f;
    static constexpr float kStagger  = 0.015f;

    void Open(const Vec2& origin = {}) {
        int prev = (int)state;
        m_authorizedOpen = true;
        s_lastKnownOpen = true;
        if (state == Opening) { debug::Log("BL-Open", "skip(opening) prev=%d", prev); return; }
        if (state == Closing) { float p = elapsed / kCloseDur; elapsed = (1.0f - p) * kOpenDur; }
        else elapsed = 0.0f;
        isOpen = true; state = Opening; bloomOrigin = origin;
        debug::Log("BL-Open", "prev=%d isOpen=%d state=Opening", prev, (int)isOpen);
    }
    void Close() {
        int prev = (int)state;
        m_authorizedOpen = false;
        s_lastKnownOpen = false;
        if (state == Closing) { debug::Log("BL-Close", "skip(closing) prev=%d", prev); return; }
        if (state == Opening) { float p = elapsed / kOpenDur; elapsed = (1.0f - p) * kCloseDur; }
        else elapsed = 0.0f;
        state = Closing;
        debug::Log("BL-Close", "prev=%d isOpen=%d state=Closing", prev, (int)isOpen);
    }
    void Toggle(const Vec2& origin = {}) {
        if (state == Opening) { Close(); debug::Log("BL-Toggle", "opening->Close"); return; }
        else if (state == Closing) { Open(origin); debug::Log("BL-Toggle", "closing->Open"); return; }
        else if (m_authorizedOpen) { Close(); debug::Log("BL-Toggle", "open->Close"); return; }
        else { Open(origin); debug::Log("BL-Toggle", "closed->Open"); return; }
    }

    void Update(float dt) {
        if (state == Idle) return;
        elapsed += dt;
        float dur = (state == Opening) ? kOpenDur : kCloseDur;
        if (elapsed >= dur) {
            elapsed = dur;
            bool wasOpen = isOpen;
            int  prevState = (int)state;
            if (state == Opening) isOpen = true; else isOpen = false;
            state = Idle;
            debug::Log("BL-Update", "transition state=%d->Idle isOpen=%d->%d",
                       prevState, (int)wasOpen, (int)isOpen);
        }
    }

    float Progress() const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        float dur = (state == Opening) ? kOpenDur : kCloseDur;
        return Clamp(elapsed / dur, 0.0f, 1.0f);
    }

    float BlurProgress() const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Opening) {
            float p = Clamp(elapsed / 0.180f, 0.0f, 1.0f);
            return EaseOutQuart(p);
        } else {
            float p = Clamp((elapsed - 0.040f) / 0.160f, 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
    }

    float PanelScale() const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.96f;
        if (state == Opening) {
            float p = Clamp((elapsed - 0.030f) / 0.150f, 0.0f, 1.0f);
            return 0.96f + (1.0f - 0.96f) * EaseOutQuart(p);
        } else {
            float p = Clamp((elapsed - 0.010f) / 0.140f, 0.0f, 1.0f);
            return 1.0f - (1.0f - 0.96f) * EaseOutQuart(p);
        }
    }

    float PanelAlpha() const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Opening) {
            float p = Clamp((elapsed - 0.030f) / 0.120f, 0.0f, 1.0f);
            return EaseOutQuart(p);
        } else {
            float p = Clamp((elapsed - 0.010f) / 0.120f, 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
    }

    // Staged feature reveal for the menu open/close. The chrome (panel,
    // sidebar, group containers) is revealed first by the expanding wave;
    // this value then fades the feature rows IN smoothly on top of the
    // containers. Returns 0..1 (1 = fully visible).
    float ContentFade() const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Opening) {
            float p = Clamp((elapsed - 0.08f) / 0.24f, 0.0f, 1.0f);
            return 1.0f - std::pow(1.0f - p, 3.5f);
        } else {
            float p = Clamp((elapsed - 0.02f) / 0.09f, 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
    }

    // Ordered open reveal. The menu materialises in a fixed sequence rather
    // than by distance from the centre: the container is already scaling and
    // fading in (PanelAlpha), and these stages then bring in, in order,
    //   0 title -> 1 sidebar -> 2 search field -> 3 page content
    // Each stage starts one beat after the previous and resolves quickly, so
    // the whole sequence lands inside the open duration. Hosts pair this with
    // fx::RevealBlur on the same rect, so every stage resolves out of real blur.
    enum Stage { kStageTitle = 0, kStageSidebar, kStageSearch, kStageContent, kStageCount };
    static constexpr float kStageStep = 0.045f;
    static constexpr float kStageDur  = 0.160f;
    float StageReveal(int stage) const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Closing) {
            float p = Clamp(elapsed / (kCloseDur * 0.75f), 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
        float start = 0.020f + (float)stage * kStageStep;
        float p = Clamp((elapsed - start) / kStageDur, 0.0f, 1.0f);
        return EaseOutQuart(p);
    }

    // Expanding Wave Radius from Panel Center
    float WaveRadius(const Rect& panelBounds) const {
        if (state == Idle) return m_authorizedOpen ? 9999.0f : 0.0f;
        float halfW = panelBounds.w * 0.5f, halfH = panelBounds.h * 0.5f;
        float maxR = std::sqrt(halfW * halfW + halfH * halfH) * 1.15f;
        if (state == Opening) {
            float p = Clamp(elapsed / kOpenDur, 0.0f, 1.0f);
            return maxR * (1.0f - std::pow(1.0f - p, 4.5f));
        } else {
            float p = Clamp(elapsed / kCloseDur, 0.0f, 1.0f);
            return maxR * (1.0f - EaseOutQuart(p));
        }
    }

    // Returns [0..1] reveal factor as the top-left to bottom-right Apple motion wave passes the feature's boundary
    float FeatureReveal(const Rect& featureBounds, const Rect& panelBounds) const {
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Closing) {
            float p = Clamp(elapsed / (kCloseDur * 0.75f), 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
        
        // Directional projection coordinate from top-left (origin at panel top-left)
        float totalSpan = panelBounds.w * 0.72f + panelBounds.h * 0.48f;
        if (totalSpan <= 0.0f) totalSpan = 1.0f;

        // Furthest corner of the feature in the top-left to bottom-right sweep
        float fx = (featureBounds.x + featureBounds.w - panelBounds.x);
        float fy = (featureBounds.y + featureBounds.h - panelBounds.y);
        float featureDist = (std::max)(0.0f, fx * 0.72f + fy * 0.48f);

        float p = Clamp(elapsed / kOpenDur, 0.0f, 1.0f);
        // Apple motion deceleration curve: explosive initial jump covering ~60% in first 20%, then viscous deceleration
        float appleEase = 1.0f - std::pow(1.0f - p, 4.5f);
        float currentFront = totalSpan * appleEase * 1.38f;

        float leadIn = 65.0f;
        if (currentFront < featureDist - leadIn) return 0.0f;
        float progress = Clamp((currentFront - (featureDist - leadIn)) / (leadIn + 45.0f), 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - progress, 3.0f);
    }

    float ContentAlpha(int groupIndex, int totalGroups) const {
        (void)totalGroups;
        if (state == Idle) return m_authorizedOpen ? 1.0f : 0.0f;
        if (state == Opening) {
            float groupStart = 0.050f + (float)groupIndex * kStagger;
            float groupEnd   = 0.200f;
            if (groupStart >= groupEnd) groupStart = groupEnd - 0.05f;
            float p = Clamp((elapsed - groupStart) / 0.100f, 0.0f, 1.0f);
            return EaseOutQuart(p);
        } else {
            float p = Clamp(elapsed / 0.100f, 0.0f, 1.0f);
            return 1.0f - EaseOutQuart(p);
        }
    }

    float FrostAlpha() const {
        if (state == Idle) return 0.0f;
        float end = (state == Opening) ? kOpenDur : kCloseDur;
        float p = Clamp(elapsed / end, 0.0f, 1.0f);
        return (1.0f - p) * (state == Opening ? 1.0f : 0.5f);
    }

    // The expanding edge wave is DISABLED.
    //
    // It drew a stack of blue rounded-rect strokes travelling outward from the
    // panel edge (RenderWaveDecorations) and surged the panel's fresnel rim,
    // lens thickness and refraction along with it. On screen that reads as a
    // blurred glow bleeding all around the menu for the whole open/close, which
    // is not what we want there. The panel already scales and fades, and the
    // staged reveals (StageReveal + fx::RevealBlur) carry the motion.
    //
    // Kept as neutral values / no-ops so hosts need no changes.
    float EdgeWaveIntensity() const { return 0.0f; }
    float EdgeWaveRefraction() const { return 0.0f; }
    float EdgeWaveFresnel() const { return 1.0f; }
    float EdgeWaveLens() const { return 0.18f; }

    void RenderWaveDecorations(ImDrawList* dl, const Rect& panelBounds, float alpha) const {
        (void)dl; (void)panelBounds; (void)alpha;
    }

    float BloomRadius(const Vec2& screenSize) const {
        if (state == Idle) return 0.0f;
        float diagonal = std::sqrt(screenSize.x * screenSize.x + screenSize.y * screenSize.y);
        if (state == Opening) {
            float p = Clamp(elapsed / kOpenDur, 0.0f, 1.0f);
            return 30.0f + (diagonal * 1.2f - 30.0f) * EaseOutQuart(p);
        } else {
            float p = Clamp(elapsed / kCloseDur, 0.0f, 1.0f);
            return diagonal * 1.2f * (1.0f - EaseOutQuart(p));
        }
    }
};
inline bool GlassBloomState::s_lastKnownOpen = false;

inline void AnimateSlideIn(Spring& s, float from, float to) {
    s.value  = from;
    s.target = to;
    s.SetVelocity((to - from) * 2.0f);
}

inline void AnimateSlideOut(Spring& s, float from, float to) {
    s.value  = from;
    s.target = to;
}

// ---- Tab transition: group slide + fade out -> fade in ---------------
// Call Start() when the active tab changes. Groups crossfade: the incoming
// content holds hidden for a short beat (the "fade out" of the previous
// tab), then fades in smoothly with a subtle slide/scale settle.
struct TabTransitionState {
    float elapsed = 0.0f;
    bool  active  = false;

    static constexpr float kCharStagger   = 0.016f;
    static constexpr float kGroupStagger  = 0.050f;
    static constexpr float kRowStagger    = 0.020f;

    void Start()          { elapsed = 0.0f; active = true; }
    void Update(float dt) {
        if (active) { elapsed += dt; if (elapsed > 1.5f) active = false; }
    }

    // Whole-page blur for a tab change: blooms the instant the tab switches and
    // clears quickly, so the new page visibly re-focuses. Kept low — this lands
    // on top of the per-row reveals, and both together must still read as
    // "smooth", not "smeared".
    float ContentBlurPx() const {
        if (!active) return 0.0f;
        return 5.0f * (1.0f - EaseOutQuart(Clamp(elapsed / 0.26f, 0.0f, 1.0f)));
    }

    // Deterministic hash → random-ish float in [0,1] for the HTML-demo-like
    // pseudo-random per-element scatter, seeded by index so it's stable
    // every frame (no actual RNG — just a cheap integer hash).
    static float HashF(uint32_t seed) {
        seed = (seed ^ 61u) ^ (seed >> 16u);
        seed *= 9u;
        seed = seed ^ (seed >> 4u);
        seed *= 0x27d4eb2du;
        seed = seed ^ (seed >> 15u);
        return (float)(seed & 0x7FFFFFFFu) / 2147483648.0f;
    }

    // Group-level fade. 0 while hidden (the fade-out beat), then 0 -> 1.
    float GroupFade(int gi) const {
        float local = elapsed - (float)gi * kGroupStagger;
        if (local < 0.0f) return 0.0f;
        float f = Clamp((local - 0.02f) / 0.22f, 0.0f, 1.0f);
        return EaseOutQuart(f);
    }

    // Group-level — unified upward rise with smooth Apple ease-out deceleration
    float GroupProgress(int gi) const {
        float t = (elapsed - (float)gi * kGroupStagger) / 0.36f;
        return Clamp(t, 0.0f, 1.0f);
    }
    float GroupOffsetX(int gi) const {
        (void)gi;
        return 0.0f; // Unified alignment: no alternating left/right shuffle
    }
    float GroupOffsetY(int gi) const {
        return 12.0f * (1.0f - EaseOutQuart(GroupProgress(gi)));
    }
    float GroupScale(int gi) const {
        float p = EaseOutQuart(GroupProgress(gi));
        return 0.98f + p * 0.02f;
    }

    // Row-level. Fade-in: rows settle up into place with Apple ease-out
    float RowProgress(int gi, int ri) const {
        float start = (float)gi * kGroupStagger + 0.03f + (float)ri * (kRowStagger * 0.7f);
        float t = (elapsed - start) / 0.28f;
        return Clamp(t, 0.0f, 1.0f);
    }
    float RowOffsetX(int gi, int ri) const {
        (void)gi; (void)ri;
        return 0.0f;
    }
    float RowOffsetY(int gi, int ri) const {
        return 5.0f * (1.0f - EaseOutQuart(RowProgress(gi, ri)));
    }
    float RowBlurPx(int gi, int ri) const {
        return 4.0f * (1.0f - EaseOutQuart(RowProgress(gi, ri)));
    }
    float RowAlpha(int gi, int ri) const {
        return EaseOutQuart(RowProgress(gi, ri));
    }

    // Character-level shatter (group title).
    float CharProgress(int gi, int ci) const {
        float start = (float)gi * kGroupStagger + 0.03f + (float)ci * kCharStagger;
        float t = (elapsed - start) / 0.48f;
        return Clamp(t, 0.0f, 1.0f);
    }
    float CharOffsetX(int gi, int ci) const {
        float angle = HashF((uint32_t)(ci * 0x9E3779B9u + (uint32_t)gi * 0xAB12C89Fu)) * 6.28318f;
        float dist  = 14.0f * (0.7f + HashF((uint32_t)(ci * 0x12345678u + (uint32_t)gi * 0xFEDCBA98u)) * 0.6f);
        return cosf(angle) * dist * (1.0f - EaseOutQuart(CharProgress(gi, ci)));
    }
    float CharOffsetY(int gi, int ci) const {
        float angle = HashF((uint32_t)(ci * 0x9E3779B9u + (uint32_t)gi * 0xAB12C89Fu)) * 6.28318f;
        float dist  = 14.0f * (0.7f + HashF((uint32_t)(ci * 0x12345678u + (uint32_t)gi * 0xFEDCBA98u)) * 0.6f);
        return sinf(angle) * dist * (1.0f - EaseOutQuart(CharProgress(gi, ci)));
    }
    float CharScale(int gi, int ci) const {
        float p = EaseOutQuart(CharProgress(gi, ci));
        return 0.62f + p * 0.38f;
    }
    float CharBlurPx(int gi, int ci) const {
        return 9.0f * (1.0f - EaseOutQuart(CharProgress(gi, ci)));
    }
    float CharAlpha(int gi, int ci) const {
        return EaseOutQuart(CharProgress(gi, ci));
    }

    // Per-element text entrance: staggered top-to-bottom across the whole page.
    // Internally combines the individual stagger with the sweeping wave band
    // so elements only become visible when the wave front reaches their depth.
    float TextAlpha(int lineIdx) const {
        if (!active) return 1.0f;
        (void)lineIdx;
        float t = Clamp(elapsed / 0.25f, 0.0f, 1.0f);
        return t;  // all text fades together over 250ms
    }
    float TextOffsetY(int lineIdx) const {
        float a = TextAlpha(lineIdx);
        return (1.0f - a) * 2.0f;
    }

    // ---- Wave: a sweeping horizontal band that travels from top to bottom ----
    // WaveProgress() returns 0 (wave just started at top) to 1 (wave finished
    // at bottom).  The whole sweep takes ~420 ms.
    float WaveProgress() const {
        if (!active) return 1.0f;
        // Very slight head start so the band is already visible at the very top
        return Clamp((elapsed + 0.025f) / 0.42f, 0.0f, 1.0f);
    }
    // Fade multiplier for the wave band itself — fades out the glow as the
    // wave finishes (last 20 % of the sweep).
    float WaveGlowAlpha() const {
        if (!active) return 0.0f;
        float wp = WaveProgress();
        if (wp > 1.0f) return 0.0f;
        if (wp > 0.8f) return (1.0f - wp) / 0.2f; // fade out
        if (wp < 0.05f) return wp / 0.05f;         // fade in
        return 1.0f;
    }
    // WaveAlpha: 0 = element hasn't been revealed yet, 1 = wave has passed over
    // it.  normY is the element's vertical position normalised to [0,1] over
    // the content area (0 = top, 1 = bottom).  The wave band is ~12 % of the
    // content height — elements inside it fade in with a blur-to-sharp ramp.
    float WaveAlpha(float normY) const {
        if (!active) return 1.0f;
        float wp = WaveProgress();
        float band = 0.12f;
        float bandTop = wp - band;
        if (normY <= bandTop) return 1.0f;
        if (normY >= wp)      return 0.0f;
        float t = (wp - normY) / band;
        return utils::EaseOutQuart(Clamp(t, 0.0f, 1.0f));
    }
    // WaveBlur: simulated blur strength for elements within the wave band,
    // returning a pseudo-pixel blur value that decays as the wave passes.
    float WaveBlur(float normY) const {
        if (!active) return 0.0f;
        float wa = WaveAlpha(normY);
        if (wa >= 0.99f) return 0.0f;
        // Peak blur ~10px when element first begins to appear, decays to 0
        float t = (1.0f - wa);
        return t * t * 10.0f;  // quadratic shape: 10→0 px
    }
};

} // namespace opal::anim
