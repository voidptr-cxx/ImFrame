/**
 * @file     FontRegistry.hpp
 * @brief    Owns a FreeType library and every loaded `FontFace`, keyed by `Rendering::FontId`
 *
 * @internal
 * `FontRegistry::Load()` is the intended entry point for Phase 33's text pipeline —
 * `Application::WithFont()` will route into it once `NativeRenderer` is active (a
 * later sub-phase; today nothing constructs a `FontRegistry` yet). Until then this
 * class is exercised directly by `Tests/Rendering/FontRegistry_test.cpp`, the same
 * way `ImGuiCompatRenderer`/`NativeRendererGL3` were exercised directly by their own
 * tests before any `Application`-level wiring existed (Phase 31.2/32.4 precedent).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "FontFace.hpp"

#include "ImFrame/Rendering/CommandBuffer.hpp"

#include <cstdint>
#include <unordered_map>

namespace ImFrame::Internal {

/**
 * @class    FontRegistry
 * @brief    Loads font files on demand and hands back an opaque `Rendering::FontId`
 *
 * @internal
 * FreeType library initialisation is lazy — the constructor makes no FreeType
 * calls, matching this codebase's established "constructing an instance never
 * requires a context to already exist" convention (`NativeRendererGL3`'s
 * `EnsureInitialized()`). `_library` is only created on the first `Load()` call.
 *
 * Non-copyable, non-moveable — every `FontId` a caller holds implicitly refers
 * to a specific `FontRegistry` instance's internal map; moving the registry
 * would not invalidate the map entries themselves, but there is no current
 * need to move one, and forbidding it removes any ambiguity about which
 * instance a `FontId` was issued by.
 *
 * @since    3.0.0
 */
class FontRegistry {
public:
    FontRegistry() = default;
    ~FontRegistry();

    FontRegistry(const FontRegistry&)            = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;
    FontRegistry(FontRegistry&&)                 = delete;
    FontRegistry& operator=(FontRegistry&&)      = delete;

    /**
     * @brief    Loads a font file, returning an opaque handle for later lookup.
     *
     * @param[in]  path        Path to a `.ttf`/`.otf` file.
     * @param[in]  sizePixels  Initial pixel size hint — see `FontFace::Load()`.
     * @return   A `FontId` valid for the lifetime of this registry, or:
     *           - `Error::FileNotFound` if `path` does not exist.
     *           - `Error::FontLoadFailed` if the FreeType library failed to
     *             initialise, or the file could not be loaded/bound.
     * @throws   Nothing.
     */
    [[nodiscard]] Result<Rendering::FontId> Load(const Utility::Path& path, float sizePixels);

    /**
     * @brief    Looks up a previously loaded face.
     *
     * @param[in]  id  A `FontId` returned by `Load()`.
     * @return   Pointer to the loaded `FontFace`, or `nullptr` if `id` is
     *           default-constructed (invalid) or unknown to this registry.
     * @throws   Nothing.
     */
    [[nodiscard]] const FontFace* Get(Rendering::FontId id) const noexcept;

private:
    FT_Library _library = nullptr;

    /// Keyed by FontId::Value() rather than FontId itself — FontId has no std::hash
    /// specialization and adding one for this sole internal use isn't warranted.
    std::unordered_map<std::uint32_t, FontFace> _faces;

    std::uint32_t _nextId = 1; ///< 0 is FontId's reserved "invalid/default font" value.
};

} // namespace ImFrame::Internal
