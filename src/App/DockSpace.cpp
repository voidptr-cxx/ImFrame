/**
 * @file     DockSpace.cpp
 * @brief    Full-window dockspace and persistent layout management
 *
 * @internal
 * Implements DockSpace::Begin() / End(), the default three-column layout, and
 * the SaveLayout / LoadLayout / ResetLayout / ListLayouts API.
 * DockBuilder APIs require <imgui_internal.h>.
 *
 * Layout persistence uses Utility::Config (TOML-subset) — all writes are
 * dispatched through Config::Set() which coalesces them asynchronously so the
 * render thread is never blocked by I/O.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/DockSpace.hpp"
#include "ImFrame/Utility/Config.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace ImFrame::App {

// ─── Configuration ────────────────────────────────────────────────────────────

DockSpace& DockSpace::WithMenuBar(bool enabled) noexcept {
    _menuBar = enabled;
    return *this;
}

void DockSpace::SetConfig(Utility::Config* config) {
    _config = config;
    if (!_config) return;

    // Pre-load the default ini so ResetLayout() works even before InitDefaultLayout runs.
    _defaultIni = _config->Get<std::string>("Layout.Default", "");

    // Parse the semicolon-separated layout name index.
    const std::string index = _config->Get<std::string>("Layout._index", "");
    if (!index.empty()) {
        std::string_view sv(index);
        while (!sv.empty()) {
            const auto pos = sv.find(';');
            if (pos == std::string_view::npos) {
                _layoutNames.push_back(std::string(sv));
                break;
            }
            _layoutNames.push_back(std::string(sv.substr(0, pos)));
            sv.remove_prefix(pos + 1);
        }
    }
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

    // First frame: if Config has a saved default layout, restore it before the
    // dockspace is set up so ImGui can match saved node IDs.
    if (!_layoutRestored) {
        if (_config && !_defaultIni.empty()) {
            ImGui::LoadIniSettingsFromMemory(_defaultIni.c_str(), _defaultIni.size());
        }
        _layoutRestored = true;
    }

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

    ImGuiID remaining    = dockspaceId;
    ImGuiID sidebarId    = 0;
    ImGuiID propertiesId = 0;
    ImGuiID centerId     = 0;

    // Split off left sidebar — 25% of the total width.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.25f, &sidebarId, &remaining);

    // Split off right properties — 33% of the remaining width = 25% of total.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.333f, &propertiesId, &centerId);

    ImGui::DockBuilderFinish(dockspaceId);

    // Capture the resulting ini string so ResetLayout() can always restore it.
    size_t      iniLen  = 0;
    const char* iniData = ImGui::SaveIniSettingsToMemory(&iniLen);
    _defaultIni = std::string(iniData, iniLen);

    if (_config) {
        _config->Set<std::string>("Layout.Default", _defaultIni);
    }
}

// ─── Layout management ────────────────────────────────────────────────────────

void DockSpace::SaveLayout(std::string_view name) {
    size_t      len  = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&len);
    const std::string ini(data, len);
    const std::string key = std::string("Layout.") + std::string(name);

    // Add to the name index if not already present.
    const std::string nameStr(name);
    if (std::find(_layoutNames.begin(), _layoutNames.end(), nameStr) == _layoutNames.end()) {
        _layoutNames.push_back(nameStr);
        if (_config) {
            std::string index;
            for (const auto& n : _layoutNames) {
                if (!index.empty()) index += ';';
                index += n;
            }
            _config->Set<std::string>("Layout._index", index);
        }
    }

    if (_config) {
        _config->Set<std::string>(key, ini);
    }
}

void DockSpace::LoadLayout(std::string_view name) {
    if (!_config) return;
    const std::string key = std::string("Layout.") + std::string(name);
    const std::string ini = _config->Get<std::string>(key, "");
    if (!ini.empty()) {
        ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
    }
}

void DockSpace::ResetLayout() {
    if (!_defaultIni.empty()) {
        ImGui::LoadIniSettingsFromMemory(_defaultIni.c_str(), _defaultIni.size());
    }
}

std::vector<std::string> DockSpace::ListLayouts() const {
    return _layoutNames;
}

} // namespace ImFrame::App
