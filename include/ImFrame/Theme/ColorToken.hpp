/**
 * @file     ColorToken.hpp
 * @brief    Lightweight constexpr colour and spacing value types for the theme system
 *
 * Defines three aggregate token types used by @ref ImFrame::Theme::Theme:
 *   - ColorToken   — four floats (r, g, b, a), zero heap allocation
 *   - SpacingToken — seven floats covering ImGui spacing/padding fields
 *   - RadiusToken  — four floats covering ImGui rounding fields
 *
 * All three types are constexpr-constructible. No ImGui headers are included
 * here; translation units that need ImVec4/ImVec2 conversions must perform
 * them explicitly using the public `r`, `g`, `b`, `a` fields.
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

#include <cstdint>

namespace ImFrame::Theme {

// ─── ColorToken ───────────────────────────────────────────────────────────────

/**
 * @brief Constexpr RGBA colour value — four 32-bit floats, no heap allocation
 *
 * Component values are in the range [0.0f, 1.0f]. The struct is a plain
 * aggregate so it can appear in constexpr contexts and designated initialisers.
 *
 * @since 0.9.0
 * @example
 * @code
 * constexpr ColorToken purple{0.741f, 0.576f, 0.976f, 1.0f};
 * constexpr ColorToken red = ColorFromHex(0xFF0000);
 * constexpr ColorToken semitransparent = ColorFromHex(0xBD93F9, 0.5f);
 * @endcode
 */
struct ColorToken {
    float r{0.0f}; ///< Red channel   [0, 1]
    float g{0.0f}; ///< Green channel [0, 1]
    float b{0.0f}; ///< Blue channel  [0, 1]
    float a{1.0f}; ///< Alpha channel [0, 1]
};

/**
 * @brief Construct a ColorToken from a 24-bit 0xRRGGBB hex value
 *
 * @param[in] rgb   Packed 24-bit colour in 0xRRGGBB format
 * @param[in] alpha Alpha channel value in [0.0f, 1.0f] (default 1.0f)
 * @return    ColorToken with each channel normalised to [0.0f, 1.0f]
 */
[[nodiscard]] constexpr ColorToken ColorFromHex(uint32_t rgb, float alpha = 1.0f) noexcept {
    return ColorToken{
        static_cast<float>((rgb >> 16u) & 0xFFu) / 255.0f,
        static_cast<float>((rgb >>  8u) & 0xFFu) / 255.0f,
        static_cast<float>( rgb         & 0xFFu) / 255.0f,
        alpha
    };
}

// ─── SpacingToken ─────────────────────────────────────────────────────────────

/**
 * @brief Constexpr spacing token covering ImGui item/window/frame padding fields
 *
 * Field names map directly to the corresponding ImGuiStyle members:
 * - `ItemSpacingX/Y`     → `ImGuiStyle::ItemSpacing`
 * - `WindowPaddingX/Y`   → `ImGuiStyle::WindowPadding`
 * - `FramePaddingX/Y`    → `ImGuiStyle::FramePadding`
 * - `IndentSpacing`      → `ImGuiStyle::IndentSpacing`
 *
 * @since 0.9.0
 */
struct SpacingToken {
    float ItemSpacingX{8.0f};    ///< Horizontal spacing between widgets
    float ItemSpacingY{4.0f};    ///< Vertical spacing between widgets
    float WindowPaddingX{8.0f};  ///< Horizontal padding inside window edges
    float WindowPaddingY{8.0f};  ///< Vertical padding inside window edges
    float FramePaddingX{4.0f};   ///< Horizontal padding inside framed items
    float FramePaddingY{3.0f};   ///< Vertical padding inside framed items
    float IndentSpacing{21.0f};  ///< Horizontal indentation for tree nodes
};

// ─── RadiusToken ──────────────────────────────────────────────────────────────

/**
 * @brief Constexpr rounding token covering ImGui corner-radius fields
 *
 * Field names map directly to the corresponding ImGuiStyle members:
 * - `Window`    → `ImGuiStyle::WindowRounding`
 * - `Frame`     → `ImGuiStyle::FrameRounding`
 * - `Popup`     → `ImGuiStyle::PopupRounding`
 * - `Scrollbar` → `ImGuiStyle::ScrollbarRounding`
 *
 * @since 0.9.0
 */
struct RadiusToken {
    float Window{4.0f};    ///< Window corner radius in pixels
    float Frame{4.0f};     ///< Framed-item corner radius in pixels
    float Popup{4.0f};     ///< Popup corner radius in pixels
    float Scrollbar{9.0f}; ///< Scrollbar corner radius in pixels
};

} // namespace ImFrame::Theme
