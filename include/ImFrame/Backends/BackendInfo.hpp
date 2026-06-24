/**
 * @file     BackendInfo.hpp
 * @brief    Window configuration, native context types, and the IBackend interface
 *
 * Declares the public `WindowConfig` and `VSyncMode` types, the
 * `NativeGraphicsContext` variant used by Phase 24 Viewport, and the
 * `IBackend` interface (internal) that every backend must implement.
 *
 * No ImGui or platform types appear in this header — consumers do not need to
 * know that ImGui exists. The `NativeGraphicsContext` variant uses `void*`
 * for GPU-API handles whose header dependencies (Vulkan, DX12, etc.) would
 * otherwise leak into the public API; Phase 20–23 backends document the
 * correct casts.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/FrameInfo.hpp"
#include "ImFrame/Backends/InputEvent.hpp"
#include "ImFrame/Core/Error.hpp"

#include <span>
#include <string_view>
#include <variant>

namespace ImFrame {

// ─── VSyncMode ────────────────────────────────────────────────────────────────

/**
 * @enum   VSyncMode
 * @brief  Vertical synchronisation mode for the swap chain
 *
 * @since  1.9.0
 */
enum class VSyncMode : std::uint8_t {
    Off,      ///< Disable VSync — potentially tearing, lowest latency.
    On,       ///< Enable VSync — no tearing, locked to refresh rate.
    Adaptive, ///< Adaptive VSync — uses `FIFO_RELAXED` / negative swap interval / ALLOW_TEARING. Falls back to On when unsupported.
};

// ─── WindowConfig ─────────────────────────────────────────────────────────────

/**
 * @struct   WindowConfig
 * @brief    User-facing options for creating and configuring a window
 *
 * All fields have sensible defaults. `WindowConfig{}` produces a 1280×720
 * window with VSync enabled and docking on.
 *
 * @since    0.2.0
 *
 * @example
 * @code
 * ImFrame::WindowConfig cfg{
 *     .Title  = "My App",
 *     .Width  = 1920,
 *     .Height = 1080,
 *     .VSync  = ImFrame::VSyncMode::Adaptive,
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

    /// Vertical synchronisation mode.
    VSyncMode VSync = VSyncMode::On;

    /// Enable ImGui docking. Allows panels to be docked to each other.
    bool Docking = true;

    /// Enable ImGui multi-viewport. Allows windows to be dragged outside the
    /// main OS window. Requires OS compositor support.
    bool Viewports = false;

    /// Request a HDR swap chain when the monitor supports it.
    /// Falls back to SDR silently on unsupported hardware or drivers.
    bool HDROutput = false;

    /// Number of frames that may be in flight simultaneously.
    /// Affects swap chain image count (always >= FramesInFlight + 1).
    int FramesInFlight = 2;

    /// CSS selector of the HTML canvas element to render into. Only
    /// meaningful for the Emscripten WebGPU backend (Phase 23) — ignored on
    /// every other backend. Defaults to Emscripten's shell-provided canvas.
    std::string_view EmscriptenCanvasSelector = "#canvas";
};

// ─── WindowExtent ─────────────────────────────────────────────────────────────

/**
 * @struct  WindowExtent
 * @brief   Client area dimensions of a window in pixels
 * @since   1.9.0
 */
struct WindowExtent {
    int Width  = 0; ///< Width in pixels.
    int Height = 0; ///< Height in pixels.
};

// ─── NativeGraphicsContext ────────────────────────────────────────────────────

/// OpenGL context descriptor — empty because ImGui manages GL state directly.
struct OpenGLContext {};

/**
 * @struct VulkanContext
 * @brief  Vulkan device and queue handles for Phase 24 Viewport use
 *
 * All handles are `void*` to avoid pulling `<vulkan/vulkan.h>` into the public
 * header. Phase 20 documents the correct casts.
 *
 * @since  1.9.0
 */
struct VulkanContext {
    void*         Instance             = nullptr; ///< VkInstance
    void*         PhysicalDevice       = nullptr; ///< VkPhysicalDevice
    void*         Device               = nullptr; ///< VkDevice
    std::uint32_t GraphicsQueueFamily  = 0;
    void*         GraphicsQueue        = nullptr; ///< VkQueue
    void*         ViewportCommandPool  = nullptr; ///< VkCommandPool for Phase 24 Viewport use (RESET_COMMAND_BUFFER_BIT)
    void*         DescriptorPool       = nullptr; ///< VkDescriptorPool shared across windows and frames
    std::uint32_t SwapchainImageFormat = 0;       ///< VkFormat of the primary swap chain — used by Phase 24 Viewport
};

/**
 * @struct MetalContext
 * @brief  Metal device and command queue handles for Phase 24 Viewport use
 * @since  1.9.0
 */
struct MetalContext {
    void*         Device         = nullptr; ///< MTLDevice*
    void*         CommandQueue   = nullptr; ///< MTLCommandQueue*
    std::uint32_t PixelFormat    = 0;       ///< MTLPixelFormat of the primary window's layer
    int           FramesInFlight = 2;       ///< Configured frame-in-flight count
};

/**
 * @struct DX12Context
 * @brief  Direct3D 12 device and queue handles for Phase 24 Viewport use
 * @since  1.9.0
 */
struct DX12Context {
    void*         Device            = nullptr; ///< ID3D12Device*
    void*         CommandQueue      = nullptr; ///< Direct ID3D12CommandQueue*
    void*         CopyQueue         = nullptr; ///< Copy ID3D12CommandQueue* — Phase 24 Viewport texture uploads
    void*         SrvHeap           = nullptr; ///< Shader-visible ID3D12DescriptorHeap* (CBV_SRV_UAV)
    std::uint32_t SrvDescriptorSize = 0;       ///< GetDescriptorHandleIncrementSize() for SrvHeap
    std::uint32_t SwapChainFormat   = 0;       ///< DXGI_FORMAT of the primary window's swap chain
    int           FramesInFlight    = 2;       ///< Configured frame-in-flight count
};

/**
 * @struct WebGPUContext
 * @brief  WebGPU device and queue handles for Phase 24 Viewport use
 * @since  1.9.0
 */
struct WebGPUContext {
    void*         Device          = nullptr; ///< WGPUDevice
    void*         Queue           = nullptr; ///< WGPUQueue
    std::uint32_t PreferredFormat = 0;       ///< WGPUTextureFormat of the primary surface (or offscreen target, headless)
    std::uint32_t MaxTextureDimension2D = 0; ///< WGPULimits::maxTextureDimension2D — Phase 24 Viewport size validation
    bool          IsEmscripten    = false;   ///< True on the browser build; false for the native Dawn backend
};

/**
 * @enum   HeadlessPixelFormat
 * @brief  Pixel layout of the HeadlessBackend offscreen framebuffer
 * @since  1.9.0
 */
enum class HeadlessPixelFormat : std::uint8_t {
    RGBA8, ///< 4 bytes per pixel: red, green, blue, alpha (default).
    BGRA8, ///< 4 bytes per pixel: blue, green, red, alpha.
};

/**
 * @struct HeadlessContext
 * @brief  Offscreen framebuffer descriptor for HeadlessBackend
 * @since  1.9.0
 */
struct HeadlessContext {
    int                Width       = 0;
    int                Height      = 0;
    HeadlessPixelFormat PixelFormat = HeadlessPixelFormat::RGBA8;
};

/**
 * @brief  Discriminated union of all backend-specific graphics context types
 *
 * Returned by `IBackend::GetNativeGraphicsContext()` once at startup.
 * Phase 24's `Viewport` stores this to create per-viewport framebuffers.
 *
 * @since  1.9.0
 */
using NativeGraphicsContext = std::variant<
    OpenGLContext,
    VulkanContext,
    MetalContext,
    DX12Context,
    WebGPUContext,
    HeadlessContext
>;

} // namespace ImFrame

// ─────────────────────────────────────────────────────────────────────────────

namespace ImFrame::Internal {

/**
 * @class    IBackend
 * @brief    Pure interface between the application layer and a platform backend
 *
 * Every concrete backend (GLFW+OpenGL3, Vulkan, Metal, DX12, WebGPU, Headless)
 * implements this interface. The application layer calls only these methods.
 *
 * **Lifecycle contract** (methods must be called in this order):
 * 1. `Init(config)` — once.
 * 2. Loop: `Poll()` → `BeginFrame()` → [UI code] → `EndFrame()`.
 * 3. `Shutdown()` — once, after the loop exits.
 *
 * **Input invariant:** Input events are never forwarded to ImGui inside a
 * backend. All ImGui input forwarding happens in the application layer after
 * `DrainInputEvents()`. A backend that calls `ImGui::GetIO()` in its input
 * handling is a bug.
 *
 * **Context invariant:** `GetNativeGraphicsContext()` is called once at startup
 * and the result is stored. The context it exposes is stable for the application
 * lifetime — backends must not invalidate device handles during normal operation.
 *
 * **Thread safety:** All methods must be called from the same thread that called
 * `Init()`. No method is safe to call from a background thread.
 *
 * @since    0.2.0
 *
 * @see      WindowConfig, FrameInfo, InputEvent
 */
class IBackend {
public:
    /**
     * @brief  Virtual destructor — implementations must release all resources.
     */
    virtual ~IBackend() = default;

    // ─── Required ─────────────────────────────────────────────────────────────
    // All pure-virtual methods below must be implemented by every backend.

    /**
     * @brief    Initialise the backend: create the window, graphics context,
     *           and ImGui backends.
     *
     * @param[in]  config  Window and feature configuration.
     * @return   Empty `VoidResult` on success, or an `Error` on first failure.
     * @throws   Nothing.
     */
    virtual VoidResult Init(const WindowConfig& config) = 0;

    /**
     * @brief    Process pending OS events and return per-frame metadata.
     *
     * Replaces the Phase 1 `bool Poll()`. The backend computes delta time
     * internally (e.g. via `glfwGetTime()`) and queries the monitor's refresh
     * rate. Input events are accumulated in the backend's internal queue and
     * are available via `DrainInputEvents()` after this call returns.
     *
     * @return   `FrameInfo` with ShouldClose, DeltaTime, DisplayRefreshInterval,
     *           and the list of currently active window handles.
     * @throws   Nothing.
     */
    virtual FrameInfo Poll() = 0;

    /**
     * @brief    Begin a new ImGui frame for the specified window.
     *
     * Must be called after `Poll()` and before any ImGui widget calls.
     * For single-window applications pass no argument (defaults to PrimaryWindow).
     *
     * @param[in]  handle  Window to begin the frame for. Defaults to PrimaryWindow.
     */
    virtual void BeginFrame(WindowHandle handle = PrimaryWindow) = 0;

    /**
     * @brief    End the ImGui frame, render, and present the specified window.
     *
     * @param[in]  handle  Window to end the frame for. Defaults to PrimaryWindow.
     */
    virtual void EndFrame(WindowHandle handle = PrimaryWindow) = 0;

    /**
     * @brief    Shut down the backend and release all resources.
     *
     * Safe to call multiple times — subsequent calls are no-ops.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief    Returns the native window handle.
     *
     * For the GLFW backend this is a `GLFWwindow*`. Use only for
     * platform-specific integrations not covered by `IBackend`.
     *
     * @return   Pointer to the underlying window object. nullptr if not initialised.
     */
    virtual void* NativeHandle() const = 0;

    /**
     * @brief    Cancel a pending close request.
     *
     * Called by `Application` when an `OnClose` callback returns `false`
     * (the "unsaved changes" veto pattern). Resets the backend's close flag
     * so the render loop continues on the next `Poll()` call.
     */
    virtual void CancelClose() noexcept = 0;

    // ─── Optional — default implementations provided ──────────────────────────
    // Backends override these to add capabilities. The defaults are stubs
    // sufficient for single-window backends and the test harness.

    /**
     * @brief    Returns the DPI content scale of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     * @return   Scale factor ≥ 1.0. Returns 1.0 by default.
     */
    virtual float WindowDpiScale(WindowHandle handle = PrimaryWindow) const {
        (void)handle;
        return 1.0f;
    }

    /**
     * @brief    Returns the client-area dimensions of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     * @return   `WindowExtent` with Width and Height in pixels.
     */
    virtual WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const {
        (void)handle;
        return {};
    }

    /**
     * @brief    Returns true if the specified window is currently minimised.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    virtual bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const {
        (void)handle;
        return false;
    }

    /**
     * @brief    Returns true if the specified window currently has input focus.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    virtual bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const {
        (void)handle;
        return true;
    }

    /**
     * @brief    Create a secondary window sharing the primary window's GPU context.
     *
     * @param[in]  config  Window configuration for the new window.
     * @return   A unique `WindowHandle` for the new window. Returns `PrimaryWindow`
     *           (indicating failure) if multi-window is not supported.
     */
    virtual WindowHandle CreateWindow(const WindowConfig& config) {
        (void)config;
        return PrimaryWindow;
    }

    /**
     * @brief    Destroy a secondary window and release its resources.
     *
     * Has no effect when called with `PrimaryWindow` — the primary window's
     * lifetime is bound to `Init()`/`Shutdown()`.
     *
     * @param[in]  handle  Handle returned by a prior `CreateWindow()` call.
     */
    virtual void DestroyWindow(WindowHandle handle) {
        (void)handle;
    }

    /**
     * @brief    Drain and return all input events accumulated since the last drain.
     *
     * The returned span is valid until the next call to `DrainInputEvents()`.
     * The application layer calls this once per frame after `Poll()`, processes
     * the events, and discards the span before the next frame begins.
     *
     * @return   Non-owning view of this frame's input events. Empty by default.
     */
    virtual std::span<const InputEvent> DrainInputEvents() {
        return {};
    }

    /**
     * @brief    Returns a descriptor of the backend's GPU context.
     *
     * Called once at application startup and stored by `Application`. Phase 24's
     * `Viewport` uses this to create per-viewport framebuffers.
     *
     * @return   `NativeGraphicsContext` variant. Returns `OpenGLContext{}` by default.
     */
    virtual NativeGraphicsContext GetNativeGraphicsContext() const {
        return OpenGLContext{};
    }
};

} // namespace ImFrame::Internal
