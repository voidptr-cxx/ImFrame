/**
 * @file     ThemeHotReload.hpp
 * @brief    Live theme editing via FileWatcher and TOML-described Theme overrides
 *
 * `ThemeHotReload` watches a directory for `.toml` file changes. When a change
 * is detected, the file is parsed into a `Theme` using `ThemeBuilder` and
 * stored as a pending theme. The caller retrieves it with `TakePending()` and
 * passes it to `Application::WithTheme()` to apply on the next frame.
 *
 * TOML format — each key matches a `Theme` field name and takes an RGBA array:
 * @code{.toml}
 * [colors]
 * accentDefault = [0.74, 0.58, 0.98, 1.0]
 * backgroundPrimary = [0.12, 0.12, 0.18, 1.0]
 * statusError = [0.90, 0.30, 0.30, 1.0]
 * @endcode
 *
 * Conditionally compiled: only present when `IMF_DEV_TOOLS` is defined.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/Theme/Theme.hpp"
#include "ImFrame/Utility/FileWatcher.hpp"
#include "ImFrame/Utility/Path.hpp"

#include <optional>

namespace ImFrame::DevTools {

/**
 * @class    ThemeHotReload
 * @brief    Watches a directory for TOML theme files and reloads the active theme on save
 *
 * Owns a `FileWatcher` on a configurable directory. When a `.toml` file in that
 * directory changes, it is parsed as a `Theme` override on top of the current
 * base theme and stored as a pending value. Call `TakePending()` once per frame;
 * if it returns a value, apply it via `Application::WithTheme()`.
 *
 * Call `Poll()` once per frame to dispatch FileWatcher events. This must be
 * called from the same thread that owns the `ThemeHotReload`.
 *
 * @since    1.7.0
 *
 * @example
 * @code
 * ImFrame::DevTools::ThemeHotReload hotReload;
 * hotReload.SetBaseTheme(ImFrame::Themes::Dracula);
 * hotReload.Watch(ImFrame::Utility::Path{"Assets/Themes"});
 *
 * app.OnUpdate([&](float) {
 *     hotReload.Poll();
 *     if (auto t = hotReload.TakePending()) {
 *         app.WithTheme(*t);
 *     }
 * });
 * @endcode
 */
class ThemeHotReload {
public:
    ThemeHotReload();
    ~ThemeHotReload();

    ThemeHotReload(const ThemeHotReload&)            = delete;
    ThemeHotReload& operator=(const ThemeHotReload&) = delete;

    /**
     * @brief    Sets the base theme that TOML overrides are applied on top of
     *
     * All subsequent file loads start from this base and apply only the keys
     * present in the TOML file. Unmentioned roles retain the base value.
     *
     * @param[in] base  Starting theme; copied into the hot-reload state.
     */
    void SetBaseTheme(const Theme::Theme& base) noexcept;

    /**
     * @brief    Begins watching `dir` for `.toml` file changes
     *
     * Replaces any previously active watch. If the directory does not exist
     * or the watch cannot be registered, the call is silently ignored.
     *
     * @param[in] dir  Directory path to monitor. Direct children only (non-recursive).
     */
    void Watch(const Utility::Path& dir);

    /**
     * @brief    Drains the FileWatcher event queue; call once per frame
     *
     * Must be called from the owner thread. On a `.toml` Created/Modified event
     * the file is read, parsed, and stored as the pending theme.
     */
    void Poll();

    /**
     * @brief    Returns and clears the pending theme, if one is available
     *
     * Returns `std::nullopt` when no theme file has changed since the last call.
     * After returning a value the pending state is reset; the next change will
     * populate it again.
     *
     * @return  The newly loaded theme, or `std::nullopt`.
     */
    [[nodiscard]] std::optional<Theme::Theme> TakePending() noexcept;

private:
    Theme::Theme               _base;
    Utility::FileWatcher       _watcher;
    Utility::WatchHandle       _watchHandle = Utility::InvalidWatchHandle;
    std::optional<Theme::Theme> _pending;

    void LoadFile(const Utility::Path& path);
};

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
