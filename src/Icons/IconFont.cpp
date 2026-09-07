/**
 * @file     IconFont.cpp
 * @brief    Implementation of IconFont::Load() — atlas merge for FA6 TTF files
 *
 * @internal
 * ImGui headers are confined to this translation unit. The public header
 * (IconFont.hpp) forward-declares ImFont and ImFontAtlas without including
 * imgui.h, preserving the "no ImGui in public headers" invariant.
 *
 * Glyph range lifetime: each call to Load() appends a three-element ImWchar
 * array to a function-static vector. ImGui's font atlas stores only a pointer
 * to this array; the vector keeps the data alive until atlas destruction.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-06
 * @version  0.9.5
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Icons/IconFont.hpp"
#include "ImFrame/Icons/Icons.hpp"

#include <imgui.h>

#include <array>
#include <vector>

namespace ImFrame::Icons {

ImFont* IconFont::Load(ImFontAtlas* atlas, const IconFontConfig& cfg) {
    if (!atlas || cfg.path.Native().empty()) {
        return nullptr;
    }

    // ── Glyph range ───────────────────────────────────────────────────────────
    // Each entry is a {min, max, 0}-terminated ImWchar triple. The atlas stores
    // a pointer to it, so the array must outlive the atlas build step.
    static std::vector<std::array<ImWchar, 3>> s_ranges;

    const auto rMin = static_cast<ImWchar>(cfg.rangeMin  != 0u ? cfg.rangeMin  : Fa::FA_RANGE_MIN);
    const auto rMax = static_cast<ImWchar>(cfg.rangeMax  != 0u ? cfg.rangeMax  : Fa::FA_RANGE_MAX);
    s_ranges.push_back({ rMin, rMax, ImWchar(0) });

    // ── ImFontConfig ──────────────────────────────────────────────────────────
    ImFontConfig fontCfg;
    fontCfg.MergeMode        = cfg.mergeWithPrevious;
    fontCfg.GlyphOffset      = ImVec2(cfg.glyphOffsetX, cfg.glyphOffsetY);
    fontCfg.GlyphMinAdvanceX = 0.0f;

    return atlas->AddFontFromFileTTF(
        cfg.path.ToString().c_str(),
        cfg.sizePixels,
        &fontCfg,
        s_ranges.back().data()
    );
}

} // namespace ImFrame::Icons
