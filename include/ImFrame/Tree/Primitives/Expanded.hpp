/**
 * @file     Expanded.hpp
 * @brief    Flex child that fills a proportional share of leftover main-axis space
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
 * @class    Expanded
 * @brief    Only meaningful as a direct child of `Flex` — claims `Factor` shares of leftover space
 *
 * Two `Expanded` children with factors 1 and 2 split the `Flex`'s leftover
 * main-axis space 1:2. Outside a `Flex`, behaves like a plain pass-through
 * wrapper sized to its child.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Flex().Children({ Expanded(Box()).Factor(1), Expanded(Box()).Factor(2) });
 * @endcode
 */
class Expanded {
public:
    explicit Expanded(Widget child) : _child(std::move(child)) {}

    Expanded& Factor(int factor) noexcept { _factor = factor; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    Expanded& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] int       GetFactor() const noexcept { return _factor; }
    [[nodiscard]] const Widget& GetChild() const noexcept { return _child; }

    /// @internal Produces this widget's concrete `Element`. Defined in `ExpandedRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Widget    _child;
    int       _factor = 1;
    Tree::Key _key;
};

} // namespace ImFrame::Tree::Primitives
