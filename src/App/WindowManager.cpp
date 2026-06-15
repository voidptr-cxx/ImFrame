/**
 * @file     WindowManager.cpp
 * @brief    Panel registry, visibility-bound menu rendering, and layout menu
 *
 * @internal
 * Implements WindowManager: entry registration, menu rendering via ImGui::BeginMenu,
 * RenderLayoutMenu (Layout submenu with saved layouts, inline save-as input, and
 * reset-to-default), and clear-on-shutdown.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/Window.hpp"
#include "ImFrame/App/DockSpace.hpp"

#include <imgui.h>

#include <cassert>
#include <string>

namespace ImFrame::App {

// ─── WindowManager ────────────────────────────────────────────────────────────

void WindowManager::Register(std::string_view name, bool* visible) {
    assert(visible != nullptr);
    _entries.push_back(Entry{ std::string(name), visible });
}

void WindowManager::RenderMenu(std::string_view menuTitle) {
    if (ImGui::BeginMenu(std::string(menuTitle).c_str())) {
        for (auto& entry : _entries) {
            ImGui::MenuItem(entry.name.c_str(), nullptr, entry.visible);
        }
        ImGui::EndMenu();
    }
}

void WindowManager::RenderLayoutMenu(DockSpace& dockSpace) {
    if (ImGui::BeginMenu("Layout")) {
        // One item per saved layout.
        for (const auto& name : dockSpace.ListLayouts()) {
            if (ImGui::MenuItem(name.c_str())) {
                dockSpace.LoadLayout(name);
            }
        }

        if (!dockSpace.ListLayouts().empty()) {
            ImGui::Separator();
        }

        // Inline save-as: InputText + Save button side by side.
        ImGui::SetNextItemWidth(140.0f);
        const bool submitted = ImGui::InputText("##SaveName", _saveLayoutBuf, sizeof(_saveLayoutBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Save") || submitted) && _saveLayoutBuf[0] != '\0') {
            dockSpace.SaveLayout(_saveLayoutBuf);
            _saveLayoutBuf[0] = '\0';
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Reset to Default")) {
            dockSpace.ResetLayout();
        }

        ImGui::EndMenu();
    }
}

void WindowManager::Clear() noexcept {
    _entries.clear();
}

// ─── WindowScope ──────────────────────────────────────────────────────────────

WindowScope::WindowScope(std::string_view title, bool* visible, int flags) {
    _open  = ImGui::Begin(std::string(title).c_str(),
                          visible,
                          static_cast<ImGuiWindowFlags>(flags));
    _valid = true;
}

WindowScope::~WindowScope() noexcept {
    if (_valid) {
        ImGui::End();
    }
}

WindowScope::WindowScope(WindowScope&& other) noexcept
    : _open(other._open)
    , _valid(other._valid)
{
    other._valid = false;
}

// ─── BeginWindow helper ───────────────────────────────────────────────────────

WindowScope BeginWindow(std::string_view title, bool* visible, int flags) {
    return WindowScope(title, visible, flags);
}

} // namespace ImFrame::App
