/**
 * @file     Nord.hpp
 * @brief    Nord colour theme — dark arctic blue-grey with steel-blue accents
 *
 * Canonical colour values sourced from the official Nord specification at
 * https://nordtheme.com. Uses the Polar Night palette for backgrounds,
 * Frost palette for accents, Snow Storm for text, and Aurora for status colours.
 * All fields are populated; no field is left at its zero-value default.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-04
 * @version  0.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Theme/Theme.hpp"

namespace ImFrame::Themes {

/**
 * @brief Nord built-in theme
 *
 * Palette: Background #2E3440 (Polar Night), accents #5E81AC (Frost),
 * text #ECEFF4 (Snow Storm), status colours from Aurora.
 *
 * @since 0.9.0
 */
inline constexpr Theme::Theme Nord{
    // ── Background ────────────────────────────────────────────────────────────
    .backgroundPrimary   = {0.180f, 0.204f, 0.251f, 1.0f}, // #2E3440 — Nord0
    .backgroundSecondary = {0.231f, 0.259f, 0.322f, 1.0f}, // #3B4252 — Nord1
    .backgroundTertiary  = {0.263f, 0.298f, 0.369f, 1.0f}, // #434C5E — Nord2

    // ── Surface ───────────────────────────────────────────────────────────────
    .surfaceDefault = {0.298f, 0.337f, 0.416f, 1.0f}, // #4C566A — Nord3
    .surfaceHover   = {0.298f, 0.337f, 0.416f, 0.6f}, // #4C566A @ 60%
    .surfaceActive  = {0.298f, 0.337f, 0.416f, 1.0f}, // #4C566A

    // ── Border ────────────────────────────────────────────────────────────────
    .borderDefault = {0.298f, 0.337f, 0.416f, 0.6f}, // #4C566A @ 60%
    .borderHover   = {0.506f, 0.631f, 0.757f, 1.0f}, // #81A1C1 — Nord9

    // ── Accent ────────────────────────────────────────────────────────────────
    .accentDefault = {0.369f, 0.506f, 0.675f, 1.0f}, // #5E81AC — Nord10
    .accentHover   = {0.506f, 0.631f, 0.757f, 1.0f}, // #81A1C1 — Nord9
    .accentActive  = {0.298f, 0.435f, 0.639f, 1.0f}, // #4C6FA3 — darker Frost

    // ── Text ──────────────────────────────────────────────────────────────────
    .textPrimary   = {0.925f, 0.937f, 0.957f, 1.0f}, // #ECEFF4 — Nord6
    .textSecondary = {0.847f, 0.871f, 0.914f, 1.0f}, // #D8DEE9 — Nord4
    .textDisabled  = {0.298f, 0.337f, 0.416f, 1.0f}, // #4C566A — Nord3
    .textOnAccent  = {0.925f, 0.937f, 0.957f, 1.0f}, // #ECEFF4 — readable on Frost

    // ── Status (Aurora palette) ───────────────────────────────────────────────
    .statusSuccess = {0.639f, 0.745f, 0.549f, 1.0f}, // #A3BE8C — Nord14 Green
    .statusWarning = {0.922f, 0.796f, 0.545f, 1.0f}, // #EBCB8B — Nord13 Yellow
    .statusError   = {0.749f, 0.380f, 0.416f, 1.0f}, // #BF616A — Nord11 Red
    .statusInfo    = {0.533f, 0.753f, 0.816f, 1.0f}, // #88C0D0 — Nord8 Cyan

    // ── Spacing & Radius ──────────────────────────────────────────────────────
    .spacing = {8.0f, 4.0f,  8.0f, 8.0f,  4.0f, 3.0f,  21.0f},
    .radius  = {4.0f, 4.0f,  4.0f, 9.0f},
};

} // namespace ImFrame::Themes
