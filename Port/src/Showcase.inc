    // Desktop showcase for the ported components.
    //
    // This is not a port of an Apple screen (there is no macOS 27 hardware here to
    // capture one from), so it is deliberately a *specimen*: the measured iOS 27
    // controls and the liquid-glass material, presented on a window-sized canvas.
    // The iOS pages are the actual port; this page is how the same code looks on a
    // desktop.
    ImVec2 mp = ui.mouse;

    // Backdrop: the procedural macOS-27-style ribbon wallpaper, drawn into the
    // scene so the glass has something real to refract.
    if (!ui.suppressGlass) {
        ui.dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ui.screenW, ui.screenH),
                                       IM_COL32(18, 24, 46, 255), IM_COL32(48, 26, 78, 255),
                                       IM_COL32(120, 60, 60, 255), IM_COL32(24, 40, 70, 255));
        for (int i = 0; i < 5; ++i) {
            float r = 260.0f + i * 120.0f;
            ui.dl->AddCircleFilled(ImVec2(220.0f + i * 260.0f, 260.0f + (i % 3) * 210.0f), r,
                                   IM_COL32(255, 255, 255, 10), 64);
        }
        for (int i = 0; i < 60; ++i) {
            float y = 40.0f + i * 15.0f;
            float len = 120.0f + (i * 53 % 460);
            ui.dl->AddRectFilled(ImVec2(60, y), ImVec2(60 + len, y + 6),
                                 IM_COL32(255, 255, 255, 16), 3.0f);
        }
    }

    const float cx = ui.screenW * 0.5f;
    Rect card(cx - 430, 120, 860, 560);
    ui.Glass(card, 32.0f, Rgba(1, 1, 1, 0.14f), 1.0f, true);
    ui.TextBold(ui.fonts.semibold34, 34.0f, card.x + 44, card.y + 40,
                "SwiftUI, ported", Rgba(1, 1, 1, 0.97f));
    ui.Text(ui.fonts.regular15, 15.0f, card.x + 44, card.y + 86,
            "Controls and materials measured from the iOS 27 runtime, running on Direct3D 11.",
            Rgba(1, 1, 1, 0.72f));

    // Two glass sub-panels: one regular, one clear, over the same backdrop.
    Rect a(card.x + 44, card.y + 132, 360, 150);
    ui.Glass(a, 22.0f, Rgba(1, 1, 1, 0.18f), 1.0f, true);
    ui.Text(ui.fonts.semibold15, 15.0f, a.x + 20, a.y + 16, "Regular material",
            Rgba(1, 1, 1, 0.95f));
    ui.Text(ui.fonts.regular12, 12.0f, a.x + 20, a.y + 38, "blur, refraction, rim light",
            Rgba(1, 1, 1, 0.60f));
    {
        static bool mmOn = true;
        static SwitchState mmSw;
        DrawSwitch(ui, Rect(a.x + 20, a.y + 70, 63, 28), mmOn, mmSw, ui.dt);
        static float mmVal = 0.55f;
        static SliderState mmSl;
        DrawSlider(ui, Rect(a.x + 100, a.y + 70, 240, 31), mmVal, mmSl, ui.dt);
        static int mmSeg = 0;
        static SegState mmSegS;
        static const char* mmLabels[] = { "One", "Two", "Three" };
        DrawSegmented(ui, Rect(a.x + 20, a.y + 110, 320, 31), mmSeg, 3, mmSegS, mmLabels, ui.dt);
    }

    Rect b(card.x + 428, card.y + 132, 388, 150);
    ui.Glass(b, 22.0f, Rgba(1, 1, 1, 0.05f), 1.0f, true);
    ui.Text(ui.fonts.semibold15, 15.0f, b.x + 20, b.y + 16, "Clear material",
            Rgba(1, 1, 1, 0.95f));
    ui.Text(ui.fonts.regular12, 12.0f, b.x + 20, b.y + 38, "same blur, almost no tint",
            Rgba(1, 1, 1, 0.60f));
    {
        static ButtonState p1, p2, p3;
        DrawButton(ui, Rect(b.x + 20, b.y + 74, 104, 34.3f), "Continue",
                   ButtonKind::Prominent, p1, ui.dt);
        DrawButton(ui, Rect(b.x + 136, b.y + 74, 88, 34.3f), "Cancel",
                   ButtonKind::Bordered, p2, ui.dt);
        DrawButton(ui, Rect(b.x + 236, b.y + 81, 112, 20.3f), "Learn More",
                   ButtonKind::Plain, p3, ui.dt);
    }

    // Rows: the Settings row rhythm, on glass.
    Rect rows(card.x + 44, card.y + 302, 772, 208);
    ui.Glass(rows, 22.0f, Rgba(1, 1, 1, 0.10f), 1.0f, true);
    static const char* rowLabels[] = { "General", "Appearance", "Accessibility", "Motion" };
    static const char* rowSubs[] = { "52 pt rows", "Light, dark, automatic",
                                     "SF Symbols", "spring(response:dampingFraction:)" };
    for (int i = 0; i < 4; ++i) {
        float ry = rows.y + i * 52.0f;
        if (i > 0) {
            ui.dl->AddLine(ImVec2(rows.x + 56, ry + 0.5f), ImVec2(rows.x + rows.w - 20, ry + 0.5f),
                           IM_COL32(255, 255, 255, 28), 1.0f);
        }
        ui.TextVCenter(ui.fonts.regular17, 17.0f, rows.x + 20, Rect(rows.x, ry, 300, 52),
                       rowLabels[i], Rgba(1, 1, 1, 0.95f));
        ui.TextVCenter(ui.fonts.regular12, 12.0f, rows.x + 200, Rect(rows.x, ry, 460, 52),
                       rowSubs[i], Rgba(1, 1, 1, 0.52f));
        IosChevron(ui, rows.x + rows.w - 34, ry + 20.0f, 12.0f);
    }

    ui.Text(ui.fonts.regular11, 11.0f, card.x + 44, card.y + 524,
            "Every dimension on this page comes from PORT_SPEC.md: capsule 63x28, thumb 36.3x24, "
            "track 6, rows 52.", Rgba(1, 1, 1, 0.42f));

    (void)mp;
