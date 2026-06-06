/**
 * @file     ThemeBuilder.hpp
 * @brief    Runtime fluent builder for constructing custom Theme values from a base theme
 *
 * ThemeBuilder is the escape hatch for user-defined themes. It starts from a
 * fully-specified base (typically one of the built-in themes in ImFrame::Themes)
 * and exposes per-role colour overrides plus spacing/radius replacements. The
 * final Theme value is produced by Build() and can be passed directly to
 * Application::WithTheme().
 *
 * ThemeBuilder is fully header-only: none of its methods call ImGui APIs, so
 * no <imgui.h> dependency is introduced. Following the Delegate precedent, the
 * inline bodies remain in the header.
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

namespace ImFrame::Theme {

/**
 * @brief Fluent runtime builder that produces a Theme from a base + overrides
 *
 * Construct with a base theme, call setters for each override, then call
 * Build() to obtain the final Theme. The builder does not call Apply() — that
 * responsibility belongs to Application::RunOneFrame().
 *
 * @since 0.9.0
 * @example
 * @code
 * auto custom = ThemeBuilder{ImFrame::Themes::Dracula}
 *                   .SetColor(ColorRole::AccentDefault,
 *                             ColorFromHex(0xFF79C6))   // Dracula pink
 *                   .SetRadius({.Window = 8.0f, .Frame = 8.0f,
 *                               .Popup  = 8.0f, .Scrollbar = 9.0f})
 *                   .Build();
 * app.WithTheme(custom);
 * @endcode
 */
class ThemeBuilder {
public:
    /**
     * @brief Initialise the builder from a base theme
     * @param[in] base  Starting theme; all fields are copied into the builder
     */
    explicit ThemeBuilder(const Theme& base) noexcept : _theme(base) {}

    /**
     * @brief Override a single semantic colour role
     * @param[in] role   Which role to replace
     * @param[in] token  New colour value
     * @return Reference to this builder for fluent chaining
     */
    ThemeBuilder& SetColor(ColorRole role, ColorToken token) noexcept {
        switch (role) {
            case ColorRole::BackgroundPrimary:   _theme.backgroundPrimary   = token; break;
            case ColorRole::BackgroundSecondary: _theme.backgroundSecondary = token; break;
            case ColorRole::BackgroundTertiary:  _theme.backgroundTertiary  = token; break;
            case ColorRole::SurfaceDefault:      _theme.surfaceDefault      = token; break;
            case ColorRole::SurfaceHover:        _theme.surfaceHover        = token; break;
            case ColorRole::SurfaceActive:       _theme.surfaceActive       = token; break;
            case ColorRole::BorderDefault:       _theme.borderDefault       = token; break;
            case ColorRole::BorderHover:         _theme.borderHover         = token; break;
            case ColorRole::AccentDefault:       _theme.accentDefault       = token; break;
            case ColorRole::AccentHover:         _theme.accentHover         = token; break;
            case ColorRole::AccentActive:        _theme.accentActive        = token; break;
            case ColorRole::TextPrimary:         _theme.textPrimary         = token; break;
            case ColorRole::TextSecondary:       _theme.textSecondary       = token; break;
            case ColorRole::TextDisabled:        _theme.textDisabled        = token; break;
            case ColorRole::TextOnAccent:        _theme.textOnAccent        = token; break;
            case ColorRole::StatusSuccess:       _theme.statusSuccess       = token; break;
            case ColorRole::StatusWarning:       _theme.statusWarning       = token; break;
            case ColorRole::StatusError:         _theme.statusError         = token; break;
            case ColorRole::StatusInfo:          _theme.statusInfo          = token; break;
        }
        return *this;
    }

    /**
     * @brief Replace the spacing token entirely
     * @param[in] spacing  New spacing values
     * @return Reference to this builder for fluent chaining
     */
    ThemeBuilder& SetSpacing(SpacingToken spacing) noexcept {
        _theme.spacing = spacing;
        return *this;
    }

    /**
     * @brief Replace the radius token entirely
     * @param[in] radius  New corner-radius values
     * @return Reference to this builder for fluent chaining
     */
    ThemeBuilder& SetRadius(RadiusToken radius) noexcept {
        _theme.radius = radius;
        return *this;
    }

    /**
     * @brief Produce the final Theme value
     * @return Fully-specified Theme containing the base plus all applied overrides
     */
    [[nodiscard]] Theme Build() const noexcept { return _theme; }

private:
    Theme _theme;
};

} // namespace ImFrame::Theme
