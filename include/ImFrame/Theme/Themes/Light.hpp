/**
 * @file     Light.hpp
 * @brief    Clean light colour theme — off-white backgrounds with blue accents
 *
 * Colour values follow Material Design's Grey and Blue palettes for a standard
 * professional light appearance. Accent colours use Material Blue 500/700/900.
 * Status colours use Material Design semantic colours. All fields are populated.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-04
 * @version  0.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Theme/Theme.hpp"

namespace ImFrame::Themes {

/**
 * @brief Light built-in theme
 *
 * Palette: Background #F5F5F5 (Grey 100), accents #1976D2 (Blue 700),
 * text #212121 (Grey 900), status colours from Material Design.
 *
 * @since 0.9.0
 */
inline constexpr Theme::Theme Light{
    // ── Background ────────────────────────────────────────────────────────────
    .backgroundPrimary   = {0.961f, 0.961f, 0.961f, 1.0f}, // #F5F5F5 — Grey 100
    .backgroundSecondary = {0.922f, 0.922f, 0.922f, 1.0f}, // #EBEBEB — Grey 200
    .backgroundTertiary  = {0.878f, 0.878f, 0.878f, 1.0f}, // #E0E0E0 — Grey 300

    // ── Surface ───────────────────────────────────────────────────────────────
    .surfaceDefault = {0.863f, 0.863f, 0.863f, 1.0f}, // #DCDCDC — Grey 350
    .surfaceHover   = {0.816f, 0.816f, 0.816f, 1.0f}, // #D0D0D0 — Grey 400
    .surfaceActive  = {0.769f, 0.769f, 0.769f, 1.0f}, // #C4C4C4 — Grey 450

    // ── Border ────────────────────────────────────────────────────────────────
    .borderDefault = {0.741f, 0.741f, 0.741f, 0.8f}, // #BDBDBD Grey 400 @ 80%
    .borderHover   = {0.620f, 0.620f, 0.620f, 1.0f}, // #9E9E9E — Grey 600

    // ── Accent ────────────────────────────────────────────────────────────────
    .accentDefault = {0.098f, 0.463f, 0.824f, 1.0f}, // #1976D2 — Blue 700
    .accentHover   = {0.129f, 0.588f, 0.953f, 1.0f}, // #2196F3 — Blue 500
    .accentActive  = {0.051f, 0.278f, 0.631f, 1.0f}, // #0D47A1 — Blue 900

    // ── Text ──────────────────────────────────────────────────────────────────
    .textPrimary   = {0.129f, 0.129f, 0.129f, 1.0f}, // #212121 — Grey 900
    .textSecondary = {0.380f, 0.380f, 0.380f, 1.0f}, // #616161 — Grey 700
    .textDisabled  = {0.620f, 0.620f, 0.620f, 1.0f}, // #9E9E9E — Grey 600
    .textOnAccent  = {1.000f, 1.000f, 1.000f, 1.0f}, // #FFFFFF — white on blue

    // ── Status ────────────────────────────────────────────────────────────────
    .statusSuccess = {0.220f, 0.557f, 0.235f, 1.0f}, // #388E3C — Green 700
    .statusWarning = {0.961f, 0.486f, 0.000f, 1.0f}, // #F57C00 — Orange 700
    .statusError   = {0.827f, 0.184f, 0.184f, 1.0f}, // #D32F2F — Red 700
    .statusInfo    = {0.012f, 0.533f, 0.820f, 1.0f}, // #0288D1 — Light Blue 700

    // ── Spacing & Radius ──────────────────────────────────────────────────────
    .spacing = {8.0f, 4.0f,  8.0f, 8.0f,  4.0f, 3.0f,  21.0f},
    .radius  = {4.0f, 4.0f,  4.0f, 9.0f},
};

} // namespace ImFrame::Themes
