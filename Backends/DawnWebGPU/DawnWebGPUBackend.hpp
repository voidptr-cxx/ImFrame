/**
 * @file     DawnWebGPUBackend.hpp
 * @brief    SDL3 + Dawn (native WebGPU) concrete backend implementation for ImFrame
 *
 * Implements `IBackend` using SDL3 for windowing and Dawn's native WebGPU
 * implementation for rendering. Key design properties:
 * - A single `WGPUQueue` handles all submission — WebGPU has no explicit
 *   queue-family selection, unlike Vulkan/DX12.
 * - `wgpuInstanceRequestAdapter()`/`wgpuAdapterRequestDevice()` are Future-based
 *   in the installed Dawn revision (20260410.140140) — blocked on synchronously
 *   via `wgpuInstanceWaitAny(..., UINT64_MAX)`, not the simple synchronous
 *   callback the Phase 23 proposal assumed. See DECISIONS.md.
 * - No persistent per-frame command list/fence array (unlike DX12/Vulkan) —
 *   `WGPUCommandEncoder` is created, recorded, finished, submitted, and
 *   released fresh every frame; WebGPU has no CPU-side fence the application
 *   layer needs to manage directly.
 * - Surface creation delegates to `CreateNativeSurface()`
 *   (`NativeSurface.cpp`), which calls imgui's own
 *   `ImGui_ImplWGPU_CreateWGPUSurfaceHelper()` rather than hand-rolling
 *   per-platform `WGPUSurfaceDescriptor` chains.
 *
 * @internal
 * This file is not part of the public ImFrame API.
 * Include path is provided by the ImFrame_DawnWebGPU CMake target.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "WebGPUDeviceSetup.hpp"
#include "WebGPUSurface.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"

#include <cstddef>
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @class    DawnWebGPUBackend
 * @brief    IBackend implementation using SDL3 for windowing and Dawn for WebGPU rendering
 *
 * On native platforms Dawn selects D3D12 (Windows), Metal (macOS), or Vulkan
 * (Linux) automatically — there is no explicit backend-selection toggle in
 * this phase (see proposal's Instance Creation section).
 *
 * @since    2.3.0
 *
 * @see      IBackend, WindowConfig
 */
class DawnWebGPUBackend final : public IBackend {
public:
    /**
     * @brief  Construct the backend. No resources are acquired until Init().
     *
     * @param[in]  headless  When true, `Init()` allocates an offscreen
     *                       `WGPUTexture` render target for the primary
     *                       window instead of a real surface — used by tests
     *                       and CI environments without a display. The SDL
     *                       window itself is still created (hidden).
     */
    explicit DawnWebGPUBackend(bool headless = false) noexcept : _primaryIsHeadless(headless) {}

    ~DawnWebGPUBackend() override;

    DawnWebGPUBackend(const DawnWebGPUBackend&)            = delete;
    DawnWebGPUBackend& operator=(const DawnWebGPUBackend&) = delete;
    DawnWebGPUBackend(DawnWebGPUBackend&&)                 = delete;
    DawnWebGPUBackend& operator=(DawnWebGPUBackend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    VoidResult Init(const WindowConfig& config) override;
    FrameInfo Poll() override;
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;
    void EndFrame(WindowHandle handle = PrimaryWindow) override;
    void Shutdown() override;
    void* NativeHandle() const override;
    void CancelClose() noexcept override;

    // ─── IBackend (optional overrides) ────────────────────────────────────────

    float WindowDpiScale(WindowHandle handle = PrimaryWindow) const override;
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;
    bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const override;
    bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const override;
    WindowHandle CreateWindow(const WindowConfig& config) override;
    void DestroyWindow(WindowHandle handle) override;
    std::span<const InputEvent> DrainInputEvents() override;
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    /**
     * @brief    Reads back the primary window's offscreen render target.
     *
     * Only supported when the backend was constructed with `headless = true`.
     * Row pitch is always de-strided to tightly-packed RGBA8 rows before
     * returning, matching the Phase 20–22 contract.
     *
     * @return   RGBA8 pixel data, tightly packed, top-to-bottom. Empty if not
     *           initialised or not running headless.
     */
    [[nodiscard]] std::vector<std::byte> ReadPixels() const;

private:
    struct WindowData {
        SDL_Window*    sdlWindow = nullptr;
        WebGPUSurface  surface;
        WGPUTexture    headlessTarget = nullptr; ///< Set only when headless == true. Persistent across frames.
        bool           headless       = false;
        bool           needsResize    = false;
        bool           frameStarted   = false;
        WGPUTexture    currentTexture = nullptr; ///< This frame's acquired/headless texture (owning ref for windowed).
        WGPUTextureView currentView   = nullptr; ///< This frame's view of currentTexture.
        WindowConfig   config;
    };

    // ─── Initialisation helpers ───────────────────────────────────────────────
    VoidResult InitSDL(const WindowConfig& config);
    VoidResult CreateSurfaceFor(WindowData& wd, bool headless);
    VoidResult InitImGui();

    // ─── Frame helpers ────────────────────────────────────────────────────────
    void HandleResize(WindowData& wd);
    void ReleaseFrameTexture(WindowData& wd);

    // ─── Secondary-window lookup ──────────────────────────────────────────────
    [[nodiscard]] WindowData* FindWindow(WindowHandle handle);
    [[nodiscard]] const WindowData* FindWindow(WindowHandle handle) const;
    [[nodiscard]] WindowHandle HandleForSDLWindow(SDL_WindowID id) const;

    // ─── Core WebGPU objects (instance lifetime) ──────────────────────────────
    WGPUInstance _instance = nullptr;
    WGPUAdapter  _adapter  = nullptr;
    WGPUDevice   _device   = nullptr;
    WGPUQueue    _queue    = nullptr;
    WGPUTextureFormat _headlessFormat = WGPUTextureFormat_RGBA8Unorm;
    std::uint32_t _maxTextureDimension2D = 0;

    // ─── ReadPixels (headless only) ────────────────────────────────────────────
    WGPUBuffer  _readbackBuffer     = nullptr;
    std::uint64_t _readbackBufferSize = 0;
    std::uint64_t _readbackRowPitch   = 0; ///< 256-byte-aligned row pitch of the last readback.
    std::uint32_t _readbackWidth      = 0;
    std::uint32_t _readbackHeight     = 0;
    bool          _hasReadback        = false;

    // ─── Per-window state ──────────────────────────────────────────────────────
    WindowData _primary;
    bool       _primaryIsHeadless = false;
    std::unordered_map<WindowHandle, WindowData>   _secondaryWindows;
    std::unordered_map<SDL_WindowID, WindowHandle> _sdlIdToHandle;

    // ─── Input ─────────────────────────────────────────────────────────────────
    std::vector<InputEvent> _inputQueue;
    std::vector<InputEvent> _drainBuffer;

    // ─── Timing ────────────────────────────────────────────────────────────────
    uint64_t _lastPerfCount = 0;

    // ─── State ─────────────────────────────────────────────────────────────────
    bool _initialised    = false;
    bool _sdlInitialised = false;
    bool _shouldClose    = false;
    WindowHandle _nextHandle = 1;
};

} // namespace ImFrame::Internal
