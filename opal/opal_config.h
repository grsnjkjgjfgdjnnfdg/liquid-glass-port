#pragma once

#ifndef OPAL_QUALITY_LEVEL
#define OPAL_QUALITY_LEVEL 2
#endif

namespace opal {

namespace LayoutConstants {
    constexpr float SidebarWidth    = 200.0f;
    constexpr float ContentPadding  = 16.0f;
    constexpr float GroupPadding    = 12.0f;
    constexpr float GroupRadius     = 14.0f;
    constexpr float GroupGap        = 10.0f;
    constexpr float MinColumnWidth  = 250.0f;
    constexpr float SubTabHeight    = 32.0f;
    constexpr float ToggleHeight    = 28.0f;
    constexpr float SliderHeight    = 36.0f;
    constexpr float FontSize        = 15.0f;
    constexpr float GlassOpacity    = 0.35f;
    constexpr float BlurSigma       = 4.0f;
    constexpr float TabBarHeight    = 36.0f;
    constexpr float PanelRadius     = 14.0f;
    constexpr float CardRadius      = 16.0f;
    constexpr float ButtonRadius    = 8.0f;
    constexpr float ItemSpacing     = 8.0f;
}

namespace Quality {
    inline constexpr int  GaussianTaps()        { return OPAL_QUALITY_LEVEL >= 2 ? 5 : 3; }
    inline constexpr bool ChromaticAberration() { return OPAL_QUALITY_LEVEL >= 2; }
    inline constexpr bool GradientAnimation()   { return OPAL_QUALITY_LEVEL >= 1; }
    inline constexpr bool FullBlur()            { return OPAL_QUALITY_LEVEL >= 1; }
}

} // namespace opal
