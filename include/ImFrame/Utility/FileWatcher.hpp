/**
 * @file     FileWatcher.hpp
 * @brief    Cross-platform filesystem change notification watcher
 *
 * `FileWatcher` monitors one or more paths for changes. Changes are collected
 * on a background thread (inotify/kqueue/ReadDirectoryChangesW) and dispatched
 * synchronously on the calling thread via `Poll()`. This design avoids
 * threading surprises — `Poll()` is called once per frame, and the user receives
 * all accumulated events in a predictable, ordered manner.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"

namespace ImFrame::Utility {

// ─── FileChangeType ───────────────────────────────────────────────────────────

/**
 * @enum     FileChangeType
 * @brief    The nature of a filesystem change detected by FileWatcher
 * @since    0.3.0
 */
enum class FileChangeType {
    /// A new file or directory was created at the watched path.
    Created,

    /// An existing file was modified (write or attribute change).
    Modified,

    /// A file or directory was deleted.
    Deleted,

    /// A file or directory was renamed. `FileEvent::NewPath` holds the new name.
    Renamed,
};

// ─── FileEvent ────────────────────────────────────────────────────────────────

/**
 * @struct   FileEvent
 * @brief    Describes a single filesystem change detected by FileWatcher
 * @since    0.3.0
 */
struct FileEvent {
    /// The affected path.
    Path ChangedPath;

    /// The nature of the change.
    FileChangeType Type = FileChangeType::Modified;

    /// Only valid when `Type == FileChangeType::Renamed`. Empty otherwise.
    Path NewPath;
};

// ─── WatchHandle ──────────────────────────────────────────────────────────────

/// @brief  Opaque handle returned by `FileWatcher::Watch()`.
using WatchHandle = uint64_t;

/// @brief  Sentinel value representing an invalid or unregistered watch.
inline constexpr WatchHandle InvalidWatchHandle = 0;

// ─── FileWatcher ──────────────────────────────────────────────────────────────

/**
 * @class    FileWatcher
 * @brief    Monitors filesystem paths for changes, dispatching events via Poll()
 *
 * A `FileWatcher` instance manages a background thread that listens for OS
 * filesystem events. Changes are collected into an internal queue and delivered
 * synchronously when `Poll()` is called.
 *
 * Typical usage (once per frame in the application loop):
 * ```cpp
 * uint32_t n = watcher.Poll([](const ImFrame::Utility::FileEvent& e) {
 *     if (e.Type == ImFrame::Utility::FileChangeType::Modified) {
 *         ReloadAsset(e.ChangedPath);
 *     }
 * });
 * ```
 *
 * Platform implementations:
 * - **Linux** — `inotify`
 * - **macOS / BSD** — `kqueue`
 * - **Windows** — `ReadDirectoryChangesW`
 *
 * @note     A `FileWatcher` is not copyable. Move is supported.
 *
 * @warning  `Watch()`, `Unwatch()`, and `Poll()` must all be called from the
 *           same thread (the thread that owns the FileWatcher). The background
 *           event-collection thread is internal and not accessible to callers.
 *
 * @since    0.3.0
 *
 * @see      FileEvent
 * @see      FileChangeType
 */
class FileWatcher {
public:
    /**
     * @brief  Constructs a FileWatcher and starts the background event thread.
     * @throws std::system_error  If the background thread could not be created.
     */
    FileWatcher();

    /**
     * @brief  Destructor. Stops the background thread and releases all watches.
     */
    ~FileWatcher();

    FileWatcher(const FileWatcher&)            = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&)                 noexcept;
    FileWatcher& operator=(FileWatcher&&)      noexcept;

    /**
     * @brief    Begins monitoring `path` for filesystem changes.
     *
     * The path must be an existing directory. Recursive monitoring is not
     * supported; only direct changes inside the watched directory are reported.
     *
     * @param[in]  path  An existing directory to monitor.
     *
     * @return   A non-zero `WatchHandle` on success, or `Error::WatchFailed`
     *           if the path does not exist or the OS cannot add the watch.
     *
     * @throws   Nothing.
     */
    [[nodiscard]] Result<WatchHandle> Watch(const Path& path);

    /**
     * @brief    Stops monitoring the path associated with `handle`.
     *
     * No-op if `handle` is `InvalidWatchHandle` or has already been removed.
     *
     * @param[in]  handle  Handle returned by a prior call to `Watch()`.
     */
    void Unwatch(WatchHandle handle);

    /**
     * @brief    Drains the event queue and calls `handler` for each event.
     *
     * Events are dispatched in the order they were received. The queue is
     * locked only long enough to swap its contents to a local buffer, so
     * `handler` is called without holding the lock (safe for re-entrant use).
     *
     * @param[in]  handler  Called once per queued event: `void(const FileEvent&)`.
     *
     * @return   The number of events dispatched in this call.
     *
     * @throws   Nothing (exceptions from `handler` propagate unchanged).
     */
    uint32_t Poll(const std::function<void(const FileEvent&)>& handler);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ImFrame::Utility
