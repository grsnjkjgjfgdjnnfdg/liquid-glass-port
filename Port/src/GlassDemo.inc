// ---------------------------------------------------------------------------
// Liquid-glass page, ported from the real iOS 27 capture (frames/f120.jpg).
//
// Layout measured this round from the capture itself, by building the backdrop
// model below and taking the residual mask: the panels are 355 x 75 at x = 23.3
// with tops at 148.3 / 244.7 / 339.7 / 436.0 / 532.0 / 632.7, corner radius 26.5
// (the top row of each panel is 53 pt narrower than its middle, so 2r = 53).
// The previous transcription had them 52 pt tall starting at 115 â€” 33 pt too
// high and 23 pt too short, which alone accounted for most of the panel error.
//
// Fills are the measured lift of each panel over the backdrop it sits on:
//   a = mean over channels of (interior - backdrop) / (255 - backdrop)
//   regular 0.583 Â· clear 0.184 Â· tinted (blue, 0.90) Â· capsule 0.627
//   panel 5      0.627 Â· panel 6        0.239
// ---------------------------------------------------------------------------
struct IosGlassState {
    ButtonState btn;
};

namespace glassdemo {

// Measured backdrop: a diagonal ramp. Sampling the left and right edges of
// f120.jpg at 55 pt intervals shows red climbing ~36 and green up to ~50 from
// x=6 to x=397, and the mid-line sample at y=100 lands on the mean of the two
// (59 vs 59.5), so the horizontal component is linear while the vertical one is
// piecewise linear. Both sample columns are outside the panels (x 20..382).
// The backdrop is a three-colour `LinearGradient` from `.topLeading` to
// `.bottomTrailing`, so a point's position along its axis is
//     t = (x * W + y * H) / (W * W + H * H)
// and the colour is a function of t alone. This ramp is *measured* from the PNG capture
// by binning the page by t and taking the median (Tools/_backdrop_fit.py); it is not the
// sRGB interpolation of the three declared stops, which is off by 5.7/255 on average and
// by up to 24 units in blue, because SwiftUI interpolates its gradient in a perceptual
// space. The previous table here was 17 rows of left/right colours sampled off the JPEG
// video frame, which cost 9.1 mean error over the whole page.
struct RampStop {
    float t;
    unsigned char r, g, b;
};

const RampStop kRamp[] = {
    { 0.0266f, 23, 27, 91 },
    { 0.0599f, 32, 29, 93 },
    { 0.0975f, 42, 31, 95 },
    { 0.1354f, 52, 32, 97 },
    { 0.1727f, 61, 33, 99 },
    { 0.2110f, 70, 34, 101 },
    { 0.2486f, 78, 35, 102 },
    { 0.2861f, 88, 35, 104 },
    { 0.3278f, 98, 34, 106 },
    { 0.3654f, 107, 33, 108 },
    { 0.4046f, 117, 32, 110 },
    { 0.4423f, 125, 30, 112 },
    { 0.4801f, 135, 27, 114 },
    { 0.5194f, 143, 34, 113 },
    { 0.5570f, 151, 45, 111 },
    { 0.5974f, 159, 56, 109 },
    { 0.6346f, 167, 65, 106 },
    { 0.6732f, 174, 74, 103 },
    { 0.7132f, 183, 83, 99 },
    { 0.7503f, 190, 90, 95 },
    { 0.7895f, 198, 98, 90 },
    { 0.8271f, 206, 106, 83 },
    { 0.8644f, 214, 113, 76 },
    { 0.9025f, 222, 121, 67 },
    { 0.9400f, 230, 129, 56 },
    { 0.9725f, 236, 135, 43 },
};

inline void RampColour(float t, int out[3]) {
    const int n = (int)(sizeof(kRamp) / sizeof(kRamp[0]));
    if (t <= kRamp[0].t) {
        out[0] = kRamp[0].r; out[1] = kRamp[0].g; out[2] = kRamp[0].b;
        return;
    }
    if (t >= kRamp[n - 1].t) {
        out[0] = kRamp[n - 1].r; out[1] = kRamp[n - 1].g; out[2] = kRamp[n - 1].b;
        return;
    }
    int i = 0;
    while (i + 2 < n && kRamp[i + 1].t < t) ++i;
    const float span = kRamp[i + 1].t - kRamp[i].t;
    const float f = span > 0.0f ? (t - kRamp[i].t) / span : 0.0f;
    out[0] = (int)(kRamp[i].r + (kRamp[i + 1].r - kRamp[i].r) * f + 0.5f);
    out[1] = (int)(kRamp[i].g + (kRamp[i + 1].g - kRamp[i].g) * f + 0.5f);
    out[2] = (int)(kRamp[i].b + (kRamp[i + 1].b - kRamp[i].b) * f + 0.5f);
}

inline void Backdrop(UI& ui) {
    constexpr float kRowH = 2.0f;
    constexpr int kSegs = 4;
    constexpr float kDen = (float)kIosW * kIosW + (float)kIosH * kIosH;
    for (float y = 0.0f; y < kIosH; y += kRowH) {
        for (int s = 0; s < kSegs; ++s) {
            const float x0 = (float)kIosW * (float)s / (float)kSegs;
            const float x1 = (float)kIosW * (float)(s + 1) / (float)kSegs;
            int ul[3], ur[3], br[3], bl[3];
            RampColour((x0 * kIosW + y * kIosH) / kDen, ul);
            RampColour((x1 * kIosW + y * kIosH) / kDen, ur);
            RampColour((x1 * kIosW + (y + kRowH) * kIosH) / kDen, br);
            RampColour((x0 * kIosW + (y + kRowH) * kIosH) / kDen, bl);
            ui.dl->AddRectFilledMultiColor(
                ImVec2(x0, y), ImVec2(x1, y + kRowH),
                IM_COL32(ul[0], ul[1], ul[2], 255), IM_COL32(ur[0], ur[1], ur[2], 255),
                IM_COL32(br[0], br[1], br[2], 255), IM_COL32(bl[0], bl[1], bl[2], 255));
        }
    }
}

struct PanelSpec {
    float top;
    float h;
    float radius;     // 26 for the rounded rectangles, 37.5 for the Capsule()
    const char* label;
    Rgba lift;        // measured per-channel lift: out = base + lift * (1 - base)
    Rgba liftDark;
    bool liftMode;
    float edgeStrength;   // 0 for the tinted panel, whose edge is a soft ramp
};

// Both sets measured from the PNG captures in Reference/iOS27Glass by
// Tools/_glass_lifts.py: the page's gradient is linear in y and the 24 pt gaps between
// panels show it, so the backdrop under each panel is known and `out = base + lift *
// (1 - base)` gives the coefficients directly.
//
// The dark material is *not* the light one: it lifts far less (a .regular panel lifts
// 0.63/0.51/0.71 in light and only 0.22/0.14/0.28 in dark), which is the adaptation
// the audit listed as missing. `.clear` is identical in both - it is the same sharp,
// low-tint material either way.
constexpr float kRectR = 26.0f;    // RoundedRectangle(cornerRadius: 26)
constexpr float kCapsuleR = 37.5f; // Capsule(), i.e. half of the 75 pt height

const PanelSpec kPanels[] = {
    { 148.3f, 75.0f, kRectR, "Regular",
      Rgba(0.613f, 0.493f, 0.689f), Rgba(0.191f, 0.109f, 0.249f), true, 1.0f },
    { 244.7f, 75.0f, kRectR, "Clear",
      Rgba(0.219f, 0.128f, 0.227f), Rgba(0.219f, 0.128f, 0.227f), true, 1.0f },
    { 339.7f, 75.0f, kRectR, "Tinted",   // a blue tint, in the same lift form
      Rgba(-0.969f, 0.369f, 0.740f), Rgba(-0.900f, 0.524f, 0.986f), true, 0.0f },
    { 436.0f, 75.0f, kCapsuleR, nullptr,
      Rgba(0.862f, 0.461f, 0.686f), Rgba(0.333f, 0.081f, 0.233f), true, 1.0f },
    { 532.0f, 75.0f, kRectR, nullptr,
      Rgba(0.940f, 0.474f, 0.599f), Rgba(0.308f, 0.075f, 0.137f), true, 1.0f },
    { 632.7f, 75.0f, kRectR, nullptr,
      Rgba(0.448f, 0.154f, 0.175f), Rgba(0.451f, 0.154f, 0.175f), true, 1.0f },
};

constexpr float kPanelX = 23.3f;
constexpr float kPanelW = 355.0f;
constexpr float kPanelR = 26.5f;

} // namespace glassdemo

inline void IosGlassDemo(UI& ui, IosGlassState& st) {
    (void)st;
    glassdemo::Backdrop(ui);
    // The status bar is drawn last, over the gradient with white ink. The old
    // reference was a device frame whose bar sat above a black island, and drawing
    // only the bar measured worse than drawing nothing; the PNG captures in
    // Reference/iOS27Glass are plain screenshots, and their bar is a normal one.

    // Title ink measured at (26, 93.3) 189.3 x 30.7 in the capture; the port's
    // ink landed at (28.3, 93.0), so the draw origin moves 2.3 pt left.
    ui.TextBold(ui.fonts.semibold34, 34.0f, 23.7f, 85.8f, "Liquid Glass",
                Rgba(1, 1, 1, 0.97f));
    // Ink follows the *appearance*, not the backdrop: the capture keeps the status bar
    // dark on this page even though the page itself is dark (a view controller has to
    // opt in to light content, and this one does not). `lightContent=false` still
    // switches to white in dark appearance, which is what the dark capture shows.
    IosStatusBar(ui);

    for (const glassdemo::PanelSpec& p : glassdemo::kPanels) {
        Rect r(glassdemo::kPanelX, p.top, glassdemo::kPanelW, p.h);
        if (p.liftMode) {
            // Measured per-channel lift: the form the real material takes. The
            // panels' cast shadow was measured too, and the shader's version of it
            // overshoots (panels 5.44 -> 6.51), so it stays off until the shadow
            // profile itself is measured instead of approximated.
            const Rgba& lift = ui.theme.dark ? p.liftDark : p.lift;
            ui.Glass(r, p.radius, Rgba(1, 1, 1, 1), 1.0f, false, nullptr, &lift,
                 1.0f, false, 1.0f, 1.0f, 0.0f, 1.90f, 0.0f, p.edgeStrength, 0.48f);
        } else {
            ui.Glass(r, p.radius, p.lift, p.lift.a, false);
        }
    }

    // Labels: ink tops measured at panel top + 31.3 (the cap top; "Regular" measures
    // 15.3 tall because of its descender and "Clear" 12.7), centred in the panel.
    constexpr float kLabelDy = 31.3f - 3.0f;   // the 17 pt face's ascent inside the box
    const Rgba labelInk(1, 1, 1, 0.97f);
    ui.TextCentered(ui.fonts.regular17, 17.0f,
                    Rect(glassdemo::kPanelX, 148.3f + kLabelDy, glassdemo::kPanelW, 20),
                    "Regular", labelInk);
    ui.TextCentered(ui.fonts.regular17, 17.0f,
                    Rect(glassdemo::kPanelX, 244.7f + kLabelDy, glassdemo::kPanelW, 20),
                    "Clear", labelInk);
    ui.TextCentered(ui.fonts.regular17, 17.0f,
                    Rect(glassdemo::kPanelX, 339.7f + kLabelDy, glassdemo::kPanelW, 20),
                    "Tinted", labelInk);

    // The capsule row is an HStack of the glyph and the label, and the *group* is
    // centred: glyph ink (155.3, 465.3) 18.7 x 17.7, label ink from x 185.3.
    if (ui.icons && ui.icons->Ready()) {
        DrawSymbol(*ui.icons, SfIcon::Airplane, ui.dl,
                   Rect(155.3f, 465.3f, 18.7f, 17.7f), Rgba(1, 1, 1, 0.95f), 1.0f);
    }
    ui.Text(ui.fonts.regular17, 17.0f, 185.3f, 462.0f, "Capsule", labelInk);
}
