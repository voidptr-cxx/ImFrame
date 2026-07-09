/**
 * @file     Table.hpp
 * @brief    Data table widgets: `ColumnDef` column descriptor and `TableWidget`, a virtualised table
 *
 * `TableWidget` builds a header row of column labels above a `Tree::VirtualList`
 * body, so only the rows currently visible in the viewport are materialised
 * into the widget tree each frame.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ImFrame::Widgets {

// ─── SortDirection ────────────────────────────────────────────────────────────

/**
 * @brief    Requested sort direction for a column
 * @since    1.4.0
 */
enum class SortDirection {
    None,       ///< No sort direction requested
    Ascending,  ///< Sort A → Z, 0 → N
    Descending, ///< Sort Z → A, N → 0
};

// ─── ColumnSortSpec ───────────────────────────────────────────────────────────

/**
 * @brief    Active sort specification for one column
 * @since    1.4.0
 */
struct ColumnSortSpec {
    int           columnIndex; ///< Zero-based column index
    SortDirection direction;   ///< Requested sort direction
};

// ─── SortState ────────────────────────────────────────────────────────────────

/**
 * @brief    Snapshot of a table's current sort state
 *
 * When `dirty` is `true`, re-sort your data to match `specs` — the dirty flag
 * resets automatically the following frame.
 *
 * @since    1.4.0
 */
struct SortState {
    std::vector<ColumnSortSpec> specs; ///< Active sort specifications (may be empty)
    bool                        dirty = false; ///< True when sort changed this frame
};

// ─── ColumnWidthMode ──────────────────────────────────────────────────────────

/**
 * @brief    How a column's width is determined
 * @since    1.4.0
 */
enum class ColumnWidthMode {
    Fixed,   ///< Fixed pixel width; use `ColumnDef::Width()` to set initial size
    Stretch, ///< Column fills remaining available space (default)
    Auto,    ///< Auto-sizes to content width
};

// ─── ColumnDef ────────────────────────────────────────────────────────────────

/**
 * @class    ColumnDef
 * @brief    Definition for one table column: label, sizing, sort, and optional cell renderer
 *
 * @since    1.4.0
 *
 * @example
 * @code
 * ColumnDef("Name")
 *     .Width(200.0f)
 *     .WidthMode(ColumnWidthMode::Fixed)
 *     .SortEnabled()
 *     .Renderer([&](int row) { ImGui::Text("%s", data[row].name.c_str()); });
 * @endcode
 */
class ColumnDef {
public:
    /**
     * @brief    Construct a column definition with the given header label.
     * @param[in]  label  Text shown in the column header row.
     * @throws   Nothing.
     */
    explicit ColumnDef(std::string_view label);

    ColumnDef& Width(float width);
    ColumnDef& WidthMode(Widgets::ColumnWidthMode mode);

    /**
     * @brief    Enable user-sortable header clicks for this column.
     * @param[in]  enabled  Pass `false` to disable on a previously-sortable column.
     * @return   `*this` for chaining.
     */
    ColumnDef& SortEnabled(bool enabled = true);

    /**
     * @brief    Register a per-cell renderer for this column.
     *
     * When at least one column has a `Renderer`, per-column rendering mode is used:
     * table-rendering code calls `ImGui::TableNextColumn()` and invokes each
     * column's renderer for every visible row instead of using a global row renderer.
     *
     * @param[in]  renderer  Callback invoked with the row index for each visible row.
     * @return   `*this` for chaining.
     */
    ColumnDef& Renderer(std::function<void(int rowIndex)> renderer);

private:
    std::string                  _label;
    float                        _width       = 0.0f;
    Widgets::ColumnWidthMode     _widthMode   = Widgets::ColumnWidthMode::Stretch;
    bool                         _sortEnabled = false;
    std::function<void(int)>     _renderer;
};

// ─── TableWidget (Phase 29) ─────────────────────────────────────────────────────

/**
 * @struct   TableColumn
 * @brief    One column of a `TableWidget`: header label and per-row cell text
 * @since    2.4.0
 */
struct TableColumn {
    std::string                                  Label;
    Utility::Delegate<std::string(int rowIndex)>  CellText;
};

/**
 * @class    TableWidget
 * @brief    Virtualised data table built on `Tree::VirtualList` — a `Tree::Component`
 *
 * Reimplements the "large row count, only visible rows built" core of `Table`
 * on top of Phase 29's `VirtualList` instead of `ImGuiListClipper`. A header
 * `Flex` row of column labels sits above a `VirtualList` body; each visible
 * row is a `Flex` of per-column cell `Text`, optionally wrapped in a
 * `GestureRegion` for row-click selection.
 *
 * **Not yet reimplemented**: sort-state tracking, right-click context menus,
 * striped rows, and Fixed/Stretch/Auto column width modes — this phase's scope
 * is the virtualization mechanism itself, not full feature parity with the
 * removed Phase 10–14 table API.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * TableWidget(rows.size(), 24.0f)
 *     .Column("Name",  [&](int r) { return rows[r].name; })
 *     .Column("Score", [&](int r) { return std::to_string(rows[r].score); })
 *     .OnRowClick([&](int r) { selectedRow = r; });
 * @endcode
 */
class TableWidget {
public:
    /**
     * @param[in] rowCount   Total number of data rows (may be very large).
     * @param[in] rowHeight  Uniform row height in pixels.
     */
    TableWidget(int rowCount, float rowHeight) : _rowCount(rowCount), _rowHeight(rowHeight) {}

    /// Adds a column with the given header label and per-row cell-text callback.
    TableWidget& Column(std::string label, Utility::Delegate<std::string(int)> cellText) {
        _columns.push_back(TableColumn{std::move(label), std::move(cellText)});
        return *this;
    }

    /// Fires with the clicked row's index.
    TableWidget& OnRowClick(Utility::Delegate<void(int)> cb) { _onRowClick = std::move(cb); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    TableWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }

    /// @internal Composes the header + `VirtualList` body. Defined in `Table.cpp`.
    [[nodiscard]] Tree::Widget Build() const;

private:
    int                          _rowCount;
    float                        _rowHeight;
    std::vector<TableColumn>     _columns;
    Utility::Delegate<void(int)> _onRowClick;
    Tree::Key                    _key;
};

} // namespace ImFrame::Widgets
