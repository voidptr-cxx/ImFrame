/**
 * @file     CatppuccinMocha.hpp
 * @brief    Catppuccin Mocha colour theme — warm dark brown with lilac accents
 *
 * Canonical colour values sourced from the official Catppuccin specification at
 * https://github.com/catppuccin/catppuccin (Mocha flavour). Uses Crust/Base for
 * backgrounds, Surface tones for interactive elements, Mauve for accents, and
 * the standard Catppuccin status colours. All fields are populated.
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
 * @brief Catppuccin Mocha built-in theme
 *
 * Palette: Background #1E1E2E (Base), accents #CBA6F7 (Mauve),
 * text #CDD6F4 (Text), status colours from Catppuccin Mocha's palette.
 * Uses slightly rounder corners (radius 6) compared to the other dark themes.
 *
 * @since 0.9.0
 */
inline constexpr Theme::Theme CatppuccinMocha{
    // ── Background ────────────────────────────────────────────────────────────
    .backgroundPrimary   = {0.118f, 0.118f, 0.180f, 1.0f}, // #1E1E2E — Base
    .backgroundSecondary = {0.094f, 0.094f, 0.145f, 1.0f}, // #181825 — Mantle
    .backgroundTertiary  = {0.192f, 0.196f, 0.267f, 1.0f}, // #313244 — Surface0

    // ── Surface ───────────────────────────────────────────────────────────────
    .surfaceDefault = {0.192f, 0.196f, 0.267f, 1.0f}, // #313244 — Surface0
    .surfaceHover   = {0.271f, 0.278f, 0.353f, 1.0f}, // #45475A — Surface1
    .surfaceActive  = {0.345f, 0.357f, 0.439f, 1.0f}, // #585B70 — Surface2

    // ── Border ────────────────────────────────────────────────────────────────
    .borderDefault = {0.271f, 0.278f, 0.353f, 0.7f}, // #45475A @ 70%
    .borderHover   = {0.498f, 0.518f, 0.612f, 1.0f}, // #7F849C — Overlay0

    // ── Accent ────────────────────────────────────────────────────────────────
    .accentDefault = {0.796f, 0.651f, 0.969f, 1.0f}, // #CBA6F7 — Mauve
    .accentHover   = {0.831f, 0.737f, 0.980f, 1.0f}, // #D4BCFA — lighter Mauve
    .accentActive  = {0.694f, 0.573f, 0.929f, 1.0f}, // #B192ED — deeper Mauve

    // ── Text ──────────────────────────────────────────────────────────────────
    .textPrimary   = {0.804f, 0.839f, 0.957f, 1.0f}, // #CDD6F4 — Text
    .textSecondary = {0.729f, 0.761f, 0.871f, 1.0f}, // #BAC2DE — Subtext1
    .textDisabled  = {0.424f, 0.439f, 0.525f, 1.0f}, // #6C7086 — Overlay1
    .textOnAccent  = {0.118f, 0.118f, 0.180f, 1.0f}, // #1E1E2E — Base (dark on lilac)

    // ── Status ────────────────────────────────────────────────────────────────
    .statusSuccess = {0.651f, 0.890f, 0.631f, 1.0f}, // #A6E3A1 — Green
    .statusWarning = {0.980f, 0.702f, 0.529f, 1.0f}, // #FAB387 — Peach
    .statusError   = {0.953f, 0.545f, 0.659f, 1.0f}, // #F38BA8 — Red
    .statusInfo    = {0.537f, 0.863f, 0.922f, 1.0f}, // #89DCEB — Sky

    // ── Spacing & Radius (rounder than other dark themes) ────────────────────
    .spacing = {8.0f, 4.0f,  8.0f, 8.0f,  4.0f, 3.0f,  21.0f},
    .radius  = {6.0f, 6.0f,  6.0f, 9.0f},
};

} // namespace ImFrame::Themes
