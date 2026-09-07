/**
 * @file     Table.cpp
 * @brief    Implementation of Widgets::ColumnDef and TableWidget's Build()
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Widgets/Table.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/VirtualList.hpp"

namespace ImFrame::Widgets {

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

// ─── TableWidget (Phase 29) ─────────────────────────────────────────────────────

Tree::Widget TableWidget::Build() const {
    using Tree::Primitives::Flex;
    using Tree::Primitives::GestureRegion;
    using Tree::Primitives::Text;
    using Tree::VirtualList;

    std::vector<Tree::Widget> headerCells;
    headerCells.reserve(_columns.size());
    for (const auto& col : _columns) {
        headerCells.push_back(Text(col.Label));
    }
    Tree::Widget header = Flex(Flex::Axis::Horizontal).Gap(8.0f).Children(std::move(headerCells));

    const TableWidget* self = this;
    Tree::Widget body = VirtualList(_rowCount, _rowHeight, [self](int rowIndex) -> Tree::Widget {
        std::vector<Tree::Widget> cells;
        cells.reserve(self->_columns.size());
        for (const auto& col : self->_columns) {
            std::string text = col.CellText ? col.CellText(rowIndex) : std::string{};
            cells.push_back(Text(std::move(text)));
        }
        Tree::Widget rowFlex = Flex(Flex::Axis::Horizontal).Gap(8.0f).Children(std::move(cells));

        if (self->_onRowClick) {
            return GestureRegion().OnClick([self, rowIndex] { self->_onRowClick(rowIndex); }).Child(rowFlex);
        }
        return rowFlex;
    });

    return Flex(Flex::Axis::Vertical).Children({header, body});
}

} // namespace ImFrame::Widgets
