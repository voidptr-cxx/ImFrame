/**
 * @file     DockSpaceRO.hpp
 * @brief    The sole ImGui touch-point for `App::DockSpace` and `App::DockSpaceWidget`
 *
 * Mirrors `RootBridge.hpp`'s shape exactly: free functions, not a `Tree::Element`
 * subclass, living in `src/Tree/RenderObjects/` so that `App::DockSpace` (stateful
 * layout-persistence infrastructure, not a per-frame declarative value — see
 * `DECISIONS.md`, Phase 30.3) never touches raw ImGui directly, closing the
 * "only `RenderObjects/` calls raw ImGui" gap `DockSpace.cpp` was the last
 * holdout on.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-09
 * @version  2.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ImFrame::Internal {

/// Result of opening the dockspace host window.
struct DockSpaceBeginInfo {
    std::uint32_t Id;                 ///< Dockspace node ID (`ImGui::GetID("MainDockSpace")`).
    bool          NeedsDefaultLayout; ///< `true` when no dock node exists yet for `Id`.
};

/**
 * @brief    Opens a full-viewport dockspace host window and emits `ImGui::DockSpace()`.
 *
 * `windowName`/`dockspaceIdName` must be unique per independent dockspace —
 * `App::DockSpace` (the automatic, `Application`-driven one) and
 * `App::DockSpaceWidget` (the opt-in tree primitive) pass different literals
 * so the two can never collide even though `Application::RunOneFrame()` runs
 * both in the same frame when a `SetRoot()` tree also uses `DockSpaceWidget`.
 *
 * @param[in] menuBar          Reserve a menu bar area in the host window.
 * @param[in] windowName       Unique ImGui window id/title for the host window.
 * @param[in] dockspaceIdName  Unique string passed to `ImGui::GetID()` for the dock node.
 * Must be paired with `EndDockSpaceWindow()`.
 */
[[nodiscard]] DockSpaceBeginInfo BeginDockSpaceWindow(bool menuBar, const char* windowName, const char* dockspaceIdName);

/// Closes the window opened by `BeginDockSpaceWindow()`.
void EndDockSpaceWindow();

/// Builds the compile-time default three-column layout for `dockspaceId`, returns the captured ini string.
[[nodiscard]] std::string InitDefaultLayoutNodes(std::uint32_t dockspaceId);

/// Captures the current ImGui ini settings (dock layout included) as a string.
[[nodiscard]] std::string CaptureIniSettings();

/// Restores ImGui ini settings previously captured by `CaptureIniSettings()`/`InitDefaultLayoutNodes()`.
void RestoreIniSettings(std::string_view ini);

} // namespace ImFrame::Internal
