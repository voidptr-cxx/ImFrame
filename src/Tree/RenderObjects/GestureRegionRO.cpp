/**
 * @file     GestureRegionRO.cpp
 * @brief    `GestureRegion` primitive's concrete `Element` — InvisibleButton hit-testing
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "../ElementInternal.hpp"

#include <imgui.h>

#include <algorithm>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::GestureRegion;

class GestureRegionElement final : public Element {
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
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);
        ImGui::InvisibleButton("##gesture", ImVec2{std::max(_size.x, 1.0f), std::max(_size.y, 1.0f)});
        ImGui::PopID();

        const bool hovered = ImGui::IsItemHovered();
        if (hovered != _wasHovered) {
            if (_config.GetOnHover()) { _config.GetOnHover()(hovered); }
            _wasHovered = hovered;
        }

        if (ImGui::IsItemClicked()) {
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (_config.GetOnDoubleClick()) { _config.GetOnDoubleClick()(); }
            } else if (_config.GetOnClick()) {
                _config.GetOnClick()();
            }
        }

        const bool active = ImGui::IsItemActive();
        if (active && !_wasActive) {
            if (_config.GetOnDragStart()) { _config.GetOnDragStart()(); }
        }
        if (active) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            if ((delta.x != 0.0f || delta.y != 0.0f) && _config.GetOnDragMove()) {
                _config.GetOnDragMove()({delta.x, delta.y});
            }
        }
        if (!active && _wasActive) {
            if (_config.GetOnDragEnd()) { _config.GetOnDragEnd()(); }
        }
        _wasActive = active;

        if (hovered) {
            const ImVec2 wheel = {ImGui::GetIO().MouseWheelH, ImGui::GetIO().MouseWheel};
            if ((wheel.x != 0.0f || wheel.y != 0.0f) && _config.GetOnScroll()) {
                _config.GetOnScroll()({wheel.x, wheel.y});
            }
        }

        if (_child) { _child->Paint(cmd, position); }
    }

private:
    void Sync(const Widget& widget) {
        _config                    = widget.As<GestureRegion>();
        const Widget* childWidget = _config.GetChild() ? &*_config.GetChild() : nullptr;
        ReconcileChild(this, _child, childWidget);
    }

    GestureRegion             _config;
    std::unique_ptr<Element> _child;
    bool                      _wasHovered = false;
    bool                      _wasActive  = false;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> GestureRegion::CreateElement() const {
    return std::make_unique<Internal::GestureRegionElement>();
}

} // namespace ImFrame::Tree::Primitives
