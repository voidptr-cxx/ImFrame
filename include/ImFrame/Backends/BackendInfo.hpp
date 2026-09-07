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
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Backends/FrameInfo.hpp"
#include "ImFrame/Backends/InputEvent.hpp"
#include "ImFrame/Core/Error.hpp"

#include <cstdint>
#include <memory>
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

// ─── IViewportFramebuffer ────────────────────────────────────────────────────

/**
 * @struct ViewportHandles
 * @brief  Raw GPU handles for one Viewport framebuffer, backend-specific fields populated
 *
 * Each backend fills only the fields relevant to it. `ViewportRegistry` converts
 * this struct into the appropriate `Rendering::ViewportImageXxx` member.
 *
 * @internal
 * @since  2.0.0
 */
struct ViewportHandles {
    // ── OpenGL ────────────────────────────────────────────────────────────────
    unsigned int glFBO       = 0; ///< GLuint FBO — bind before user renders.
    unsigned int glColorTex  = 0; ///< GLuint colour texture attached to glFBO.
    // ── Vulkan ────────────────────────────────────────────────────────────────
    void* vkImage   = nullptr; ///< VkImage in COLOR_ATTACHMENT_OPTIMAL layout.
    void* vkView    = nullptr; ///< VkImageView for vkImage.
    void* vkCmdBuf  = nullptr; ///< VkCommandBuffer from ViewportCommandPool, already begun.
    // ── Metal ─────────────────────────────────────────────────────────────────
    void* mtlTex    = nullptr; ///< MTLTexture* (BGRA8Unorm_sRGB). ARC-managed.
    // ── DX12 ──────────────────────────────────────────────────────────────────
    void*          d3dResource = nullptr; ///< ID3D12Resource* in RENDER_TARGET state.
    std::uintptr_t d3dRTV      = 0;       ///< D3D12_CPU_DESCRIPTOR_HANDLE.ptr for colour target.
    std::uint64_t  d3dSRV      = 0;       ///< D3D12_GPU_DESCRIPTOR_HANDLE.ptr sampled by ImGui.
    void*          d3dCmdList  = nullptr; ///< ID3D12GraphicsCommandList7* opened by ImFrame.
    // ── WebGPU ────────────────────────────────────────────────────────────────
    void* wgpuTex     = nullptr; ///< WGPUTexture (RenderAttachment | TextureBinding).
    void* wgpuView    = nullptr; ///< WGPUTextureView for wgpuTex.
    void* wgpuEncoder = nullptr; ///< WGPUCommandEncoder created by ImFrame. Do not Finish it.
    // ── Headless ──────────────────────────────────────────────────────────────
    std::uint8_t* headlessPixels = nullptr; ///< RGBA8 CPU buffer, width*height*4 bytes.
    // ── Common ────────────────────────────────────────────────────────────────
    std::uint32_t width       = 0; ///< Framebuffer width in pixels.
    std::uint32_t height      = 0; ///< Framebuffer height in pixels.
    std::uint64_t imTextureId = 0; ///< Opaque ImTextureID — passed to ImGui::Image().
};

/**
 * @class IViewportFramebuffer
 * @brief Abstract owner of one Viewport's offscreen GPU resources
 *
 * Each backend provides a concrete subclass. `ViewportRegistry` holds one
 * instance per registered `Viewport` and drives the render cycle via
 * `BeginRender` / `EndRender` around the user's `OnRender` callback.
 *
 * All methods are called from the main thread between `Poll()` and `BeginFrame()`.
 *
 * @internal
 * @since 2.0.0
 */
class IViewportFramebuffer {
public:
    virtual ~IViewportFramebuffer() = default;

    /**
     * @brief    Recreate GPU resources at a new pixel size.
     *
     * Called by `ViewportRegistry` when the Viewport's size changes. The old
     * resources are released before new ones are allocated.
     *
     * @param[in]  width   New width in pixels. Must be > 0.
     * @param[in]  height  New height in pixels. Must be > 0.
     */
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;

    /**
     * @brief    Transition the framebuffer to a render-ready state.
     *
     * Called once per frame before the user's `OnRender` callback. Backends that
     * use command buffers open them here; Vulkan transitions the image layout.
     *
     * @param[in]  frameIndex  Current frame-in-flight index.
     */
    virtual void BeginRender(std::uint32_t frameIndex) = 0;

    /**
     * @brief    Submit GPU work and synchronise with the main render pass.
     *
     * Called immediately after `OnRender` returns. Backends submit their command
     * buffer and, where necessary, block the CPU until the GPU completes the
     * viewport render so the texture is ready for `ImGui::Image()` sampling.
     *
     * @param[in]  frameIndex  Current frame-in-flight index.
     */
    virtual void EndRender(std::uint32_t frameIndex) = 0;

    /**
     * @brief    Return raw GPU handles for the current frame.
     *
     * Called between `BeginRender` and `EndRender`. `ViewportRegistry` converts
     * the populated fields into the appropriate `Rendering::ViewportImageXxx`.
     *
     * @return   `ViewportHandles` with backend-relevant fields populated.
     */
    [[nodiscard]] virtual ViewportHandles GetHandles() const = 0;
};

} // namespace ImFrame::Internal — close early so IBackend opens its own block below

// Re-open ImFrame::Internal for IBackend
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

    /**
     * @brief    Allocate a backend-specific offscreen framebuffer for a Viewport.
     *
     * Called by `ViewportRegistry` on the first frame a `Viewport` is shown, and
     * again after a size change. Backends that do not support Viewport rendering
     * return `nullptr` (default).
     *
     * @param[in]  width   Framebuffer width in pixels.
     * @param[in]  height  Framebuffer height in pixels.
     * @return   Owning pointer to the framebuffer, or `nullptr` if unsupported.
     */
    virtual std::unique_ptr<IViewportFramebuffer> CreateViewportFramebuffer(
        std::uint32_t width, std::uint32_t height) {
        (void)width;
        (void)height;
        return nullptr;
    }
};

} // namespace ImFrame::Internal
