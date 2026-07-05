/**
 * @file     ContextMenu.cpp
 * @brief    ContextMenu implementation wrapping ImGui context popup helpers
 *
 * @internal
 * `Show()` calls `BeginPopupContextItem()` (right-click on the last widget);
 * `ShowWindow()` calls `BeginPopupContextWindow()` (right-click anywhere in
 * the current window).  Both delegate to `RenderItems()` if the popup opens.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Overlay/ContextMenu.hpp"
#include "../Tree/ElementInternal.hpp"

#include <imgui.h>

namespace ImFrame::Overlay {

// MSVC's C4996 fires on the deprecated `ContextMenu`'s own out-of-line fluent
// setters below (their `ContextMenu&` return type counts as a "use" of the
// deprecated class, even in the class's own implementation) — suppressed here
// since this is the deprecated API's own continued implementation, not an
// external caller.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

ContextMenu::ContextMenu(std::string id) : _id(std::move(id)) {}

ContextMenu& ContextMenu::Item(std::string label, std::function<void()> action, bool enabled) {
    _items.push_back({ std::move(label), std::move(action), enabled, false });
    return *this;
}

ContextMenu& ContextMenu::Separator() {
    _items.push_back({ "", nullptr, true, true });
    return *this;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

void ContextMenu::RenderItems() {
    for (const auto& entry : _items) {
        if (entry.isSeparator) {
            ImGui::Separator();
        } else if (ImGui::MenuItem(entry.label.c_str(), nullptr, false, entry.enabled)) {
            if (entry.action) {
                entry.action();
            }
            ImGui::CloseCurrentPopup();
        }
    }
}

void ContextMenu::Show() {
    if (ImGui::BeginPopupContextItem(_id.c_str())) {
        RenderItems();
        ImGui::EndPopup();
    }
}

void ContextMenu::ShowWindow() {
    if (ImGui::BeginPopupContextWindow(_id.c_str())) {
        RenderItems();
        ImGui::EndPopup();
    }
}

} // namespace ImFrame::Overlay

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
