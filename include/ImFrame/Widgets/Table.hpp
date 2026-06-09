/**
 * @file     Table.hpp
 * @brief    Sortable, virtualised data table widget wrapping ImGui's table and list-clipper APIs
 *
 * `Table` uses `ImGuiListClipper` to render only the rows currently visible in
 * the viewport — a 100 000-row table fires at most ~50 row callbacks per frame.
 *
 * Columns are defined with `ColumnDef` and registered via `Column()`.  Rendering
 * is driven either by a per-row `rowRenderer` (the callback handles all
 * `TableNextColumn()` calls itself) or by per-column `ColumnDef::Renderer`
 * callbacks (the table manages column iteration internally).  The two modes are
 * mutually exclusive: if ANY column has a `Renderer`, per-column mode is used and
 * the global `rowRenderer` passed to `Render()` is ignored.
 *
 * `Table` is designed to be stored as a class member and reused across frames
 * because sort state and context-menu state persist between `Render()` calls.
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

#include <functional>
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
 * @brief    Snapshot of the table's current sort state
 *
 * Retrieve via `Table::GetSortState()` after every `Render()` call.
 * When `dirty` is `true`, re-sort your data to match `specs` then call
 * `Render()` again — the dirty flag resets automatically the following frame.
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
     * When at least one column has a `Renderer`, `Table::Render()` enters
     * per-column mode: it calls `ImGui::TableNextColumn()` and invokes each
     * column's renderer for every visible row.  The global `rowRenderer`
     * argument to `Table::Render()` is unused in this mode.
     *
     * @param[in]  renderer  Callback invoked with the row index for each visible row.
     * @return   `*this` for chaining.
     */
    ColumnDef& Renderer(std::function<void(int rowIndex)> renderer);

private:
    friend class Table;
    std::string                  _label;
    float                        _width       = 0.0f;
    Widgets::ColumnWidthMode     _widthMode   = Widgets::ColumnWidthMode::Stretch;
    bool                         _sortEnabled = false;
    std::function<void(int)>     _renderer;
};

// ─── Table ────────────────────────────────────────────────────────────────────

/**
 * @class    Table
 * @brief    Virtualised, sortable data table built on ImGui tables and ImGuiListClipper
 *
 * Store as a class member and call `Render()` once per frame.  Column
 * definitions are added once via `Column()`; the builder setters may be called
 * before the first `Render()` or updated between frames.
 *
 * **Rendering modes** (automatic selection):
 * - *Per-column mode* — if ANY `ColumnDef` has a `Renderer`, the table calls
 *   `ImGui::TableNextColumn()` and the column renderer for each visible cell.
 *   The `rowRenderer` argument to `Render()` is ignored.
 * - *Row-renderer mode* — otherwise, `rowRenderer` is called once per visible
 *   row and must call `ImGui::TableNextColumn()` before each cell.
 *
 * **Sorting** is the caller's responsibility.  After `Render()`, check
 * `GetSortState().dirty`; when true, re-sort your data per `specs` and redraw.
 *
 * @since    1.4.0
 *
 * @example
 * @code
 * // Per-column renderer mode (recommended for typed data)
 * _table.Column(ColumnDef("Name").SortEnabled()
 *               .Renderer([&](int r) { ImGui::Text("%s", rows[r].name.c_str()); }))
 *       .Column(ColumnDef("Score")
 *               .Renderer([&](int r) { ImGui::Text("%d", rows[r].score); }))
 *       .Striped();
 *
 * _table.Render(static_cast<int>(rows.size()));
 *
 * if (_table.GetSortState().dirty) { std::sort(rows.begin(), rows.end(), ...); }
 * @endcode
 */
class Table {
public:
    /**
     * @brief    Construct a table with a unique identifier and column count.
     * @param[in]  id       Unique ImGui identifier (e.g., `"##my_table"`).
     * @param[in]  columns  Expected number of columns — must match the number of
     *                      `Column()` calls made before `Render()`.
     * @throws   Nothing.
     */
    Table(std::string_view id, int columns);

    /**
     * @brief    Merge additional raw `ImGuiTableFlags` with the builder-derived flags.
     * @param[in]  flags  Any combination of `ImGuiTableFlags_*` values cast to `int`.
     * @return   `*this` for chaining.
     */
    Table& Flags(int flags);

    Table& OuterSize(float width, float height);
    Table& Scrollable(bool scrollable = true);
    Table& Borders(bool borders = true);
    Table& Striped(bool striped = true);

    /**
     * @brief    Add a column definition (call once per column before first `Render()`).
     * @param[in]  def  Column built with `ColumnDef`.
     * @return   `*this` for chaining.
     */
    Table& Column(ColumnDef def);

    /**
     * @brief    Register a per-row right-click context menu callback.
     *
     * When a row is right-clicked the callback is invoked with the zero-based
     * row index inside an open ImGui popup.  Add `ImGui::MenuItem()` calls there.
     *
     * @param[in]  callback  Receives the row index of the right-clicked row.
     * @return   `*this` for chaining.
     */
    Table& ContextMenu(std::function<void(int rowIndex)> callback);

    /**
     * @brief    Render the table for the current frame using `ImGuiListClipper`.
     *
     * Only visible rows fire callbacks regardless of `rowCount`.
     *
     * @param[in]  rowCount     Total number of data rows (may be very large).
     * @param[in]  rowRenderer  Per-row callback; receives the row index and must
     *                          call `ImGui::TableNextColumn()` before each cell.
     *                          Ignored when any column has a `ColumnDef::Renderer`.
     * @throws   Nothing.
     */
    void Render(int rowCount, std::function<void(int rowIndex)> rowRenderer = {});

    /**
     * @brief    Retrieve the table's sort state as of the last `Render()` call.
     *
     * @return   Snapshot of active sort specs and dirty flag.
     *           `dirty` is `true` only on the frame the user changed the sort.
     */
    [[nodiscard]] SortState GetSortState() const;

private:
    std::string                  _id;
    int                          _columnCount;
    int                          _extraFlags       = 0;
    float                        _outerWidth       = 0.0f;
    float                        _outerHeight      = 0.0f;
    bool                         _scrollable       = false;
    bool                         _borders          = true;
    bool                         _striped          = false;
    std::vector<ColumnDef>       _columns;
    std::function<void(int)>     _rowContextMenu;
    SortState                    _sortState {};
    int                          _pendingContextRow = -1;
    int                          _activeContextRow  = -1;
};

} // namespace ImFrame::Widgets
