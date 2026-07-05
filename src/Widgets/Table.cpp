/**
 * @file     Table.cpp
 * @brief    Implementation of Widgets::Table and Widgets::ColumnDef
 *
 * @internal
 * ImGui headers are confined to this translation unit.  The public header
 * (Table.hpp) contains no ImGui includes, preserving the architecture invariant.
 *
 * Rendering mode selection:
 *   Per-column mode  — if any ColumnDef has a Renderer, the table iterates
 *     columns itself: TableNextColumn() + colDef.renderer(row) per visible cell.
 *   Row-renderer mode — otherwise, rowRenderer(row) is called once per visible
 *     row; the callback is responsible for all TableNextColumn() calls.
 *
 * Context menu:
 *   Right-click is detected by comparing the mouse Y position against each
 *   row's top cursor position during the clipper loop.  The row index is stored
 *   in _pendingContextRow and used to open a popup after EndTable().
 *   _activeContextRow persists while the popup is open so the callback
 *   receives the correct index even on frames after the initial click.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Table.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/VirtualList.hpp"

#include <imgui.h>

#include <algorithm>

namespace ImFrame::Widgets {

// MSVC's C4996 fires on the deprecated `Table`'s own out-of-line fluent setters
// below (their `Table&` return type counts as a "use" of the deprecated class,
// even in the class's own implementation) — unlike Button/Checkbox, whose
// setters are inline in the header and so exempt. Suppressed here since this is
// the deprecated API's own continued implementation, not an external caller.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

// ─── ColumnDef ────────────────────────────────────────────────────────────────

ColumnDef::ColumnDef(std::string_view label) : _label(label) {}

ColumnDef& ColumnDef::Width(float width) {
    _width = width;
    return *this;
}

ColumnDef& ColumnDef::WidthMode(Widgets::ColumnWidthMode mode) {
    _widthMode = mode;
    return *this;
}

ColumnDef& ColumnDef::SortEnabled(bool enabled) {
    _sortEnabled = enabled;
    return *this;
}

ColumnDef& ColumnDef::Renderer(std::function<void(int)> renderer) {
    _renderer = std::move(renderer);
    return *this;
}

// ─── Table ────────────────────────────────────────────────────────────────────

Table::Table(std::string_view id, int columns)
    : _id(id), _columnCount(columns) {}

Table& Table::Flags(int flags)                          { _extraFlags = flags;         return *this; }
Table& Table::OuterSize(float w, float h)               { _outerWidth = w; _outerHeight = h; return *this; }
Table& Table::Scrollable(bool v)                        { _scrollable = v;             return *this; }
Table& Table::Borders(bool v)                           { _borders    = v;             return *this; }
Table& Table::Striped(bool v)                           { _striped    = v;             return *this; }
Table& Table::Column(ColumnDef def)                     { _columns.push_back(std::move(def)); return *this; }
Table& Table::ContextMenu(std::function<void(int)> cb)  { _rowContextMenu = std::move(cb); return *this; }

SortState Table::GetSortState() const { return _sortState; }

void Table::Render(int rowCount, std::function<void(int)> rowRenderer) {
    const bool useColumnRenderers = std::any_of(
        _columns.begin(), _columns.end(),
        [](const ColumnDef& c) { return static_cast<bool>(c._renderer); });

    // Build flags from builder state
    int flags = _extraFlags;
    if (_scrollable) flags |= ImGuiTableFlags_ScrollY;
    if (_borders)    flags |= ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH;
    if (_striped)    flags |= ImGuiTableFlags_RowBg;

    const bool anySortable = std::any_of(
        _columns.begin(), _columns.end(),
        [](const ColumnDef& c) { return c._sortEnabled; });
    if (anySortable) flags |= ImGuiTableFlags_Sortable;

    if (!ImGui::BeginTable(_id.c_str(), _columnCount, flags,
                           ImVec2{_outerWidth, _outerHeight})) {
        return;
    }

    // Column setup
    for (const auto& col : _columns) {
        ImGuiTableColumnFlags colFlags = ImGuiTableColumnFlags_None;
        if (!col._sortEnabled)
            colFlags |= ImGuiTableColumnFlags_NoSort;
        if (col._widthMode == ColumnWidthMode::Fixed)
            colFlags |= ImGuiTableColumnFlags_WidthFixed;
        else if (col._widthMode == ColumnWidthMode::Stretch)
            colFlags |= ImGuiTableColumnFlags_WidthStretch;
        ImGui::TableSetupColumn(col._label.c_str(), colFlags, col._width);
    }
    ImGui::TableHeadersRow();

    // Read back sort state
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
        if (specs->SpecsDirty) {
            _sortState.dirty = true;
            _sortState.specs.clear();
            _sortState.specs.reserve(static_cast<size_t>(specs->SpecsCount));
            for (int i = 0; i < specs->SpecsCount; ++i) {
                const auto& s = specs->Specs[i];
                SortDirection dir = SortDirection::None;
                if (s.SortDirection == ImGuiSortDirection_Ascending)
                    dir = SortDirection::Ascending;
                else if (s.SortDirection == ImGuiSortDirection_Descending)
                    dir = SortDirection::Descending;
                _sortState.specs.push_back({static_cast<int>(s.ColumnIndex), dir});
            }
            specs->SpecsDirty = false;
        } else {
            _sortState.dirty = false;
        }
    }

    const float rowH          = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 windowPos    = ImGui::GetWindowPos();
    const float  windowWidth  = ImGui::GetWindowWidth();
    _pendingContextRow = -1;

    ImGuiListClipper clipper;
    clipper.Begin(rowCount);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            ImGui::TableNextRow();
            const float rowTop = ImGui::GetCursorScreenPos().y;

            if (useColumnRenderers) {
                for (const auto& col : _columns) {
                    ImGui::TableNextColumn();
                    if (col._renderer) col._renderer(row);
                }
            } else if (rowRenderer) {
                rowRenderer(row);
            }

            // Detect right-click for context menu
            if (_rowContextMenu) {
                const ImVec2 mouse = ImGui::GetMousePos();
                const bool onRow = mouse.y >= rowTop && mouse.y < rowTop + rowH
                                && mouse.x >= windowPos.x
                                && mouse.x  < windowPos.x + windowWidth;
                if (onRow && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    _pendingContextRow = row;
                }
            }
        }
    }

    // Open context menu popup when a row was right-clicked this frame
    if (_rowContextMenu) {
        if (_pendingContextRow >= 0) {
            _activeContextRow = _pendingContextRow;
            ImGui::OpenPopup("##tbl_ctx");
        }
        if (ImGui::BeginPopup("##tbl_ctx")) {
            _rowContextMenu(_activeContextRow);
            ImGui::EndPopup();
        }
    }

    ImGui::EndTable();
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ─── TableWidget (Phase 29) ─────────────────────────────────────────────────────

Tree::Widget TableWidget::Build() const {
    using Tree::Primitives::Flex;
    using Tree::Primitives::GestureRegion;
    using Tree::Primitives::Text;
    using Tree::VirtualList;

    std::vector<Tree::Widget> headerCells;
    headerCells.reserve(_columns.size());
    for (const auto& col : _columns) {
        headerCells.push_back(Tree::Widget(Text(col.Label)));
    }
    Tree::Widget header = Tree::Widget(Flex(Flex::Axis::Horizontal).Gap(8.0f).Children(std::move(headerCells)));

    const TableWidget* self = this;
    Tree::Widget body = Tree::Widget(VirtualList(_rowCount, _rowHeight, [self](int rowIndex) -> Tree::Widget {
        std::vector<Tree::Widget> cells;
        cells.reserve(self->_columns.size());
        for (const auto& col : self->_columns) {
            std::string text = col.CellText ? col.CellText(rowIndex) : std::string{};
            cells.push_back(Tree::Widget(Text(std::move(text))));
        }
        Tree::Widget rowFlex = Tree::Widget(Flex(Flex::Axis::Horizontal).Gap(8.0f).Children(std::move(cells)));

        if (self->_onRowClick) {
            return Tree::Widget(
                GestureRegion().OnClick([self, rowIndex] { self->_onRowClick(rowIndex); }).Child(rowFlex));
        }
        return rowFlex;
    }));

    return Tree::Widget(Flex(Flex::Axis::Vertical).Children({header, body}));
}

} // namespace ImFrame::Widgets
