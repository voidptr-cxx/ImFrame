/**
 * @file     FontRegistry.cpp
 * @brief    Implementation of `FontRegistry::Load()`/`Get()` and its RAII lifetime
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

#include "FontRegistry.hpp"

namespace ImFrame::Internal {

FontRegistry::~FontRegistry() {
    // Explicit clear before FT_Done_FreeType() — a user-provided destructor body runs
    // BEFORE member destructors, so without this, _faces' FontFace entries would call
    // FT_Done_Face() on an already-destroyed library once their own destructors ran.
    _faces.clear();
    if (_library != nullptr) {
        FT_Done_FreeType(_library);
        _library = nullptr;
    }
}

Result<Rendering::FontId> FontRegistry::Load(const Utility::Path& path, float sizePixels) {
    if (_library == nullptr) {
        if (FT_Init_FreeType(&_library) != 0) {
            return std::unexpected(Error::FontLoadFailed);
        }
    }

    Result<FontFace> face = FontFace::Load(_library, path, sizePixels);
    if (!face) {
        return std::unexpected(face.error());
    }

    const std::uint32_t id = _nextId++;
    _faces.emplace(id, std::move(*face));
    return Rendering::FontId(id);
}

const FontFace* FontRegistry::Get(Rendering::FontId id) const noexcept {
    if (!id.IsValid()) { return nullptr; }
    const auto it = _faces.find(id.Value());
    return it != _faces.end() ? &it->second : nullptr;
}

} // namespace ImFrame::Internal
