/**
 * @file     DockSpace.hpp
 * @brief    Full-window dockspace RAII helper with default layout initialisation
 *
 * `DockSpace` encapsulates the boilerplate needed to create an ImGui full-screen
 * dockspace behind all other windows. It is owned by `Application` and driven
 * from `Application::RunOneFrame()` — application code never calls `Begin()`
 * or `End()` directly.
 *
 * On the first frame the dockspace has no nodes, `InitDefaultLayout()` is called
 * automatically to create a sensible three-column layout (sidebar 25%, centre
 * 50%, properties 25%). Phase 16 will replace this with a persistent layout
 * loaded from the user's config file.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <cstdint>

namespace ImFrame::App {

/**
 * @class    DockSpace
 * @brief    Full-window dockspace manager called once per frame by Application
 *
 * Creates an invisible full-screen window behind all other ImGui windows and
 * calls `ImGui::DockSpace()` so that panels can be docked into it.
 *
 * Optionally renders a menu bar area. When enabled, the user's `OnUi` callback
 * can call `ImGui::BeginMenuBar()` / `ImGui::EndMenuBar()` inside the dockspace
 * window to populate the menu bar.
 *
 * @note     `Begin()` and `End()` are called by `Application::RunOneFrame()`.
 *           Do not call them from application code.
 *
 * @since    0.8.0
 *
 * @example
 * @code
 * // Configured via Application builder:
 * app.WithMenuBar(true);
 * @endcode
 */
class DockSpace {
public:
    // ─── Configuration ────────────────────────────────────────────────────────

    /**
     * @brief    Enable or disable the menu bar area.
     *
     * When enabled, `Application` exposes a menu bar at the top of the dockspace
     * window that can be populated inside the `OnUi` callback.
     *
     * @param[in]  enabled  `true` to show the menu bar (default).
     * @return   Reference to this DockSpace for fluent chaining.
     */
    DockSpace& WithMenuBar(bool enabled = true) noexcept;

    // ─── Frame interface (called by Application::RunOneFrame) ─────────────────

    /**
     * @brief    Begin the dockspace for this frame.
     *
     * Sets up the full-screen background window, emits `ImGui::DockSpace()`,
     * and initialises the default layout if this is the first frame.
     *
     * @warning  Must be called inside an active ImGui frame (after BeginFrame()).
     */
    void Begin();

    /**
     * @brief    End the dockspace for this frame.
     *
     * Closes the full-screen background window opened by `Begin()`.
     *
     * @warning  Must be called after `Begin()` and before EndFrame().
     */
    void End();

private:
    bool _menuBar = false; ///< Whether to render a menu bar area.

    /**
     * @brief    Initialise the default three-column dockspace layout.
     *
     * Called automatically on the first frame when no dockspace nodes exist.
     * Creates: left sidebar (25%), centre panel (50%), right properties (25%).
     *
     * @param[in]  dockspaceId  ImGui dockspace node ID from `ImGui::GetID()`.
     */
    void InitDefaultLayout(uint32_t dockspaceId);
};

} // namespace ImFrame::App
