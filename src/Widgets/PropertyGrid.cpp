/**
 * @file     PropertyGrid.cpp
 * @brief    Implementation of PropertyGridRow and PropertyGridWidget
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/PropertyGrid.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Widgets/Separator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

// ─── PropertyGridRow / PropertyGridWidget (Phase 30) ────────────────────────────

namespace ImFrame::Widgets {

PropertyGridRow::PropertyGridRow(std::string groupName, bool)
    : label(std::move(groupName)), widget(Tree::Primitives::Text("")), isSeparator(true) {}

PropertyGridRow PropertyGridRow::Separator(std::string groupName) {
    return PropertyGridRow(std::move(groupName), true);
}

Tree::Widget PropertyGridWidget::Build() const {
    using Tree::Primitives::Expanded;
    using Tree::Primitives::Flex;
    using Tree::Primitives::Text;

    // Split ratio can't be resolved to a fixed pixel width here — Build() runs
    // before any Layout() pass has constraint information. Express it as a
    // Flex factor pair instead; Flex's own DistributeFlexFactors() does the
    // proportional-width math at Layout() time. See DECISIONS.md (Phase 30.1).
    const int labelFactor = std::clamp(static_cast<int>(std::lround(_splitRatio * 100.0f)), 1, 99);
    const int valueFactor = 100 - labelFactor;

    std::vector<Tree::Widget> rowWidgets;
    rowWidgets.reserve(_rows.size());
    for (const auto& row : _rows) {
        if (row.isSeparator) {
            rowWidgets.emplace_back(SeparatorWidget().Label(row.label));
        } else {
            rowWidgets.emplace_back(Flex(Flex::Axis::Horizontal)
                .CrossAlign(Flex::CrossAlignment::Center)
                .Children({
                    Tree::Widget(Expanded(Tree::Widget(Text(row.label))).Factor(labelFactor)),
                    Tree::Widget(Expanded(row.widget).Factor(valueFactor)),
                }));
        }
    }

    return Tree::Widget(Flex(Flex::Axis::Vertical).Children(std::move(rowWidgets)));
}

} // namespace ImFrame::Widgets
