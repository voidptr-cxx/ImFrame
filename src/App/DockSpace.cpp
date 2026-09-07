/**
 * @file     DockSpace.cpp
 * @brief    Full-window dockspace and persistent layout management
 *
 * @internal
 * Implements DockSpace::Begin() / End(), the default three-column layout, and
 * the SaveLayout / LoadLayout / ResetLayout / ListLayouts API.
 *
 * Phase 30.3: the raw ImGui/DockBuilder calls this class used to make
 * directly now live in `src/Tree/RenderObjects/DockSpaceRO.hpp`/`.cpp`,
 * mirroring the `RootBridge` pattern already established for the widget
 * tree's own root window — this file no longer includes `<imgui.h>` at all.
 * Behavior is unchanged; only the ImGui call site moved.
 *
 * Layout persistence uses Utility::Config (TOML-subset) — all writes are
 * dispatched through Config::Set() which coalesces them asynchronously so the
 * render thread is never blocked by I/O.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/App/DockSpace.hpp"
#include "ImFrame/Utility/Config.hpp"
#include "Tree/RenderObjects/DockSpaceRO.hpp"

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
    // First frame: if Config has a saved default layout, restore it before the
    // dockspace is set up so ImGui can match saved node IDs.
    if (!_layoutRestored) {
        if (_config && !_defaultIni.empty()) {
            Internal::RestoreIniSettings(_defaultIni);
        }
        _layoutRestored = true;
    }

    const Internal::DockSpaceBeginInfo info =
        Internal::BeginDockSpaceWindow(_menuBar, "##DockSpace", "MainDockSpace");
    if (info.NeedsDefaultLayout) {
        InitDefaultLayout(info.Id);
    }
}

// ─── End ──────────────────────────────────────────────────────────────────────

void DockSpace::End() {
    Internal::EndDockSpaceWindow();
}

// ─── InitDefaultLayout ────────────────────────────────────────────────────────

void DockSpace::InitDefaultLayout(uint32_t dockspaceId) {
    _defaultIni = Internal::InitDefaultLayoutNodes(dockspaceId);

    if (_config) {
        _config->Set<std::string>("Layout.Default", _defaultIni);
    }
}

// ─── Layout management ────────────────────────────────────────────────────────

void DockSpace::SaveLayout(std::string_view name) {
    const std::string ini = Internal::CaptureIniSettings();
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
    Internal::RestoreIniSettings(ini);
}

void DockSpace::ResetLayout() {
    Internal::RestoreIniSettings(_defaultIni);
}

std::vector<std::string> DockSpace::ListLayouts() const {
    return _layoutNames;
}

} // namespace ImFrame::App
