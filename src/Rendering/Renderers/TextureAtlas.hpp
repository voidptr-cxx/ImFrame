/**
 * @file     TextureAtlas.hpp
 * @brief    Packs small images into one growable atlas texture
 *
 * @internal
 * `TextureAtlas` reserves rectangular regions via a shelf packer: rows
 * ("shelves") are filled left-to-right, and a new shelf starts under the
 * previous one when nothing already open is tall enough. When no shelf has
 * room, the atlas grows by doubling its *height* (up to `MaxAtlasSize`,
 * default 4096) while keeping width fixed at its initial value.
 *
 * This is a deliberate simplification of `PHASE_32_PROPOSAL.md`'s literal
 * "starts at 1024×1024 and doubles when full" (square growth): height-only
 * growth means every `AtlasRegion` a caller has already been given keeps the
 * exact same absolute pixel coordinates forever — a square-growth atlas would
 * either have to move already-packed rects (invalidating every previously
 * issued `AtlasRegion`) or waste the newly-doubled width entirely. `Uv()`
 * computes normalized UVs from `Width()`/`Height()` *at call time*, not at
 * `Alloc()` time, so growth is always safe to call between an `Alloc()` and
 * its matching `Uv()`/`Upload()`.
 *
 * `TextureAtlas` also owns a CPU-side RGBA8 pixel buffer and implements
 * `Upload()`/`ReadRegion()` against it directly — there is no real GPU
 * texture here yet. Uploading that buffer to an actual GPU texture (and
 * recreating the texture object on growth) is a real backend's job, added in
 * a later Phase 32 sub-phase once `NativeRendererGL3` exists. Growing here
 * simply reallocates the pixel buffer and row-copies existing content into
 * place, since shelf coordinates never move.
 *
 * The vendored `rectpack2D` algorithm named in the proposal was deliberately
 * not used — see `.claude/DECISIONS.md`, Phase 32 Interstitial, for why.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/CommandBuffer.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ImFrame::Internal {

/// One packed rectangle's absolute pixel coordinates within a `TextureAtlas`.
struct AtlasRegion {
    std::uint32_t X      = 0;
    std::uint32_t Y      = 0;
    std::uint32_t Width  = 0;
    std::uint32_t Height = 0;
};

/**
 * @class    TextureAtlas
 * @brief    Packs small images (glyph bitmaps, UI icons) into one growable RGBA8 atlas
 *
 * @since    3.0.0
 *
 * @example
 * @code
 * TextureAtlas atlas; // 1024 wide, grows in height up to 4096
 * auto region = atlas.Alloc(16, 16);
 * if (region) {
 *     atlas.Upload(*region, glyphPixels);
 *     Widgets::Vec2 uvMin, uvMax;
 *     std::tie(uvMin, uvMax) = atlas.Uv(*region);
 * }
 * @endcode
 */
class TextureAtlas {
public:
    static constexpr std::uint32_t kDefaultWidth    = 1024;
    static constexpr std::uint32_t kDefaultHeight    = 1024;
    static constexpr std::uint32_t kDefaultMaxHeight = 4096;

    /**
     * @brief    Constructs an atlas starting at `initialWidth` x `initialHeight`, growing in height only.
     * @param[in] initialWidth   Fixed width for the lifetime of this atlas. Default 1024.
     * @param[in] initialHeight  Starting height. Default 1024.
     * @param[in] maxHeight      Height ceiling — `Alloc()` fails once growth would exceed this. Default 4096.
     */
    explicit TextureAtlas(std::uint32_t initialWidth = kDefaultWidth, std::uint32_t initialHeight = kDefaultHeight,
                           std::uint32_t maxHeight = kDefaultMaxHeight);

    /**
     * @brief    Reserves a `width` x `height` region, growing the atlas in height if no shelf has room.
     * @param[in] width   Requested region width, in pixels. Must be `<= Width()`.
     * @param[in] height  Requested region height, in pixels.
     * @return   The reserved region, or `std::nullopt` if it cannot fit even after growing to `MaxHeight()`.
     */
    [[nodiscard]] std::optional<AtlasRegion> Alloc(std::uint32_t width, std::uint32_t height);

    /**
     * @brief    Copies `rgba8Pixels` (exactly `region.Width * region.Height * 4` bytes) into the atlas buffer.
     * @param[in] region       A region previously returned by `Alloc()` on this atlas.
     * @param[in] rgba8Pixels  Tightly-packed RGBA8 source pixels, `region.Width * region.Height * 4` bytes.
     */
    void Upload(const AtlasRegion& region, const std::vector<std::uint8_t>& rgba8Pixels);

    /// Reads back the pixels previously uploaded to `region` — for tests and diagnostics only.
    [[nodiscard]] std::vector<std::uint8_t> ReadRegion(const AtlasRegion& region) const;

    /// Normalized `[0,1]` UV bounds for `region`, computed from the atlas's *current* dimensions.
    [[nodiscard]] std::pair<Widgets::Vec2, Widgets::Vec2> Uv(const AtlasRegion& region) const noexcept;

    [[nodiscard]] std::uint32_t Width() const noexcept { return _width; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return _height; }
    [[nodiscard]] std::uint32_t MaxHeight() const noexcept { return _maxHeight; }

    /// Opaque id for this atlas's (conceptual, not-yet-GPU-backed) texture — stable across growth.
    [[nodiscard]] Rendering::TextureId TextureId() const noexcept { return _textureId; }

    /// Increments on every `Upload()`/`Grow()` call — lets a real GPU-backed consumer (a later
    /// sub-phase's `TextRendererGL3`, mirroring this atlas's pixel buffer into an actual GL
    /// texture) detect "has the CPU-side buffer changed since I last uploaded it to the GPU"
    /// without diffing pixels, by remembering the generation it last uploaded.
    [[nodiscard]] std::uint64_t Generation() const noexcept { return _generation; }

private:
    struct Shelf {
        std::uint32_t YOffset = 0;
        std::uint32_t Height  = 0;
        std::uint32_t XCursor = 0;
    };

    /// Doubles `_height` (capped at `_maxHeight`) and grows `_pixels` in place. Returns false if already at the cap.
    bool Grow();

    std::uint32_t _width;
    std::uint32_t _height;
    std::uint32_t _maxHeight;
    Rendering::TextureId _textureId;
    std::uint64_t _generation = 0;

    std::vector<Shelf> _shelves;
    std::vector<std::uint8_t> _pixels; ///< RGBA8, row-major, `_width * _height * 4` bytes.
};

} // namespace ImFrame::Internal
