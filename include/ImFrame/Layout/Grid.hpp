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

#include "ImFrame/Widgets/Types.hpp"

#include <string_view>

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
/// @deprecated Phase 10–14 imperative widget API, not yet reimplemented as a Tree Component. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] Grid {
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

} // namespace ImFrame::Layout
