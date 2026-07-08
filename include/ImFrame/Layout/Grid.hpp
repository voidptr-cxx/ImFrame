/**
 * @file     Grid.hpp
 * @brief    Fixed-column grid layout using the ImGui table API
 *
 * `Grid::Render()` is a variadic template constrained by `Widgets::Renderable`.
 * The ImGui table calls (`BeginTable`, `TableNextColumn`, `EndTable`) are
 * delegated to non-template internal helpers so that `<imgui.h>` is not pulled
 * into this public header.
 *
 * Items fill columns left-to-right; ImGui handles row creation automatically
 * when the column count wraps.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>
#include <vector>

// ─── Internal helpers (declared here for template Render() — not public API) ─
namespace ImFrame::Internal {

/** @brief  Call `ImGui::BeginTable(id, columns, flags)`. Returns false if not visible. */
bool GridBeginTable(std::string_view id, int columns, float cellPadding);

/** @brief  Call `ImGui::TableNextColumn()`. */
void GridNextColumn();

/** @brief  Call `ImGui::EndTable()`. Only valid after a successful `GridBeginTable()`. */
void GridEndTable();

} // namespace ImFrame::Internal

namespace ImFrame::Layout {

/**
 * @class    Grid
 * @brief    Divides available width into N equal columns and fills them left-to-right
 *
 * Backed by the ImGui table API with `ImGuiTableFlags_None`. Items are placed
 * into successive columns; a new row is created automatically when the column
 * count wraps.
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * Grid(3).Render(
 *     Widgets::Button("A"),
 *     Widgets::Button("B"),
 *     Widgets::Button("C"),
 *     Widgets::Button("D")   // wraps to row 2, column 1
 * );
 * @endcode
 */
/// @deprecated Use `GridWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use GridWidget instead. See Docs/Migration_v1_to_v2.md.")]] Grid {
public:
    /**
     * @brief    Construct a grid with the given column count.
     * @param[in]  columns  Number of equal-width columns. Must be >= 1.
     * @throws   Nothing.
     */
    explicit Grid(int columns) noexcept : _columns(columns) {}

    Grid& Spacing(float cellPadding) { _cellPadding = cellPadding; return *this; }
    Grid& Id(std::string_view id)    { _id = id;                   return *this; }

    /**
     * @brief    Render all `widgets` into the grid, wrapping columns as needed.
     * @tparam   Ts  Widget types; each must satisfy `Widgets::Renderable`.
     * @param[in,out]  widgets  Widgets to render; each occupies one cell.
     * @return   Nothing.
     * @throws   Nothing.
     */
    template <Widgets::Renderable... Ts>
    void Render(Ts&&... widgets) {
        if (!Internal::GridBeginTable(_id, _columns, _cellPadding)) return;
        auto doRender = [](auto& w) {
            Internal::GridNextColumn();
            w.Show();
        };
        (doRender(widgets), ...);
        Internal::GridEndTable();
    }

private:
    std::string_view _id          = "##imf_grid";
    int              _columns     = 1;
    float            _cellPadding = 0.0f;
};

// ─── GridWidget (Phase 30) ──────────────────────────────────────────────────────

/**
 * @class    GridWidget
 * @brief    Declarative N-column layout — `Tree::PrimitiveWidget` replacement for `Grid`
 *
 * Unlike `Grid` (backed by `ImGui::BeginTable`), `GridWidget` is pure
 * layout math with no ImGui table involved — the same architectural choice
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
 *     Widget(ButtonWidget("A")), Widget(ButtonWidget("B")),
 *     Widget(ButtonWidget("C")), Widget(ButtonWidget("D")), // wraps to row 2
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
