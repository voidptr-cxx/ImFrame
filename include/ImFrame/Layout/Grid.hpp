/**
 * @file     Grid.hpp
 * @brief    Declarative N-column grid layout — pure layout math, no ImGui table
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <vector>

namespace ImFrame::Layout {

/**
 * @class    GridWidget
 * @brief    Declarative N-column grid layout — pure layout math, no ImGui table
 *
 * `GridWidget` is pure layout math with no ImGui table involved — the same architectural choice
 * already made when `HStack`/`VStack` were replaced by the pure-layout-math
 * `Flex` primitive. Children fill columns left-to-right, wrapping into a new
 * row every `Columns()` children; each column is an equal share of the
 * available width, and each row's height is the tallest child in that row.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * GridWidget(3).Spacing(4.0f).Children({
 *     ButtonWidget("A"), ButtonWidget("B"),
 *     ButtonWidget("C"), ButtonWidget("D"), // wraps to row 2
 * });
 * @endcode
 */
class GridWidget {
public:
    explicit GridWidget(int columns) noexcept : _columns(columns) {}

    GridWidget& Spacing(float cellPadding) noexcept { _cellPadding = cellPadding; return *this; }
    GridWidget& Children(std::vector<Tree::Widget> children) { _children = std::move(children); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    GridWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] int GetColumns() const noexcept { return _columns; }
    [[nodiscard]] float GetSpacing() const noexcept { return _cellPadding; }
    [[nodiscard]] const std::vector<Tree::Widget>& GetChildren() const noexcept { return _children; }

    /// @internal Produces this grid's concrete `Element`. Defined in `src/Tree/RenderObjects/GridRO.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    int                        _columns     = 1;
    float                      _cellPadding = 0.0f;
    std::vector<Tree::Widget>  _children;
    Tree::Key                  _key;
};

} // namespace ImFrame::Layout
