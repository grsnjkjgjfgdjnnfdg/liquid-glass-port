#pragma once
#include "theme.h"

namespace opal {

inline Theme Theme::Liquid() {
    Theme t;
    t.name        = "Liquid";
    t.bg          = Color::FromBytes(11, 11, 14);
    t.surface     = Color::FromBytes(18, 18, 22);
    t.surfaceAlt  = Color::FromBytes(24, 24, 29);
    t.border      = Color(1, 1, 1, 0.08f);
    t.borderBright = Color(1, 1, 1, 0.15f);
    t.text        = Color::FromBytes(245, 245, 248);
    t.textMuted   = Color::FromBytes(180, 180, 195);
    t.textDim     = Color::FromBytes(140, 140, 155);
    t.accent      = Color::FromBytes(99, 102, 241);
    t.accentDim   = Color::FromBytes(79, 70, 229);
    t.accentAlt   = Color::FromBytes(124, 127, 248);
    t.glassOpacity  = 0.35f;
    t.blurIntensity = 6.0f;
    t.fresnelPower  = 0.12f;
    return t;
}

inline Theme Theme::LiquidLight() {
    Theme t;
    t.name        = "Liquid Light";
    t.bg          = Color::FromBytes(240, 240, 244);
    t.surface     = Color::FromBytes(255, 255, 255);
    t.surfaceAlt  = Color::FromBytes(235, 235, 240);
    t.border      = Color(0, 0, 0, 0.08f);
    t.borderBright = Color(0, 0, 0, 0.15f);
    t.text        = Color::FromBytes(20, 20, 25);
    t.textMuted   = Color::FromBytes(80, 80, 90);
    t.textDim     = Color::FromBytes(130, 130, 140);
    t.accent      = Color::FromBytes(99, 102, 241);
    t.accentDim   = Color::FromBytes(79, 70, 229);
    t.accentAlt   = Color::FromBytes(124, 127, 248);
    t.glassOpacity  = 0.25f;
    t.blurIntensity = 4.0f;
    t.fresnelPower  = 0.10f;
    return t;
}

inline Theme Theme::Minimal() {
    Theme t;
    t.name        = "Minimal";
    t.bg          = Color::FromBytes(8, 8, 10);
    t.surface     = Color::FromBytes(14, 14, 18);
    t.surfaceAlt  = Color::FromBytes(20, 20, 25);
    t.border      = Color(1, 1, 1, 0.05f);
    t.borderBright = Color(1, 1, 1, 0.10f);
    t.text        = Color::FromBytes(235, 235, 240);
    t.textMuted   = Color::FromBytes(160, 160, 170);
    t.textDim     = Color::FromBytes(110, 110, 120);
    t.accent      = Color::FromBytes(99, 102, 241);
    t.accentDim   = Color::FromBytes(79, 70, 229);
    t.accentAlt   = Color::FromBytes(124, 127, 248);
    t.glassOpacity  = 0.25f;
    t.blurIntensity = 6.0f;
    t.fresnelPower  = 0.08f;
    return t;
}

inline Theme Theme::NoirGlass() {
    Theme t;
    t.name        = "Noir Glass";
    t.bg          = Color::FromBytes(10, 10, 16);
    t.surface     = Color::FromBytes(18, 18, 28);
    t.surfaceAlt  = Color::FromBytes(25, 25, 38);
    t.border      = Color(1, 1, 1, 0.06f);
    t.borderBright = Color(1, 1, 1, 0.12f);
    t.text        = Color::FromBytes(240, 240, 248);
    t.textMuted   = Color::FromBytes(160, 160, 180);
    t.textDim     = Color::FromBytes(100, 100, 115);
    t.accent      = Color::FromBytes(99, 102, 241);
    t.accentDim   = Color::FromBytes(79, 70, 229);
    t.accentAlt   = Color::FromBytes(124, 127, 248);
    t.glassOpacity  = 0.76f;
    t.blurIntensity = 4.5f;
    t.fresnelPower  = 0.18f;
    t.panelRadius   = 16.0f;
    t.groupRadius   = 16.0f;
    t.cardRadius    = 18.0f;
    t.buttonRadius  = 10.0f;
    return t;
}

} // namespace opal
