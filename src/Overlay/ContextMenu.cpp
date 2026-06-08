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

#include <imgui.h>

namespace ImFrame::Overlay {

ContextMenu::ContextMenu(std::string id) : _id(std::move(id)) {}

ContextMenu& ContextMenu::Item(std::string label, std::function<void()> action, bool enabled) {
    _items.push_back({ std::move(label), std::move(action), enabled, false });
    return *this;
}

ContextMenu& ContextMenu::Separator() {
    _items.push_back({ "", nullptr, true, true });
    return *this;
}

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
