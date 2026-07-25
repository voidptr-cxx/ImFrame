/**
 * @file     SizedBoxRO.cpp
 * @brief    `SizedBox` primitive's concrete `Element` — reserves space, draws nothing
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

#include "ImFrame/Tree/Primitives/SizedBox.hpp"

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::SizedBox;

class SizedBoxElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<SizedBox>();
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<SizedBox>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        _size = constraints.Constrain({_config.GetWidth(), _config.GetHeight()});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 /*position*/) override {}

private:
    SizedBox _config;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> SizedBox::CreateElement() const {
    return std::make_unique<Internal::SizedBoxElement>();
}

} // namespace ImFrame::Tree::Primitives
