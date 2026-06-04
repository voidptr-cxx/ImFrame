/**
 * @file     DockSpace.cpp
 * @brief    Full-window dockspace and default layout initialisation
 *
 * @internal
 * Implements DockSpace::Begin() / End() and the default three-column layout.
 * DockBuilder APIs require <imgui_internal.h>.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/DockSpace.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ImFrame::App {

// ─── Configuration ────────────────────────────────────────────────────────────

DockSpace& DockSpace::WithMenuBar(bool enabled) noexcept {
    _menuBar = enabled;
    return *this;
}

// ─── Begin ────────────────────────────────────────────────────────────────────

void DockSpace::Begin() {
    // Cover the entire main viewport with an invisible, non-interactive window.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus          |
        ImGuiWindowFlags_NoBackground;

    if (_menuBar) {
        windowFlags |= ImGuiWindowFlags_MenuBar;
    }

    ImGui::Begin("##DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // Emit the dockspace and initialise the default layout on the very first frame.
    const ImGuiID id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (ImGui::DockBuilderGetNode(id) == nullptr) {
        InitDefaultLayout(id);
    }
}

// ─── End ──────────────────────────────────────────────────────────────────────

void DockSpace::End() {
    ImGui::End();
}

// ─── InitDefaultLayout ────────────────────────────────────────────────────────

void DockSpace::InitDefaultLayout(uint32_t dockspaceId) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID sidebarId    = 0;
    ImGuiID propertiesId = 0;
    ImGuiID centerId     = 0;
    ImGuiID remaining    = dockspaceId;

    // Split off left sidebar — 25% of the total width.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.25f, &sidebarId, &remaining);

    // Split off right properties — 33% of the remaining width = 25% of total.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.333f, &propertiesId, &centerId);

    ImGui::DockBuilderFinish(dockspaceId);
}

} // namespace ImFrame::App
