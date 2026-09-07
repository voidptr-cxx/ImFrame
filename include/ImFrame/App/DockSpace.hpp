/**
 * @file     DockSpace.hpp
 * @brief    Full-window dockspace manager with persistent named layout support
 *
 * `DockSpace` encapsulates the boilerplate needed to create an ImGui full-screen
 * dockspace behind all other windows. It is owned by `Application` and driven
 * from `Application::RunOneFrame()` — application code never calls `Begin()`
 * or `End()` directly.
 *
 * On the first frame, if a default layout is stored in the Config, it is
 * restored via `ImGui::LoadIniSettingsFromMemory()`. Otherwise `InitDefaultLayout()`
 * runs and the result is saved to Config under `"Layout.Default"`.
 *
 * Layout saves and loads are always done through `Utility::Config::Set()` —
 * async writes, never blocking the render thread.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ImFrame::Utility { class Config; }

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
 * Named layouts can be saved, loaded, and reset via `SaveLayout()` / `LoadLayout()`
 * / `ResetLayout()`. All persistence is delegated to a `Utility::Config` instance
 * provided by `Application` — layout writes are async and never block the render
 * thread.
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
 *
 * // In OnUi callback:
 * app.GetDockSpace().SaveLayout("My Layout");
 * app.GetDockSpace().LoadLayout("My Layout");
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

    /**
     * @brief    Wire a Config instance for layout persistence.
     *
     * Called once by `Application::Run()` before the render loop starts.
     * Reads any existing default and layout names from Config so that
     * `ResetLayout()` and `ListLayouts()` work correctly on the first frame.
     *
     * @param[in]  config  Non-owning pointer to the Application-owned Config.
     *                     Must remain valid for the lifetime of the render loop.
     *                     Passing `nullptr` disables layout persistence.
     */
    void SetConfig(Utility::Config* config);

    // ─── Frame interface (called by Application::RunOneFrame) ─────────────────

    /**
     * @brief    Begin the dockspace for this frame.
     *
     * On the first call, attempts to restore the default layout from Config.
     * If no saved layout exists, `InitDefaultLayout()` runs and captures the
     * resulting ini string for future `ResetLayout()` calls.
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

    // ─── Layout management ────────────────────────────────────────────────────

    /**
     * @brief    Capture the current dockspace state and save it under `name`.
     *
     * Calls `ImGui::SaveIniSettingsToMemory()` and stores the result in Config
     * under the key `"Layout.<name>"`. The name is added to the layout index if
     * not already present. Must be called on the render thread inside an active
     * ImGui frame.
     *
     * @param[in]  name  User-defined layout name. Must not be `"Default"` or
     *                   `"_index"` (those are reserved keys).
     */
    void SaveLayout(std::string_view name);

    /**
     * @brief    Restore the dockspace state previously saved under `name`.
     *
     * Calls `ImGui::LoadIniSettingsFromMemory()` with the stored ini string.
     * If no layout with `name` exists this is a no-op. Must be called on the
     * render thread inside an active ImGui frame.
     *
     * @param[in]  name  Name of a layout previously passed to `SaveLayout()`.
     */
    void LoadLayout(std::string_view name);

    /**
     * @brief    Restore the compile-time default three-column layout.
     *
     * Reloads the ini string captured when `InitDefaultLayout()` first ran.
     * If the default has not yet been captured (first frame not yet rendered)
     * this is a no-op. Must be called on the render thread inside an active
     * ImGui frame.
     */
    void ResetLayout();

    /**
     * @brief    Return the names of all layouts previously saved via `SaveLayout()`.
     *
     * The returned vector contains the names in insertion order. `"Default"` is
     * not included — use `ResetLayout()` to restore it.
     *
     * @return   Vector of layout name strings. May be empty.
     */
    [[nodiscard]] std::vector<std::string> ListLayouts() const;

private:
    bool                     _menuBar        = false; ///< Whether to render a menu bar area.
    Utility::Config*         _config         = nullptr; ///< Non-owning; null = no persistence.
    bool                     _layoutRestored = false;   ///< True after first-frame ini load attempt.
    std::string              _defaultIni;               ///< Captured after InitDefaultLayout.
    std::vector<std::string> _layoutNames;              ///< Names of user-saved layouts.

    /**
     * @brief    Initialise the default three-column dockspace layout.
     *
     * Called automatically on the first frame when no dockspace nodes exist.
     * Creates: left sidebar (25%), centre panel (50%), right properties (25%).
     * Captures the resulting ini string into `_defaultIni` and saves it to
     * Config under `"Layout.Default"`.
     *
     * @param[in]  dockspaceId  ImGui dockspace node ID from `ImGui::GetID()`.
     */
    void InitDefaultLayout(uint32_t dockspaceId);
};

} // namespace ImFrame::App
