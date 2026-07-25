/**
 * @file     StackRO.cpp
 * @brief    `Stack` primitive's concrete `Element` — z-order layering, no layout negotiation
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

#include "ImFrame/Tree/Primitives/Stack.hpp"
#include "../ElementInternal.hpp"

#include <algorithm>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Stack;

class StackElement final : public Element {
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
        Widgets::Vec2 maxSize{constraints.MinWidth, constraints.MinHeight};
        for (auto& child : _children) {
            const Widgets::Vec2 childSize = child->Layout(constraints);
            maxSize.x = std::max(maxSize.x, childSize.x);
            maxSize.y = std::max(maxSize.y, childSize.y);
        }
        _size = constraints.Constrain(maxSize);
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        for (auto& child : _children) { child->Paint(cmd, position); }
    }

private:
    void Sync(const Widget& widget) {
        _config = widget.As<Stack>();
        ReconcileChildren(this, _children, _config.GetChildren());
    }

    Stack                                 _config;
    std::vector<std::unique_ptr<Element>> _children;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Stack::CreateElement() const {
    return std::make_unique<Internal::StackElement>();
}

} // namespace ImFrame::Tree::Primitives
