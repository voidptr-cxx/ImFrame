/**
 * @file     WindowManager.cpp
 * @brief    Panel registry and visibility-bound menu rendering
 *
 * @internal
 * Implements WindowManager: entry registration, menu rendering via ImGui::BeginMenu,
 * and clear-on-shutdown.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/Window.hpp"

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

WindowScope::~WindowScope() {
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
