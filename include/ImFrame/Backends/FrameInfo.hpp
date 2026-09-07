/**
 * @file     FrameInfo.hpp
 * @brief    Per-frame timing, display metadata, and active window list
 *
 * `FrameInfo` is returned by `IBackend::Poll()` each frame. It replaces the
 * bare `bool` return from Phase 1 and carries all frame-level information the
 * application layer needs: whether to close, how much time has passed, the
 * monitor refresh rate for frame pacing, and which windows are currently active.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Backends/InputEvent.hpp"

#include <vector>

namespace ImFrame {

/**
 * @struct   FrameInfo
 * @brief    Metadata produced by IBackend::Poll() for each frame
 *
 * The backend computes all timing information from its own clock and monitor
 * query APIs. The application layer stores `DeltaTime` and
 * `DisplayRefreshInterval` for use by animation systems and frame-pacing logic.
 *
 * @since    1.9.0
 *
 * @example
 * @code
 * while (true) {
 *     auto info = backend.Poll();
 *     if (info.ShouldClose) break;
 *     myAnim.Update(info.DeltaTime);
 *     backend.BeginFrame();
 *     // render …
 *     backend.EndFrame();
 * }
 * @endcode
 */
struct FrameInfo {
    /// True when the primary window's close button was pressed (or ESC if configured).
    bool ShouldClose = false;

    /// Elapsed seconds since the previous Poll() call. Synthetic 1/60 on the first frame.
    float DeltaTime = 1.0f / 60.0f;

    /// Monitor refresh interval in seconds (1/60 for 60 Hz, 1/144 for 144 Hz, etc.).
    float DisplayRefreshInterval = 1.0f / 60.0f;

    /// Handles of all currently visible windows. Index 0 is always PrimaryWindow.
    std::vector<WindowHandle> ActiveWindows;
};

} // namespace ImFrame
