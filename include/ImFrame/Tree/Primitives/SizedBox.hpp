/**
 * @file     SizedBox.hpp
 * @brief    Explicit fixed-size spacer with no content
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

namespace ImFrame::Tree::Primitives {

/**
 * @class    SizedBox
 * @brief    Reserves an exact `Width` × `Height` of layout space; draws nothing
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * SizedBox().Width(16.0f).Height(16.0f); // fixed-size gap
 * @endcode
 */
class SizedBox {
public:
    SizedBox() = default;

    SizedBox& Width(float w) noexcept { _width = w; return *this; }
    SizedBox& Height(float h) noexcept { _height = h; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    SizedBox& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] float     GetWidth() const noexcept { return _width; }
    [[nodiscard]] float     GetHeight() const noexcept { return _height; }

    /// @internal Produces this box's concrete `Element`. Defined in `SizedBoxRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    float     _width  = 0.0f;
    float     _height = 0.0f;
    Tree::Key _key;
};

} // namespace ImFrame::Tree::Primitives
