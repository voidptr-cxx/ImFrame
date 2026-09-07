/**
 * @file     SpacerRO.cpp
 * @brief    `Spacer` primitive's concrete `Element` — empty leaf, sized entirely by `Flex`
 *
 * `Spacer::GetFactor()` always returns `1` (see `Spacer.hpp`), so `Flex`
 * allocates it a share of leftover main-axis space exactly like a factor-1
 * `Expanded`. This element has no child and draws nothing.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Tree/Primitives/Spacer.hpp"

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Spacer;

class SpacerElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
    }

    void Update(const Widget& newWidget) override { RecordWidgetMeta(newWidget); }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        _size = constraints.Constrain({0.0f, 0.0f});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 /*position*/) override {}
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Spacer::CreateElement() const {
    return std::make_unique<Internal::SpacerElement>();
}

} // namespace ImFrame::Tree::Primitives
