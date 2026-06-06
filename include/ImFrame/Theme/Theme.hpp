/**
 * @file     Theme.hpp
 * @brief    Semantic theme descriptor mapping design tokens to ImGui style slots
 *
 * A Theme is a constexpr-eligible aggregate that carries typed colour, spacing,
 * and radius tokens. Semantic colour groups are:
 *   background  — primary, secondary, tertiary window layers
 *   surface     — interactive surface states (default / hover / active)
 *   border      — separator and outline states
 *   accent      — brand/highlight colour states
 *   text        — foreground text and on-accent text
 *   status      — success / warning / error / info indicators
 *
 * The only non-constexpr operation is @ref Theme::Apply(), which translates
 * every token to the corresponding ImGuiCol_* and ImGuiStyle field. Apply()
 * must be called from a translation unit that includes <imgui.h>; it must not
 * be called from a public header.
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

#include "ImFrame/Theme/ColorToken.hpp"

namespace ImFrame::Theme {

// ─── ColorRole ────────────────────────────────────────────────────────────────

/**
 * @brief Enumerates each semantic colour role in @ref Theme
 *
 * Used by @ref ThemeBuilder::SetColor to identify which field to override
 * when constructing a custom theme at runtime.
 *
 * @since 0.9.0
 */
enum class ColorRole {
    BackgroundPrimary,   ///< Primary window/panel background
    BackgroundSecondary, ///< Child windows, popups, menu bars
    BackgroundTertiary,  ///< Title bars, tab bar background

    SurfaceDefault,      ///< Default interactive surface (frame, button)
    SurfaceHover,        ///< Hovered interactive surface
    SurfaceActive,       ///< Pressed/active interactive surface

    BorderDefault,       ///< Default separator and outline
    BorderHover,         ///< Hovered separator and outline

    AccentDefault,       ///< Primary brand/highlight colour
    AccentHover,         ///< Hovered accent
    AccentActive,        ///< Pressed/active accent

    TextPrimary,         ///< Primary foreground text
    TextSecondary,       ///< Secondary/subdued text
    TextDisabled,        ///< Disabled text
    TextOnAccent,        ///< Text drawn on top of accent backgrounds

    StatusSuccess,       ///< Success / positive indicator
    StatusWarning,       ///< Warning / caution indicator
    StatusError,         ///< Error / negative indicator
    StatusInfo,          ///< Informational indicator
};

// ─── Theme ────────────────────────────────────────────────────────────────────

/**
 * @brief Constexpr-eligible theme descriptor — semantic tokens for every ImGui colour slot
 *
 * Theme is a plain aggregate; construct it with designated initialisers to get
 * compile-time theme constants:
 *
 * @since 0.9.0
 * @example
 * @code
 * // Apply a built-in theme
 * app.WithTheme(ImFrame::Themes::Dracula);
 *
 * // Build a custom theme at runtime
 * auto myTheme = ImFrame::Theme::ThemeBuilder{ImFrame::Themes::Nord}
 *                    .SetColor(ImFrame::Theme::ColorRole::AccentDefault,
 *                              ImFrame::Theme::ColorFromHex(0xFF79C6))
 *                    .Build();
 * app.WithTheme(myTheme);
 * @endcode
 */
struct Theme {
    // ─── Background ───────────────────────────────────────────────────────────
    ColorToken backgroundPrimary;   ///< Primary window/panel background
    ColorToken backgroundSecondary; ///< Child windows, popups, menu bars
    ColorToken backgroundTertiary;  ///< Title bars, tab bar background

    // ─── Surface ──────────────────────────────────────────────────────────────
    ColorToken surfaceDefault;  ///< Default interactive surface
    ColorToken surfaceHover;    ///< Hovered interactive surface
    ColorToken surfaceActive;   ///< Pressed/active interactive surface

    // ─── Border ───────────────────────────────────────────────────────────────
    ColorToken borderDefault; ///< Default separator and outline
    ColorToken borderHover;   ///< Hovered separator and outline

    // ─── Accent ───────────────────────────────────────────────────────────────
    ColorToken accentDefault; ///< Primary brand/highlight colour
    ColorToken accentHover;   ///< Hovered accent
    ColorToken accentActive;  ///< Pressed/active accent

    // ─── Text ─────────────────────────────────────────────────────────────────
    ColorToken textPrimary;  ///< Primary foreground text
    ColorToken textSecondary;///< Secondary/subdued text
    ColorToken textDisabled; ///< Disabled text
    ColorToken textOnAccent; ///< Text drawn on top of accent backgrounds

    // ─── Status ───────────────────────────────────────────────────────────────
    ColorToken statusSuccess; ///< Success/positive indicator
    ColorToken statusWarning; ///< Warning/caution indicator
    ColorToken statusError;   ///< Error/negative indicator
    ColorToken statusInfo;    ///< Informational indicator

    // ─── Spacing & Radius ─────────────────────────────────────────────────────
    SpacingToken spacing; ///< Item, window, and frame padding values
    RadiusToken  radius;  ///< Corner rounding values

    /**
     * @brief Translate every semantic token to the corresponding ImGui style slot
     *
     * Writes directly to `ImGui::GetStyle()`. Must be called with an active
     * ImGui context (i.e. after `ImGui::CreateContext()`). Called at most once
     * per frame from `Application::RunOneFrame()` when the dirty flag is set.
     *
     * A `static_assert` inside the implementation verifies that
     * `ImGuiCol_COUNT == 63`; if Dear ImGui adds a new colour slot in a future
     * update the build will fail until `Apply()` is updated to cover it.
     */
    void Apply() const;
};

} // namespace ImFrame::Theme
