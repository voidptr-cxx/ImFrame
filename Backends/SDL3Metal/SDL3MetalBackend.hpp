/**
 * @file     SDL3MetalBackend.hpp
 * @brief    SDL3 + Metal concrete backend implementation for ImFrame (macOS)
 *
 * Implements `IBackend` using SDL3 for windowing and Metal for rendering.
 * Key design properties:
 * - ARC throughout — no manual retain/release, no @autoreleasepool in
 *   BeginFrame()/EndFrame() (only around the Poll() event drain).
 * - Single `dispatch_semaphore_t` per window (initial count == frames-in-flight)
 *   replaces Vulkan's per-slot fence/semaphore pair.
 * - No explicit swap chain recreation on resize — only `CAMetalLayer.drawableSize`
 *   is updated.
 * - `presentDrawable:` is encoded into the command buffer before `commit`.
 *
 * @internal
 * This file is not part of the public ImFrame API. It is an Objective-C++
 * header — consumers must compile any translation unit that includes it as
 * `.mm`. Include path is provided by the ImFrame_SDL3Metal CMake target.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "FrameResources.hpp"
#include "MetalLayer.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"

#include <Metal/Metal.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <cstddef>
#include <dispatch/dispatch.h>
#include <unordered_map>
#include <vector>

namespace ImFrame::Internal {

/**
 * @class    SDL3MetalBackend
 * @brief    IBackend implementation using SDL3 for windowing and Metal for rendering
 *
 * Targets macOS 10.15+. ImGui is integrated via `imgui_impl_sdl3` (the
 * `ForMetal` init path) and `imgui_impl_metal`.
 *
 * @since    2.1.0
 *
 * @see      IBackend, WindowConfig
 */
class SDL3MetalBackend final : public IBackend {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief  Construct the backend. No resources are acquired until Init().
     *
     * @param[in]  headless  When true, `Init()` allocates an offscreen
     *                       `MTLTexture` render target for the primary window
     *                       instead of a real `CAMetalLayer` — used by tests
     *                       and CI environments without a display surface.
     *                       The SDL window itself is still created (hidden)
     *                       so `Poll()`'s event-pump machinery is unchanged.
     */
    explicit SDL3MetalBackend(bool headless = false) noexcept : _primaryIsHeadless(headless) {}

    /**
     * @brief  Destructor — calls Shutdown() if still initialised.
     */
    ~SDL3MetalBackend() override;

    SDL3MetalBackend(const SDL3MetalBackend&)            = delete;
    SDL3MetalBackend& operator=(const SDL3MetalBackend&) = delete;
    SDL3MetalBackend(SDL3MetalBackend&&)                 = delete;
    SDL3MetalBackend& operator=(SDL3MetalBackend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    /**
     * @brief    Initialise SDL3, create the Metal device/command queue/layer, and
     *           bring up the ImGui Metal and SDL3 backends.
     *
     * @param[in]  config  Window and feature options.
     * @return   Empty result on success; an `Error` on the first failure.
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Poll SDL3 events, translate to InputEvents, compute delta time.
     *
     * Wrapped in a single `@autoreleasepool` to collect Objective-C temporaries
     * created during SDL3 event processing — the only autorelease pool in the
     * per-frame path (none inside BeginFrame()/EndFrame()).
     *
     * @return   `FrameInfo` with ShouldClose, DeltaTime, DisplayRefreshInterval,
     *           and the list of active window handles.
     * @throws   Nothing.
     */
    FrameInfo Poll() override;

    /**
     * @brief    Wait on the frame semaphore, acquire the next drawable, and
     *           start an ImGui frame.
     *
     * @param[in]  handle  Window to begin. Defaults to PrimaryWindow.
     */
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Finalise the ImGui frame, render, present, and commit.
     *
     * @param[in]  handle  Window to end. Defaults to PrimaryWindow.
     */
    void EndFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Release all Metal and SDL3 resources. Safe to call multiple times.
     */
    void Shutdown() override;

    /**
     * @brief    Returns the primary SDL_Window* as void*.
     *
     * @return   The SDL_Window* cast to void*. nullptr if not initialised.
     */
    void* NativeHandle() const override;

    /**
     * @brief    Cancel the pending close request.
     */
    void CancelClose() noexcept override;

    // ─── IBackend (optional overrides) ────────────────────────────────────────

    /**
     * @brief    Returns the DPI content scale of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    float WindowDpiScale(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns the client-area size of the specified window in pixels.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window is currently minimised.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window currently has input focus.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Create a secondary SDL3 window with its own CAMetalLayer.
     *
     * @param[in]  config  Window configuration for the new window.
     * @return   A unique `WindowHandle` ≥ 1. Returns `PrimaryWindow` on failure.
     */
    WindowHandle CreateWindow(const WindowConfig& config) override;

    /**
     * @brief    Destroy a secondary window.
     *
     * Has no effect when called with `PrimaryWindow`.
     *
     * @param[in]  handle  Handle from a prior `CreateWindow()` call.
     */
    void DestroyWindow(WindowHandle handle) override;

    /**
     * @brief    Drain all input events accumulated since the last call.
     */
    std::span<const InputEvent> DrainInputEvents() override;

    /**
     * @brief    Returns a populated `MetalContext` for Phase 24 Viewport use.
     */
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    /**
     * @brief    Allocate an MTLTexture framebuffer for Viewport use.
     */
    std::unique_ptr<IViewportFramebuffer> CreateViewportFramebuffer(
        std::uint32_t width, std::uint32_t height) override;

    /**
     * @brief    Reads back the primary window's offscreen render target.
     *
     * Only supported when the backend was created via `Application::CreateHeadless()`
     * (i.e. the primary window has no real `CAMetalLayer`). Windowed-mode
     * capture of a presented `CAMetalLayer` drawable is not yet implemented —
     * see `DECISIONS.md` for the scoping rationale. Matches
     * `HeadlessBackend::ReadPixels()`'s contract exactly when it does apply.
     *
     * @return   RGBA8 pixel data, tightly packed, top-to-bottom. Empty if not
     *           initialised, not running headless, or the pixel format is an
     *           HDR (16-bit float) format.
     */
    [[nodiscard]] std::vector<std::byte> ReadPixels() const;

private:
    // ─── Per-window resources ─────────────────────────────────────────────────

    struct WindowData {
        SDL_Window*                 sdlWindow      = nullptr;
        SDL_MetalView                metalView      = nullptr; ///< nullptr when headless (no CAMetalLayer view created).
        MetalLayer                  metalLayer;
        dispatch_semaphore_t        frameSemaphore = nullptr;
        std::vector<FrameResources> frames;
        uint32_t                    frameIndex     = 0; ///< Cycles [0, framesInFlight).
        bool                        needsResize    = false;
        bool                        frameStarted   = false;
        WindowConfig                config;
    };

    // ─── Initialisation helpers ───────────────────────────────────────────────
    VoidResult InitSDL(const WindowConfig& config);
    VoidResult CreateDevice();
    VoidResult CreateMetalLayerFor(WindowData& wd, bool headless);
    VoidResult AllocateFrameResources(WindowData& wd);
    VoidResult InitImGui(const WindowConfig& config);

    // ─── ReadPixels (headless only) ───────────────────────────────────────────
    bool EnsureReadbackTexture(std::size_t width, std::size_t height, MTLPixelFormat format);

    // ─── Frame helpers ────────────────────────────────────────────────────────
    void HandleResize(WindowData& wd);

    // ─── Secondary-window lookup ──────────────────────────────────────────────
    [[nodiscard]] WindowData* FindWindow(WindowHandle handle);
    [[nodiscard]] const WindowData* FindWindow(WindowHandle handle) const;
    [[nodiscard]] WindowHandle HandleForSDLWindow(SDL_WindowID id) const;

    // ─── Core Metal objects (instance lifetime, ARC-managed) ──────────────────
    id<MTLDevice>       _device       = nil;
    id<MTLCommandQueue> _commandQueue = nil;

    // ─── ReadPixels (headless only) ───────────────────────────────────────────
    // Readback texture used only on discrete (non-unified-memory) GPUs.
    id<MTLTexture>        _readbackTexture           = nil;
    // The command buffer that performed the most recent headless frame's
    // render + optional readback blit — ReadPixels() waits on it before
    // reading, matching SDL3VulkanBackend::ReadPixels()'s blocking contract.
    id<MTLCommandBuffer>  _lastHeadlessCommandBuffer = nil;

    // ─── Per-window state ─────────────────────────────────────────────────────
    WindowData _primary;
    bool       _primaryIsHeadless = false;
    std::unordered_map<WindowHandle, WindowData>    _secondaryWindows;
    std::unordered_map<SDL_WindowID, WindowHandle>  _sdlIdToHandle;

    // ─── Input ────────────────────────────────────────────────────────────────
    std::vector<InputEvent> _inputQueue;
    std::vector<InputEvent> _drainBuffer;

    // ─── Timing ───────────────────────────────────────────────────────────────
    uint64_t _lastPerfCount = 0;

    // ─── State ────────────────────────────────────────────────────────────────
    bool _initialised    = false;
    /// True once InitSDL() has called SDL_Init() successfully. Tracked
    /// separately from _initialised/_device so Shutdown() still runs full
    /// cleanup (and calls SDL_Quit()) when Init() fails partway through —
    /// e.g. SDL_CreateWindow() or CreateDevice() failing after InitSDL()
    /// already succeeded, where both _initialised and _device would
    /// otherwise be falsy/nil.
    bool _sdlInitialised = false;
    bool _shouldClose    = false;
    int  _framesInFlight = 2;
    WindowHandle _nextHandle = 1;
};

} // namespace ImFrame::Internal
