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
    ui.Glass(Rect(25, 137, 348, 220), 40.0f, Rgba(1, 1, 1, 0.50f), 1.0f, true);
    // Clear material: the grid must stay visible and bend near the rim.
    ui.Glass(Rect(25, 337, 348, 170), 40.0f, Rgba(1, 1, 1, 0.06f), 1.0f, true);
}
