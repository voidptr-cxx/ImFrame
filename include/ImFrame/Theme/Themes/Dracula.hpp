/**
 * @file     Dracula.hpp
 * @brief    Dracula colour theme — dark purple background with lavender accents
 *
 * Canonical colour values sourced from the official Dracula specification at
 * https://draculatheme.com/contribute. All fields are populated; no field is
 * left at its zero-value default.
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
 * @brief Dracula built-in theme
 *
 * Palette: Background #282A36 (dark navy), accents #BD93F9 (purple),
 * text #F8F8F2 (off-white), status colours from Dracula's official palette.
 *
 * @since 0.9.0
 */
inline constexpr Theme::Theme Dracula{
    // ── Background ────────────────────────────────────────────────────────────
    .backgroundPrimary   = {0.157f, 0.165f, 0.212f, 1.0f}, // #282A36 — Base
    .backgroundSecondary = {0.267f, 0.278f, 0.353f, 1.0f}, // #44475A — Current Line
    .backgroundTertiary  = {0.220f, 0.227f, 0.290f, 1.0f}, // #383A4A — between Base/CL

    // ── Surface ───────────────────────────────────────────────────────────────
    .surfaceDefault = {0.267f, 0.278f, 0.353f, 1.0f}, // #44475A
    .surfaceHover   = {0.384f, 0.447f, 0.643f, 0.4f}, // #6272A4 @ 40%
    .surfaceActive  = {0.384f, 0.447f, 0.643f, 1.0f}, // #6272A4

    // ── Border ────────────────────────────────────────────────────────────────
    .borderDefault = {0.384f, 0.447f, 0.643f, 0.5f}, // #6272A4 @ 50%
    .borderHover   = {0.384f, 0.447f, 0.643f, 1.0f}, // #6272A4

    // ── Accent ────────────────────────────────────────────────────────────────
    .accentDefault = {0.741f, 0.576f, 0.976f, 1.0f}, // #BD93F9 — Purple
    .accentHover   = {0.812f, 0.663f, 0.984f, 1.0f}, // #CFA9FB — lighter purple
    .accentActive  = {0.659f, 0.478f, 0.965f, 1.0f}, // #A87AF6 — deeper purple

    // ── Text ──────────────────────────────────────────────────────────────────
    .textPrimary   = {0.973f, 0.973f, 0.949f, 1.0f}, // #F8F8F2 — Foreground
    .textSecondary = {0.749f, 0.749f, 0.749f, 1.0f}, // #BFBFBF — muted
    .textDisabled  = {0.384f, 0.447f, 0.643f, 1.0f}, // #6272A4 — Comment
    .textOnAccent  = {0.157f, 0.165f, 0.212f, 1.0f}, // #282A36 — dark on purple

    // ── Status ────────────────────────────────────────────────────────────────
    .statusSuccess = {0.314f, 0.980f, 0.482f, 1.0f}, // #50FA7B — Green
    .statusWarning = {1.000f, 0.722f, 0.424f, 1.0f}, // #FFB86C — Orange
    .statusError   = {1.000f, 0.333f, 0.333f, 1.0f}, // #FF5555 — Red
    .statusInfo    = {0.545f, 0.914f, 0.992f, 1.0f}, // #8BE9FD — Cyan

    // ── Spacing & Radius ──────────────────────────────────────────────────────
    .spacing = {8.0f, 4.0f,  8.0f, 8.0f,  4.0f, 3.0f,  21.0f},
    .radius  = {4.0f, 4.0f,  4.0f, 9.0f},
};

} // namespace ImFrame::Themes
