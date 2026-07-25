/**
 * @file     FlexRO.cpp
 * @brief    `Flex` primitive's concrete `Element` — two-pass flexbox-style layout
 *
 * Pass 1 measures non-flexible children (loose on the main axis). The
 * remaining main-axis space is split among flexible children
 * (`Expanded`/`Spacer`, detected via `Widget::FlexFactor()`) proportionally
 * to their factor, via `DistributeFlexFactors()`. Pass 2 re-measures
 * flexible children with a tight main-axis constraint. Offsets are then
 * computed from the final sizes via `ComputeMainAxisOffsets()` /
 * `ComputeCrossAxisOffset()`.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "../ElementInternal.hpp"
#include "../Layout.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Flex;

namespace {

FlexMainAlign ToInternalMainAlign(Flex::MainAlignment a) noexcept {
    switch (a) {
    case Flex::MainAlignment::Start:        return FlexMainAlign::Start;
    case Flex::MainAlignment::Center:       return FlexMainAlign::Center;
    case Flex::MainAlignment::End:          return FlexMainAlign::End;
    case Flex::MainAlignment::SpaceBetween: return FlexMainAlign::SpaceBetween;
    case Flex::MainAlignment::SpaceAround:  return FlexMainAlign::SpaceAround;
    }
    return FlexMainAlign::Start;
}

FlexCrossAlign ToInternalCrossAlign(Flex::CrossAlignment a) noexcept {
    switch (a) {
    case Flex::CrossAlignment::Start:   return FlexCrossAlign::Start;
    case Flex::CrossAlignment::Center:  return FlexCrossAlign::Center;
    case Flex::CrossAlignment::End:     return FlexCrossAlign::End;
    case Flex::CrossAlignment::Stretch: return FlexCrossAlign::Stretch;
    }
    return FlexCrossAlign::Start;
}

} // namespace

class FlexElement final : public Element {
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
        const bool  horizontal = _config.GetAxis() == Flex::Axis::Horizontal;
        const float mainAvail  = horizontal ? constraints.MaxWidth : constraints.MaxHeight;
        const float crossAvail = horizontal ? constraints.MaxHeight : constraints.MaxWidth;
        const auto& widgets    = _config.GetChildren();
        const std::size_t n    = _children.size();

        std::vector<int>           factors(n, 0);
        std::vector<float>         fixedMain(n, 0.0f);
        std::vector<Widgets::Vec2> childSizes(n);

        for (std::size_t i = 0; i < n; ++i) {
            factors[i] = widgets[i].FlexFactor();
            if (factors[i] == 0) {
                const BoxConstraints loose = horizontal
                    ? BoxConstraints{0.0f, std::numeric_limits<float>::max(), 0.0f, crossAvail}
                    : BoxConstraints{0.0f, crossAvail, 0.0f, std::numeric_limits<float>::max()};
                childSizes[i] = _children[i]->Layout(loose);
                fixedMain[i]  = horizontal ? childSizes[i].x : childSizes[i].y;
            }
        }

        const std::vector<float> mainSizes = DistributeFlexFactors(mainAvail, _config.GetGap(), factors, fixedMain);

        for (std::size_t i = 0; i < n; ++i) {
            if (factors[i] > 0) {
                const BoxConstraints tight = horizontal
                    ? BoxConstraints{mainSizes[i], mainSizes[i], 0.0f, crossAvail}
                    : BoxConstraints{0.0f, crossAvail, mainSizes[i], mainSizes[i]};
                childSizes[i] = _children[i]->Layout(tight);
            }
        }

        float crossSize = 0.0f;
        for (std::size_t i = 0; i < n; ++i) {
            crossSize = std::max(crossSize, horizontal ? childSizes[i].y : childSizes[i].x);
        }

        const FlexCrossAlign crossAlign = ToInternalCrossAlign(_config.GetCrossAlign());
        if (crossAlign == FlexCrossAlign::Stretch) {
            for (std::size_t i = 0; i < n; ++i) {
                const BoxConstraints stretched = horizontal
                    ? BoxConstraints{mainSizes[i], mainSizes[i], crossSize, crossSize}
                    : BoxConstraints{crossSize, crossSize, mainSizes[i], mainSizes[i]};
                childSizes[i] = _children[i]->Layout(stretched);
            }
        }

        const FlexMainAlign      mainAlign   = ToInternalMainAlign(_config.GetMainAlign());
        const std::vector<float> mainOffsets = ComputeMainAxisOffsets(mainAlign, mainAvail, _config.GetGap(), mainSizes);

        _offsets.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const float childCross  = horizontal ? childSizes[i].y : childSizes[i].x;
            const float crossOffset = ComputeCrossAxisOffset(crossAlign, crossSize, childCross);
            _offsets[i] = horizontal ? Widgets::Vec2{mainOffsets[i], crossOffset}
                                     : Widgets::Vec2{crossOffset, mainOffsets[i]};
        }

        const float totalMain = mainSizes.empty()
            ? 0.0f
            : std::accumulate(mainSizes.begin(), mainSizes.end(), 0.0f) +
                  (n > 1 ? _config.GetGap() * static_cast<float>(n - 1) : 0.0f);

        const Widgets::Vec2 ownSize = horizontal ? Widgets::Vec2{totalMain, crossSize}
                                                  : Widgets::Vec2{crossSize, totalMain};
        _size = constraints.Constrain(ownSize);
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        for (std::size_t i = 0; i < _children.size(); ++i) {
            _children[i]->Paint(cmd, {position.x + _offsets[i].x, position.y + _offsets[i].y});
        }
    }

private:
    void Sync(const Widget& widget) {
        _config = widget.As<Flex>();
        ReconcileChildren(this, _children, _config.GetChildren());
    }

    Flex                                   _config{Flex::Axis::Horizontal};
    std::vector<std::unique_ptr<Element>>  _children;
    std::vector<Widgets::Vec2>             _offsets;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Flex::CreateElement() const {
    return std::make_unique<Internal::FlexElement>();
}

} // namespace ImFrame::Tree::Primitives
