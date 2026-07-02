/**
 * @file     RootBridge.cpp
 * @brief    Implementation of the Reconciler's sole ImGui touch-point
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

#include "RootBridge.hpp"

#include <imgui.h>

namespace ImFrame::Internal {

RootWindowInfo BeginRootWindow() {
    // Without an explicit position/size, a fresh "##ImFrameTreeRoot" floats at
    // ImGui's auto-cascading window position with a small default size — pin
    // it to the main viewport's work area every frame instead.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

    ImGui::Begin("##ImFrameTreeRoot", nullptr,
                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    return {{avail.x, avail.y}, {pos.x, pos.y}};
}

void EndRootWindow() {
    ImGui::End();
}

} // namespace ImFrame::Internal
