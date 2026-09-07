/**
 * @file     IconFont.hpp
 * @brief    Icon font loader that merges a FontAwesome 6 TTF into ImGui's font atlas
 *
 * Call `IconFont::Load()` once per font file during application startup — before
 * the first call to `ImGui::NewFrame()`. The font must be merged into an atlas that
 * already contains at least one regular text font; a standalone icon atlas is not
 * supported (see invariant below).
 *
 * Invariant: `MergeWithPrevious` is always `true`. Attempting to load an icon font
 * as the sole atlas entry will produce an empty atlas and a null return value.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-06
 * @version  0.9.5
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Utility/Path.hpp"

#include <cstdint>

// Forward-declare ImGui atlas and font types without pulling in imgui.h.
// These structs are defined in imgui.h; we only use pointer types here.
struct ImFont;
struct ImFontAtlas;

namespace ImFrame::Icons {

// ─── IconFontConfig ───────────────────────────────────────────────────────────

/**
 * @struct   IconFontConfig
 * @brief    Parameters for merging one FA6 TTF file into an existing font atlas
 *
 * @since    0.9.5
 *
 * @example
 * @code
 * auto* font = ImFrame::Icons::IconFont::Load(
 *     ImGui::GetIO().Fonts,
 *     { .Path       = "Assets/Fonts/fa-solid-900.ttf",
 *       .SizePixels = 14.0f }
 * );
 * @endcode
 */
struct IconFontConfig {
    Utility::Path path;                     ///< Absolute or relative path to the .ttf file.
    float         sizePixels      = 14.0f;  ///< Glyph height in pixels (before DPI scaling).
    uint32_t      rangeMin        = 0u;     ///< Override glyph range min (0 = use FA_RANGE_MIN).
    uint32_t      rangeMax        = 0u;     ///< Override glyph range max (0 = use FA_RANGE_MAX).
    bool          mergeWithPrevious = true; ///< Must remain true — icon fonts are always merged.
    float         glyphOffsetX    = 0.0f;  ///< Horizontal glyph shift in pixels.
    float         glyphOffsetY    = 2.0f;  ///< Vertical glyph shift (positive = down). FA6 needs ≈ 2.
};

// ─── IconFont ─────────────────────────────────────────────────────────────────

/**
 * @class    IconFont
 * @brief    Static helper that registers an icon TTF with an `ImFontAtlas`
 *
 * All methods are static — `IconFont` is not meant to be instantiated.
 *
 * @since    0.9.5
 */
class IconFont {
public:
    IconFont()                             = delete;
    IconFont(const IconFont&)              = delete;
    IconFont& operator=(const IconFont&)   = delete;

    /**
     * @brief    Merge an icon font file into an existing atlas.
     *
     * Configures `ImFontConfig::MergeMode = true` and sets the FA6 glyph range
     * before calling `atlas->AddFontFromFileTTF()`. The glyph range array is kept
     * alive in a function-static buffer so the atlas can reference it until
     * `atlas->Build()` is called.
     *
     * @param[in]  atlas  ImGui font atlas to merge into. Must not be null, and must
     *                    already contain at least one regular text font.
     * @param[in]  cfg    Load parameters (path, size, range overrides, glyph offset).
     * @return   Pointer to the merged `ImFont` entry on success; `nullptr` if the
     *           path is empty or the atlas pointer is null.
     * @throws   Nothing — errors result in a null return value.
     */
    [[nodiscard]] static ImFont* Load(ImFontAtlas* atlas, const IconFontConfig& cfg);
};

} // namespace ImFrame::Icons
