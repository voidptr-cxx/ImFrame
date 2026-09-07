/**
 * @file     Stack.hpp
 * @brief    Z-axis layering primitive
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

#include <vector>

namespace ImFrame::Tree::Primitives {

/**
 * @class    Stack
 * @brief    Renders children in order, later children painted on top, no layout negotiation
 *
 * Each child is offered the `Stack`'s own constraints and positioned at the
 * `Stack`'s top-left corner. Used for overlays, badge indicators, and layered
 * visuals.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Stack().Children({ Image(icon), Badge(count) });
 * @endcode
 */
class Stack {
public:
    Stack() = default;

    Stack& Children(std::vector<Widget> children) { _children = std::move(children); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    Stack& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::vector<Widget>& GetChildren() const noexcept { return _children; }

    /// @internal Produces this stack's concrete `Element`. Defined in `StackRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    std::vector<Widget> _children;
    Tree::Key            _key;
};

} // namespace ImFrame::Tree::Primitives
