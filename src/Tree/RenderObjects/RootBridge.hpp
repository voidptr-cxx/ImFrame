/**
 * @file     RootBridge.hpp
 * @brief    The sole ImGui touch-point used by `Reconciler` to host and place the tree root
 *
 * `DockSpace::Begin()` calls `ImGui::DockSpace()`, which consumes the entire
 * host window's content region to register the dock node — nothing painted
 * directly afterward in that same window is inside its clip rect. The widget
 * tree therefore needs its own dockable `ImGui::Begin()/End()` window, exactly
 * like any other ImFrame `Window`.
 *
 * Kept inside `src/Tree/RenderObjects/` so `Reconciler.cpp` itself never calls
 * raw ImGui functions, preserving the "RenderObject implementations live
 * exclusively in `src/Tree/RenderObjects/`" invariant.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

namespace ImFrame::Internal {

/// Available size and cursor screen position inside the freshly opened root window.
struct RootWindowInfo {
    Widgets::Vec2 AvailableSize;
    Widgets::Vec2 CursorScreenPos;
};

/// Opens the dockable host window for the widget tree. Must be paired with `EndRootWindow()`.
[[nodiscard]] RootWindowInfo BeginRootWindow();

/// Closes the host window opened by `BeginRootWindow()`.
void EndRootWindow();

} // namespace ImFrame::Internal
