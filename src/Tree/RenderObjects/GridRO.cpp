/**
 * @file     GridRO.cpp
 * @brief    `GridWidget` primitive's concrete `Element` — pure-math N-column layout
 *
 * Unlike the deprecated `Layout::Grid` (backed by `ImGui::BeginTable`), this
 * layout is computed entirely in the `Layout()`/`Paint()` pass — no ImGui
 * table call is involved. Mirrors the precedent set when `HStack`/`VStack`
 * were replaced by the pure-layout-math `Flex` primitive (see `FlexRO.cpp`).
 *
 * Children fill columns left-to-right; a new row starts every `Columns()`
 * children. Each column is an equal share of the available width; each row's
 * height is the tallest child measured in that row.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-07
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Layout/Grid.hpp"
#include "../ElementInternal.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Layout::GridWidget;

class GridElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        Sync(widget);
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        Sync(newWidget);
    }

    void Unmount() override {
        for (auto& child : _children) { child->Unmount(); }
        _children.clear();
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        const int         columns = std::max(1, _config.GetColumns());
        const float        spacing = _config.GetSpacing();
        const std::size_t  n       = _children.size();
        const float mainAvail = constraints.MaxWidth;
        const float colWidth  = (mainAvail - spacing * static_cast<float>(columns - 1)) /
                                 static_cast<float>(columns);

        std::vector<Widgets::Vec2> childSizes(n);
        const BoxConstraints loose{0.0f, colWidth, 0.0f, std::numeric_limits<float>::max()};
        for (std::size_t i = 0; i < n; ++i) {
            childSizes[i] = _children[i]->Layout(loose);
        }

        const std::size_t rows = n == 0 ? 0 : (n + static_cast<std::size_t>(columns) - 1) /
                                                   static_cast<std::size_t>(columns);
        std::vector<float> rowHeights(rows, 0.0f);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t row = i / static_cast<std::size_t>(columns);
            rowHeights[row] = std::max(rowHeights[row], childSizes[i].y);
        }

        std::vector<float> rowYOffsets(rows, 0.0f);
        float y = 0.0f;
        for (std::size_t r = 0; r < rows; ++r) {
            rowYOffsets[r] = y;
            y += rowHeights[r] + spacing;
        }
        const float totalHeight = rows == 0 ? 0.0f : y - spacing;

        _offsets.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t row = i / static_cast<std::size_t>(columns);
            const std::size_t col = i % static_cast<std::size_t>(columns);
            _offsets[i] = {static_cast<float>(col) * (colWidth + spacing), rowYOffsets[row]};
        }

        _size = constraints.Constrain({mainAvail, totalHeight});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        for (std::size_t i = 0; i < _children.size(); ++i) {
            _children[i]->Paint(cmd, {position.x + _offsets[i].x, position.y + _offsets[i].y});
        }
    }

private:
    void Sync(const Widget& widget) {
        _config = widget.As<GridWidget>();
        ReconcileChildren(this, _children, _config.GetChildren());
    }

    GridWidget                             _config{1};
    std::vector<std::unique_ptr<Element>>  _children;
    std::vector<Widgets::Vec2>             _offsets;
};

} // namespace ImFrame::Internal

namespace ImFrame::Layout {

std::unique_ptr<Tree::Element> GridWidget::CreateElement() const {
    return std::make_unique<Internal::GridElement>();
}

} // namespace ImFrame::Layout
