/**
 * @file     ExpandedRO.cpp
 * @brief    `Expanded` primitive's concrete `Element` — pass-through to its single child
 *
 * `Flex` reads each child widget's `FlexFactor()` directly (see `Widget.hpp`)
 * to decide main-axis sizing, then calls `Layout()`/`Paint()` on this element
 * like any other child — `Expanded` itself does no special-casing.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "../ElementInternal.hpp"

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Expanded;

class ExpandedElement final : public Element {
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
        if (_child) { _child->Unmount(); _child.reset(); }
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        _size = _child ? _child->Layout(constraints) : constraints.Constrain({0.0f, 0.0f});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        if (_child) { _child->Paint(cmd, position); }
    }

private:
    void Sync(const Widget& widget) {
        const Expanded& config      = widget.As<Expanded>();
        const Widget&    childWidget = config.GetChild();
        ReconcileChild(this, _child, &childWidget);
    }

    std::unique_ptr<Element> _child;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Expanded::CreateElement() const {
    return std::make_unique<Internal::ExpandedElement>();
}

} // namespace ImFrame::Tree::Primitives
