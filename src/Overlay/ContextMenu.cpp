/**
 * @file     ContextMenu.cpp
 * @brief    ContextMenuElement — `ContextMenuWidget`'s concrete `Element`
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Overlay/ContextMenu.hpp"
#include "../Tree/ElementInternal.hpp"

#include <imgui.h>

// ─── ContextMenuWidget / ContextMenuElement (Phase 29) ──────────────────────────

namespace ImFrame::Internal {

class ContextMenuElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        Sync(widget);
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        Sync(newWidget);
    }

    void Unmount() override {
        if (_child) { _child->Unmount(); }
        _child.reset();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = _child ? _child->Layout(constraints) : Widgets::Vec2{};
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        if (_child) { _child->Paint(position); }

        ImGui::PushID(this);
        if (ImGui::BeginPopupContextItem("##ctx")) {
            for (const auto& entry : _items) {
                if (entry.IsSeparator) {
                    ImGui::Separator();
                } else if (ImGui::MenuItem(entry.Label.c_str(), nullptr, false, entry.Enabled)) {
                    if (entry.Action) { entry.Action(); }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

private:
    void Sync(const Tree::Widget& widget) {
        const auto& config = widget.As<Overlay::ContextMenuWidget>();
        _items              = config.GetItems();
        const Tree::Widget* childWidget = &config.GetChild();
        ReconcileChild(this, _child, childWidget);
    }

    std::vector<Overlay::ContextMenuEntry> _items;
    std::unique_ptr<Tree::Element>         _child;
};

} // namespace ImFrame::Internal

namespace ImFrame::Overlay {

std::unique_ptr<Tree::Element> ContextMenuWidget::CreateElement() const {
    return std::make_unique<Internal::ContextMenuElement>();
}

} // namespace ImFrame::Overlay
