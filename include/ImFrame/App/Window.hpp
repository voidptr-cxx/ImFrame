/**
 * @file     Window.hpp
 * @brief    WindowManager registry and WindowScope RAII helper for ImGui panels
 *
 * Provides two facilities used by application code to manage UI panels:
 *
 * - `WindowManager` — a registry of named panels with `bool*` visibility flags.
 *   `RenderMenu()` emits a checked menu item per panel for show/hide control.
 *   `RenderLayoutMenu()` adds a "Layout" submenu backed by `DockSpace` for
 *   saving, loading, and resetting named dockspace layouts.
 *
 * - `WindowScope` / `BeginWindow()` — an RAII scope guard that wraps
 *   `ImGui::Begin()` / `ImGui::End()`. Construct it with an `if` statement to
 *   skip rendering when the window is collapsed or invisible.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ImFrame::App {

class DockSpace; // forward declaration for RenderLayoutMenu parameter

// ─── WindowManager ────────────────────────────────────────────────────────────

/**
 * @class    WindowManager
 * @brief    Registry of named UI panels with visibility flag binding
 *
 * Panels call `Register()` during initialisation, passing their display name and
 * a pointer to their local `bool` visibility flag. `RenderMenu()` then renders a
 * `BeginMenu` / `EndMenu` block with a checked `MenuItem` per registered panel,
 * directly toggling the underlying flag.
 *
 * `WindowManager` does not create or own ImGui windows — the visibility flag is
 * the only state it touches. `Clear()` is called by `Application` on shutdown.
 *
 * Access is through `Application::GetWindowManager()`.
 *
 * @note     The `bool*` pointers must remain valid until `Clear()` is called.
 *           Typically panels are stack or member variables whose lifetime equals
 *           the Application's lifetime.
 *
 * @since    0.8.0
 *
 * @example
 * @code
 * bool showProperties = true;
 * app.GetWindowManager().Register("Properties", &showProperties);
 * // In OnUi:
 * if (showProperties) {
 *     if (auto w = ImFrame::App::BeginWindow("Properties", &showProperties)) {
 *         // render panel contents
 *     }
 * }
 * @endcode
 */
class WindowManager {
public:
    /**
     * @brief    Register a panel with a display name and a visibility flag pointer.
     *
     * @param[in]  name     Display name shown in the menu item.
     * @param[in]  visible  Pointer to the panel's visibility flag. Must not be null.
     *                      Must remain valid until `Clear()` is called.
     */
    void Register(std::string_view name, bool* visible);

    /**
     * @brief    Render a `BeginMenu` block containing one checked menu item per
     *           registered panel.
     *
     * Call this from inside `ImGui::BeginMenuBar()` / `ImGui::EndMenuBar()`.
     * Each item is a `ImGui::MenuItem` with a checkmark bound to the panel's
     * visibility flag.
     *
     * @param[in]  menuTitle  Title of the top-level menu (e.g. `"View"`).
     */
    void RenderMenu(std::string_view menuTitle);

    /**
     * @brief    Render a `"Layout"` submenu for saving, loading, and resetting
     *           named dockspace layouts.
     *
     * Call this from inside `ImGui::BeginMenuBar()` / `ImGui::EndMenuBar()`.
     * The submenu contains:
     * - One `MenuItem` per layout saved via `DockSpace::SaveLayout()`.
     * - An inline InputText + Save button for saving the current layout under
     *   a new name.
     * - A `"Reset to Default"` item that calls `DockSpace::ResetLayout()`.
     *
     * @param[in]  dockSpace  The dockspace whose layout is managed.
     */
    void RenderLayoutMenu(DockSpace& dockSpace);

    /**
     * @brief    Remove all registered panels.
     *
     * Called automatically by `Application` during shutdown. The `bool*` pointers
     * held by this manager are not accessed after `Clear()` returns.
     */
    void Clear() noexcept;

private:
    struct Entry {
        std::string name;    ///< Display name.
        bool*       visible; ///< Non-owning pointer to the panel's visibility flag.
    };

    std::vector<Entry> _entries;
    char _saveLayoutBuf[128] = {}; ///< InputText buffer for the "save as" field.
};

// ─── WindowScope ──────────────────────────────────────────────────────────────

/**
 * @class    WindowScope
 * @brief    RAII scope guard that wraps `ImGui::Begin()` / `ImGui::End()`
 *
 * The constructor calls `ImGui::Begin()`; the destructor calls `ImGui::End()`.
 * `ImGui::End()` is always called regardless of whether the window is visible,
 * matching ImGui's contract that every `Begin()` must be paired with an `End()`.
 *
 * Use with an `if` statement to skip rendering when the window is collapsed:
 * @code
 * if (auto w = BeginWindow("My Panel", &visible)) {
 *     ImGui::Text("Only rendered when visible and not collapsed.");
 * }
 * @endcode
 *
 * `flags` maps directly to `ImGuiWindowFlags` via `static_cast`. Pass `0` for
 * the default flags.
 *
 * @note     Non-copyable. Moveable so that `BeginWindow()` can return by value.
 *
 * @since    0.8.0
 */
class WindowScope {
public:
    /**
     * @brief    Begin an ImGui window.
     *
     * @param[in]  title    Window title and ImGui ID.
     * @param[in]  visible  Optional pointer to a visibility flag. When non-null
     *                      and `*visible` is false the window is not shown.
     *                      ImGui writes `false` here when the user clicks the
     *                      close button (if `ImGuiWindowFlags_NoCollapse` is not set).
     * @param[in]  flags    `ImGuiWindowFlags` bitmask cast to `int`. Pass `0` for defaults.
     */
    explicit WindowScope(std::string_view title, bool* visible = nullptr, int flags = 0);

    /// @brief  Calls `ImGui::End()` unless this scope was moved from.
    ~WindowScope();

    WindowScope(const WindowScope&)            = delete;
    WindowScope& operator=(const WindowScope&) = delete;

    /// @brief  Move constructor — transfers ownership, leaving moved-from scope inert.
    WindowScope(WindowScope&& other) noexcept;
    WindowScope& operator=(WindowScope&&)      = delete;

    /// @return  `true` if the window is visible and not collapsed.
    [[nodiscard]] bool IsOpen() const noexcept { return _open; }

    /// @return  `true` if the window is visible and not collapsed (same as `IsOpen()`).
    explicit operator bool() const noexcept { return _open; }

private:
    bool _open  = false; ///< Return value of ImGui::Begin().
    bool _valid = false; ///< False in moved-from state — skips ImGui::End().
};

// ─── BeginWindow helper ───────────────────────────────────────────────────────

/**
 * @brief    Factory function that constructs a `WindowScope`.
 *
 * Prefer this over constructing `WindowScope` directly:
 * @code
 * if (auto w = BeginWindow("Properties", &visible)) { ... }
 * @endcode
 *
 * @param[in]  title    Window title and ImGui ID.
 * @param[in]  visible  Optional pointer to visibility flag.
 * @param[in]  flags    `ImGuiWindowFlags` bitmask as `int`.
 * @return   A `WindowScope` RAII guard.
 */
[[nodiscard]] WindowScope BeginWindow(std::string_view title,
                                      bool*            visible = nullptr,
                                      int              flags   = 0);

} // namespace ImFrame::App
