/**
 * @file     DockSpaceRO.cpp
 * @brief    Implementation of the DockSpace/DockSpaceWidget ImGui touch-points
 *
 * @internal
 * `DockBuilder*` APIs require `<imgui_internal.h>`, consistent with the
 * original `DockSpace.cpp` (Phase 7) before this extraction.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-09
 * @version  2.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "DockSpaceRO.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ImFrame::Internal {

DockSpaceBeginInfo BeginDockSpaceWindow(bool menuBar, const char* windowName, const char* dockspaceIdName) {
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

    if (menuBar) {
        windowFlags |= ImGuiWindowFlags_MenuBar;
    }

    ImGui::Begin(windowName, nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID id = ImGui::GetID(dockspaceIdName);
    ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    return {static_cast<std::uint32_t>(id), ImGui::DockBuilderGetNode(id) == nullptr};
}

void EndDockSpaceWindow() {
    ImGui::End();
}

std::string InitDefaultLayoutNodes(std::uint32_t dockspaceId) {
    const ImGuiID        id       = static_cast<ImGuiID>(dockspaceId);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, viewport->WorkSize);

    ImGuiID remaining    = id;
    ImGuiID sidebarId    = 0;
    ImGuiID propertiesId = 0;
    ImGuiID centerId     = 0;

    // Split off left sidebar — 25% of the total width.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.25f, &sidebarId, &remaining);

    // Split off right properties — 33% of the remaining width = 25% of total.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.333f, &propertiesId, &centerId);

    ImGui::DockBuilderFinish(id);

    return CaptureIniSettings();
}

std::string CaptureIniSettings() {
    size_t      len  = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&len);
    return std::string(data, len);
}

void RestoreIniSettings(std::string_view ini) {
    if (!ini.empty()) {
        ImGui::LoadIniSettingsFromMemory(ini.data(), ini.size());
    }
}

} // namespace ImFrame::Internal
