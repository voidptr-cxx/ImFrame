/**
 * @file     FontFace.hpp
 * @brief    Owns one loaded FreeType face bound to a HarfBuzz font for shaping
 *
 * @internal
 * `FontFace` owns exactly one `FT_Face` reference (from `FT_New_Face()`,
 * released by this class's own `FT_Done_Face()` call) and a non-owning
 * `hb_font_t*` wrapper over it (`hb_ft_font_create()`, the non-`_referenced`
 * variant — HarfBuzz does not take its own `FT_Face` reference, so this
 * class's destructor is the sole owner of the face's lifetime; the `hb_font_t`
 * must not outlive the `FT_Face` it wraps, hence it is destroyed first).
 *
 * Lives under `src/Rendering/Text/`, not `include/ImFrame/` — this is Phase
 * 33's first internal building block, with no public API yet (that lands once
 * `Application::WithFont()` is wired to `FontRegistry` in a later sub-phase).
 * FreeType/HarfBuzz headers are included directly here (unlike ImGui, which
 * `include/ImFrame/` must never expose) — there is no equivalent "never expose
 * FreeType/HarfBuzz" architecture invariant, and every consumer of this type
 * (`FontRegistry`, later `TextShaper`/`GlyphAtlas`) needs the real `FT_Face`/
 * `hb_font_t*` types to call shaping/rasterization functions directly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

namespace ImFrame::Internal {

/**
 * @class    FontFace
 * @brief    RAII owner of one `FT_Face` + its bound `hb_font_t*`
 *
 * @internal
 * Move-only — copying an `FT_Face`/`hb_font_t*` pair would require reference
 * counting neither this class nor its callers need (`FontRegistry` owns each
 * `FontFace` by value in a map, keyed by `Rendering::FontId`).
 *
 * @since    3.0.0
 */
class FontFace {
public:
    /**
     * @brief    Loads a font file into a new `FT_Face` and binds a `hb_font_t` to it.
     *
     * @param[in]  library     FreeType library instance. Must outlive the returned `FontFace`.
     * @param[in]  path        Path to a `.ttf`/`.otf` file.
     * @param[in]  sizePixels  Initial pixel size hint, applied via `FT_Set_Pixel_Sizes()`. MSDF
     *                         rendering (Phase 33.4+) allows re-rendering at other sizes without
     *                         quality loss, but FreeType still needs an initial size to hint from.
     *
     * @return   The loaded face, or:
     *           - `Error::FileNotFound` if `path` does not exist.
     *           - `Error::FontLoadFailed` if FreeType/HarfBuzz could not load or bind it.
     * @throws   Nothing.
     */
    [[nodiscard]] static Result<FontFace> Load(FT_Library library, const Utility::Path& path, float sizePixels);

    ~FontFace();

    FontFace(const FontFace&)            = delete;
    FontFace& operator=(const FontFace&) = delete;
    FontFace(FontFace&& other) noexcept;
    FontFace& operator=(FontFace&& other) noexcept;

    [[nodiscard]] FT_Face     Face() const noexcept { return _face; }
    [[nodiscard]] hb_font_t*  HarfBuzzFont() const noexcept { return _hbFont; }
    [[nodiscard]] float       SizePixels() const noexcept { return _sizePixels; }

private:
    FontFace(FT_Face face, hb_font_t* hbFont, float sizePixels) noexcept;

    FT_Face    _face       = nullptr;
    hb_font_t* _hbFont     = nullptr;
    float      _sizePixels = 0.0f;
};

} // namespace ImFrame::Internal
