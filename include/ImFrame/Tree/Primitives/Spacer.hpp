/**
 * @file     Spacer.hpp
 * @brief    Invisible flex child that pushes flanking children to opposite ends
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
 * @class    Spacer
 * @brief    Only meaningful as a direct child of `Flex` — consumes all leftover main-axis space
 *
 * Equivalent to `Expanded` with a fixed factor of `1` and no visible content;
 * used to push neighbouring children to opposite ends of a row or column.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Flex().Children({ Text("Left"), Spacer(), Text("Right") });
 * @endcode
 */
class Spacer {
public:
    Spacer() = default;

    /// Explicit identity override — see `Tree::Key`.
    Spacer& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] int       GetFactor() const noexcept { return 1; }

    /// @internal Produces this widget's concrete `Element`. Defined in `SpacerRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Tree::Key _key;
};

} // namespace ImFrame::Tree::Primitives
