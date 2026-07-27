/**
 * @file     TextureAtlas.cpp
 * @brief    `TextureAtlas` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "TextureAtlas.hpp"

#include "ImFrame/Core/Error.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace ImFrame::Internal {

namespace {

/// Mints a fresh, non-zero `Rendering::TextureId` for each `TextureAtlas` instance — 0 stays reserved as "invalid".
[[nodiscard]] Rendering::TextureId NextAtlasTextureId() {
    static std::atomic<std::uint64_t> counter{1};
    return Rendering::TextureId{counter.fetch_add(1, std::memory_order_relaxed)};
}

} // namespace

TextureAtlas::TextureAtlas(std::uint32_t initialWidth, std::uint32_t initialHeight, std::uint32_t maxHeight)
    : _width(initialWidth), _height(initialHeight), _maxHeight(std::max(initialHeight, maxHeight)),
      _textureId(NextAtlasTextureId()), _pixels(static_cast<std::size_t>(_width) * _height * 4, 0) {}

bool TextureAtlas::Grow() {
    if (_height >= _maxHeight) { return false; }

    const std::uint32_t newHeight = std::min(_height * 2, _maxHeight);
    std::vector<std::uint8_t> grown(static_cast<std::size_t>(_width) * newHeight * 4, 0);

    // Existing shelf coordinates never move — a straight row-range copy preserves every previously issued
    // AtlasRegion's pixel content exactly where it already was.
    const std::size_t existingBytes = static_cast<std::size_t>(_width) * _height * 4;
    std::memcpy(grown.data(), _pixels.data(), existingBytes);

    _pixels = std::move(grown);
    _height = newHeight;
    return true;
}

std::optional<AtlasRegion> TextureAtlas::Alloc(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > _width) { return std::nullopt; }

    // First-fit: reuse an existing shelf tall enough with room left on its row.
    for (Shelf& shelf : _shelves) {
        if (shelf.Height >= height && shelf.XCursor + width <= _width) {
            const AtlasRegion region{.X = shelf.XCursor, .Y = shelf.YOffset, .Width = width, .Height = height};
            shelf.XCursor += width;
            return region;
        }
    }

    // No open shelf fits — start a new one below the last, growing the atlas in height as needed.
    const std::uint32_t nextY = _shelves.empty() ? 0 : _shelves.back().YOffset + _shelves.back().Height;
    while (nextY + height > _height) {
        if (!Grow()) { return std::nullopt; }
    }

    _shelves.push_back(Shelf{.YOffset = nextY, .Height = height, .XCursor = width});
    return AtlasRegion{.X = 0, .Y = nextY, .Width = width, .Height = height};
}

void TextureAtlas::Upload(const AtlasRegion& region, const std::vector<std::uint8_t>& rgba8Pixels) {
    IMF_ASSERT(rgba8Pixels.size() == static_cast<std::size_t>(region.Width) * region.Height * 4);
    IMF_ASSERT(region.X + region.Width <= _width && region.Y + region.Height <= _height);

    for (std::uint32_t row = 0; row < region.Height; ++row) {
        const std::size_t srcOffset = static_cast<std::size_t>(row) * region.Width * 4;
        const std::size_t dstOffset = (static_cast<std::size_t>(region.Y + row) * _width + region.X) * 4;
        std::memcpy(_pixels.data() + dstOffset, rgba8Pixels.data() + srcOffset,
                    static_cast<std::size_t>(region.Width) * 4);
    }
}

std::vector<std::uint8_t> TextureAtlas::ReadRegion(const AtlasRegion& region) const {
    IMF_ASSERT(region.X + region.Width <= _width && region.Y + region.Height <= _height);

    std::vector<std::uint8_t> out(static_cast<std::size_t>(region.Width) * region.Height * 4, 0);
    for (std::uint32_t row = 0; row < region.Height; ++row) {
        const std::size_t srcOffset = (static_cast<std::size_t>(region.Y + row) * _width + region.X) * 4;
        const std::size_t dstOffset = static_cast<std::size_t>(row) * region.Width * 4;
        std::memcpy(out.data() + dstOffset, _pixels.data() + srcOffset, static_cast<std::size_t>(region.Width) * 4);
    }
    return out;
}

std::pair<Widgets::Vec2, Widgets::Vec2> TextureAtlas::Uv(const AtlasRegion& region) const noexcept {
    const auto w = static_cast<float>(_width);
    const auto h = static_cast<float>(_height);
    const Widgets::Vec2 uvMin{static_cast<float>(region.X) / w, static_cast<float>(region.Y) / h};
    const Widgets::Vec2 uvMax{static_cast<float>(region.X + region.Width) / w,
                               static_cast<float>(region.Y + region.Height) / h};
    return {uvMin, uvMax};
}

} // namespace ImFrame::Internal
