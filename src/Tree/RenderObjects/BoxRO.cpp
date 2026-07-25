/**
 * @file     BoxRO.cpp
 * @brief    `Box` primitive's concrete `Element` — sizing, padding, fill, and border
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

#include "ImFrame/Tree/Primitives/Box.hpp"
#include "../ElementInternal.hpp"

#include <algorithm>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Box;

class BoxElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Box>();
        SyncChild();
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Box>();
        SyncChild();
    }

    void Unmount() override {
        if (_child) { _child->Unmount(); _child.reset(); }
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        BoxConstraints own = constraints;
        if (_config.GetWidth() >= 0.0f) {
            own.MinWidth = own.MaxWidth = constraints.ConstrainWidth(_config.GetWidth());
        }
        if (_config.GetHeight() >= 0.0f) {
            own.MinHeight = own.MaxHeight = constraints.ConstrainHeight(_config.GetHeight());
        }
        own.MinWidth  = std::max(own.MinWidth, _config.GetMinWidth());
        own.MaxWidth  = std::min(own.MaxWidth, _config.GetMaxWidth());
        own.MinHeight = std::max(own.MinHeight, _config.GetMinHeight());
        own.MaxHeight = std::min(own.MaxHeight, _config.GetMaxHeight());
        own.MaxWidth  = std::max(own.MaxWidth, own.MinWidth);
        own.MaxHeight = std::max(own.MaxHeight, own.MinHeight);

        const Widgets::EdgeInsets padding = _config.GetPadding();
        BoxConstraints childConstraints{
            std::max(0.0f, own.MinWidth  - padding.Left - padding.Right),
            std::max(0.0f, own.MaxWidth  - padding.Left - padding.Right),
            std::max(0.0f, own.MinHeight - padding.Top  - padding.Bottom),
            std::max(0.0f, own.MaxHeight - padding.Top  - padding.Bottom),
        };

        Widgets::Vec2 contentSize{0.0f, 0.0f};
        if (_child) { contentSize = _child->Layout(childConstraints); }

        const Widgets::Vec2 outerSize{
            contentSize.x + padding.Left + padding.Right,
            contentSize.y + padding.Top + padding.Bottom
        };
        _size = own.Constrain(outerSize);
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        const Widgets::Vec4 bg     = _config.GetBackground();
        const Widgets::Vec4 border = _config.GetBorderColor();
        if (bg.w > 0.0f || (border.w > 0.0f && _config.GetBorderWidth() > 0.0f)) {
            cmd.Push(Rendering::DrawRect{
                .Position    = position,
                .Size        = _size,
                .Radii       = Rendering::CornerRadii::All(_config.GetRadius()),
                .FillColor   = bg,
                .StrokeColor = border,
                .StrokeWidth = _config.GetBorderWidth(),
            });
        }

        if (_child) {
            const Widgets::EdgeInsets padding = _config.GetPadding();
            _child->Paint(cmd, {position.x + padding.Left, position.y + padding.Top});
        }
    }

private:
    void SyncChild() {
        const Widget* childWidget = _config.GetChild() ? &*_config.GetChild() : nullptr;
        ReconcileChild(this, _child, childWidget);
    }

    Box                      _config;
    std::unique_ptr<Element> _child;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Box::CreateElement() const {
    return std::make_unique<Internal::BoxElement>();
}

} // namespace ImFrame::Tree::Primitives
