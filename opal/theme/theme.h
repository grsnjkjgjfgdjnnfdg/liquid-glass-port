#pragma once
#include "../opal_types.h"

struct ImFont;

namespace opal {

struct Theme {
    const char* name = "Liquid";

    Color bg;
    Color surface;
    Color surfaceAlt;
    Color border;
    Color borderBright;

    Color text;
    Color textMuted;
    Color textDim;

    Color accent;
    Color accentDim;
    Color accentAlt;

    float panelPadding  = 16.0f;
    float groupPadding  = 12.0f;
    float itemSpacing   = 8.0f;

    float panelRadius   = 14.0f;
    float groupRadius   = 14.0f;
    float cardRadius    = 16.0f;
    float buttonRadius  = 8.0f;

    float glassOpacity  = 0.35f;
    float blurIntensity = 4.0f;
    float fresnelPower  = 0.12f;

    ImFont* body      = nullptr;
    ImFont* bodySmall = nullptr;
    ImFont* heading   = nullptr;
    ImFont* icon      = nullptr;

    static Theme Liquid();
    static Theme LiquidLight();
    static Theme Minimal();
    static Theme NoirGlass();
};

} // namespace opal
