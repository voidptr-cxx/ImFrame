/**
 * @file     BackendInfo.hpp
 * @brief    Window configuration and backend abstraction interface for ImFrame
 *
 * Declares `WindowConfig` (public) and `IBackend` (internal). These two
 * types are the sole contract between the application layer and any
 * platform-specific rendering backend. No ImGui or platform types appear
 * in this header — consumers are not required to know ImGui exists.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <string_view>

#include "ImFrame/Core/Error.hpp"

namespace ImFrame {

/**
 * @struct   WindowConfig
 * @brief    User-facing options for creating and configuring the application window
 *
 * All fields have sensible defaults. Pass `WindowConfig{}` for a standard
 * 1280×720 window with vertical sync and docking enabled.
 *
 * @since    0.2.0
 *
 * @example
 * @code
 * ImFrame::WindowConfig config{
 *     .Title  = "My App",
 *     .Width  = 1920,
 *     .Height = 1080,
 *     .VSync  = true,
 * };
 * @endcode
 */
struct WindowConfig {
    /// Window title bar text.
    std::string_view Title = "ImFrame";

    /// Initial window width in pixels. Must be > 0.
    int Width = 1280;

    /// Initial window height in pixels. Must be > 0.
    int Height = 720;

    /// Enable vertical sync. Limits frame rate to the display refresh rate.
    bool VSync = true;

    /// Enable ImGui docking. Allows panels to be docked to each other.
    bool Docking = true;

    /// Enable ImGui multi-viewport. Allows windows to be dragged outside the
    /// main OS window. Requires OS compositor support.
    bool Viewports = false;
};

} // namespace ImFrame

// ─────────────────────────────────────────────────────────────────────────────

namespace ImFrame::Internal {

/**
 * @class    IBackend
 * @brief    Pure interface between the application layer and a platform backend
 *
 * Every concrete backend (GLFW+OpenGL3, Vulkan, Metal, DX12, WebGPU) implements
 * this interface. The application layer calls only these methods — it has no
 * knowledge of GLFW, OpenGL, ImGui backends, or any platform API.
 *
 * Lifecycle contract (must be followed in this order):
 * 1. `Init(config)` — once.
 * 2. Loop: `Poll()` → `BeginFrame()` → [user UI code] → `EndFrame()`.
 * 3. `Shutdown()` — once, after the loop exits.
 *
 * Thread safety: all methods must be called from the same thread that called
 * `Init()`. No method is safe to call from a background thread.
 *
 * @note     `IBackend` lives in `ImFrame::Internal::` — it is not part of the
 *           stable public API. Consumers should use the `Application` class
 *           (Phase 7) rather than implementing `IBackend` directly.
 *
 * @warning  Calling any method other than `Init()` before `Init()` succeeds
 *           results in undefined behaviour. Implementations may IMF_ASSERT.
 *
 * @since    0.2.0
 *
 * @see      WindowConfig
 */
class IBackend {
public:
    /**
     * @brief  Virtual destructor. Implementations must release all resources.
     */
    virtual ~IBackend() = default;

    /**
     * @brief    Initialise the backend: create the window, graphics context,
     *           and ImGui backends.
     *
     * @param[in]  config  Window and feature configuration.
     *
     * @return   An empty `VoidResult` on success, or an `Error` describing
     *           the first failure encountered.
     *
     * @throws   Nothing — all failures are returned via the result type.
     */
    virtual VoidResult Init(const WindowConfig& config) = 0;

    /**
     * @brief    Process pending OS events for one frame.
     *
     * @return   `true` while the window is open; `false` when the user
     *           requests close (e.g. by clicking the × button).
     */
    virtual bool Poll() = 0;

    /**
     * @brief    Begin a new ImGui frame.
     *
     * Must be called after `Poll()` returns `true` and before any ImGui
     * widget calls.
     */
    virtual void BeginFrame() = 0;

    /**
     * @brief    End the ImGui frame, render, and present.
     *
     * Calls `ImGui::Render()`, submits draw data to the GPU, and swaps
     * the swap-chain buffer. Must be called after all ImGui widget calls
     * for this frame.
     */
    virtual void EndFrame() = 0;

    /**
     * @brief    Shut down the backend and release all resources.
     *
     * Safe to call multiple times — subsequent calls are no-ops.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief    Returns the DPI content scale of the primary monitor.
     *
     * @return   Scale factor ≥ 1.0. Returns `2.0` on HiDPI/Retina displays,
     *           `1.5` at 150% Windows scaling, `1.0` on standard displays.
     */
    virtual float DpiScale() const = 0;

    /**
     * @brief    Returns the native window handle.
     *
     * For the GLFW backend this is a `GLFWwindow*`. Cast appropriately.
     * Use only for platform-specific integrations not covered by `IBackend`.
     *
     * @return   Pointer to the underlying window object. Never `nullptr`
     *           after a successful `Init()`.
     */
    virtual void* NativeHandle() const = 0;

    /**
     * @brief    Cancel a pending close request.
     *
     * Called by `Application` when the `OnClose` callback returns `false`
     * (the "unsaved changes" veto pattern). Resets the backend's close flag
     * so that the render loop continues on the next `Poll()` call.
     *
     * Has no effect if no close request is currently pending.
     */
    virtual void CancelClose() noexcept = 0;
};

} // namespace ImFrame::Internal
