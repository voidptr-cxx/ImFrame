/**
 * @file     FontFace.cpp
 * @brief    Implementation of `FontFace::Load()` and its RAII lifetime
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "FontFace.hpp"

#include "ImFrame/Utility/File.hpp"

#include <hb-ft.h>

#include <cmath>

namespace ImFrame::Internal {

Result<FontFace> FontFace::Load(FT_Library library, const Utility::Path& path, float sizePixels) {
    if (!Utility::File::Exists(path)) {
        return std::unexpected(Error::FileNotFound);
    }

    FT_Face face = nullptr;
    if (FT_New_Face(library, path.ToString().c_str(), 0, &face) != 0 || face == nullptr) {
        return std::unexpected(Error::FontLoadFailed);
    }

    const auto sizeInt = static_cast<FT_UInt>(std::lround(sizePixels));
    if (FT_Set_Pixel_Sizes(face, 0, sizeInt) != 0) {
        FT_Done_Face(face);
        return std::unexpected(Error::FontLoadFailed);
    }

    // Non-`_referenced` variant: HarfBuzz does not take its own FT_Face reference, so this
    // FontFace's destructor remains the sole owner — see this file's own header comment.
    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    if (hbFont == nullptr) {
        FT_Done_Face(face);
        return std::unexpected(Error::FontLoadFailed);
    }

    return FontFace(face, hbFont, sizePixels);
}

FontFace::FontFace(FT_Face face, hb_font_t* hbFont, float sizePixels) noexcept
    : _face(face), _hbFont(hbFont), _sizePixels(sizePixels) {}

FontFace::~FontFace() {
    if (_hbFont != nullptr) { hb_font_destroy(_hbFont); }
    if (_face != nullptr) { FT_Done_Face(_face); }
}

FontFace::FontFace(FontFace&& other) noexcept
    : _face(other._face), _hbFont(other._hbFont), _sizePixels(other._sizePixels) {
    other._face   = nullptr;
    other._hbFont = nullptr;
}

FontFace& FontFace::operator=(FontFace&& other) noexcept {
    if (this != &other) {
        if (_hbFont != nullptr) { hb_font_destroy(_hbFont); }
        if (_face != nullptr) { FT_Done_Face(_face); }

        _face       = other._face;
        _hbFont     = other._hbFont;
        _sizePixels = other._sizePixels;

        other._face   = nullptr;
        other._hbFont = nullptr;
    }
    return *this;
}

} // namespace ImFrame::Internal
