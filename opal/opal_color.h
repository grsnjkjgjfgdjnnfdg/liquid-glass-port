#pragma once
#include "../imgui/imgui.h"
#include <algorithm>

namespace opal::accent {

struct AccentStop { unsigned char r, g, b; };

inline AccentStop kTabAccent[5][3] = {
    { {208,158,32},  {220,170,38},  {180,135,25} }, // Tab 0 (Combat): Deep Imperial Gold
    { {220,170,38},  {232,185,48},  {195,148,30} }, // Tab 1 (Humanoids): Rich Warm Amber Gold
    { {232,185,48},  {244,200,60},  {205,160,38} }, // Tab 2 (World): Vivid Radiant Gold
    { {244,200,60},  {250,215,78},  {218,175,48} }, // Tab 3 (Loot): Golden Amber Glow
    { {250,215,78},  {255,230,105}, {225,188,58} }, // Tab 4 (Settings): Radiant Sunburst Gold
};

inline AccentStop kDefaultAccent[3] = {
    { 220, 170, 38 }, { 232, 185, 48 }, { 195, 148, 30 },
};

inline int  g_activeTab  = -1;
inline float g_tabBlend  = 1.0f;

inline void SetActiveTab(int tab) { g_activeTab = tab; g_tabBlend = 1.0f; }
inline int  GetActiveTab()        { return g_activeTab; }

inline AccentStop Resolve(int variant) {
    variant = std::clamp(variant, 0, 2);
    if (g_activeTab < 0 || g_activeTab >= 5)
        return kDefaultAccent[variant];
    const auto& a = kTabAccent[g_activeTab][variant];
    const auto& d = kDefaultAccent[variant];
    float t = g_tabBlend;
    return {
        (unsigned char)((float)a.r * t + (float)d.r * (1.0f - t) + 0.5f),
        (unsigned char)((float)a.g * t + (float)d.g * (1.0f - t) + 0.5f),
        (unsigned char)((float)a.b * t + (float)d.b * (1.0f - t) + 0.5f),
    };
}

inline ImU32 Primary(float a = 255.0f) { auto c = Resolve(0); return IM_COL32(c.r, c.g, c.b, (int)a); }
inline ImU32 Hover  (float a = 255.0f) { auto c = Resolve(1); return IM_COL32(c.r, c.g, c.b, (int)a); }
inline ImU32 Pressed(float a = 255.0f) { auto c = Resolve(2); return IM_COL32(c.r, c.g, c.b, (int)a); }

inline ImU32 Dim(float a = 255.0f) { auto c = Resolve(0); return IM_COL32(c.r, c.g, c.b, (int)(a * 0.35f)); }

struct RGB { float r, g, b; };
inline RGB PrimaryRGB() { auto c = Resolve(0); return { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f }; }
inline RGB HoverRGB()   { auto c = Resolve(1); return { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f }; }
inline RGB PressedRGB() { auto c = Resolve(2); return { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f }; }

} // namespace opal::accent
