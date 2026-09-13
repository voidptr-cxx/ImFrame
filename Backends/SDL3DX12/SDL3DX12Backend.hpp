/**
 * @file     SDL3DX12Backend.hpp
 * @brief    SDL3 + DirectX 12 concrete backend implementation for ImFrame (Windows)
 *
 * Implements `IBackend` using SDL3 for windowing and DirectX 12 for rendering.
 * Key design properties:
 * - `FLIP_DISCARD` swap effect only, with the frame-latency waitable object as
 *   the primary frame-pacing mechanism (replacing an explicit fence wait for
 *   the common case).
 * - A single direct command queue submits all rendering; a separate copy
 *   queue exists for Phase 24 Viewport texture uploads (unused internally by
 *   this phase — see DECISIONS.md).
 * - Two shared descriptor heaps (RTV, shader-visible SRV/CBV/UAV) allocated
 *   once and indexed via `DescriptorAllocator`, not per-window heaps.
 * - `SetDescriptorHeaps` called at most once per command list, immediately
 *   before `ImGui_ImplDX12_RenderDrawData`.
 *
 * @internal
 * This file is not part of the public ImFrame API.
 * Include path is provided by the ImFrame_SDL3DX12 CMake target.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "DescriptorAllocator.hpp"
#include "DX12SwapChain.hpp"
#include "FrameResources.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"

#include <cstddef>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

namespace ImFrame::Internal {

/**
 * @class    SDL3DX12Backend
 * @brief    IBackend implementation using SDL3 for windowing and DirectX 12 for rendering
 *
 * Targets D3D_FEATURE_LEVEL_12_0, Windows 10 1903+. ImGui is integrated via
 * `imgui_impl_sdl3` (the `ForD3D` init path) and `imgui_impl_dx12`.
 *
 * @since    2.2.0
 *
 * @see      IBackend, WindowConfig
 */
class SDL3DX12Backend final : public IBackend {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief  Construct the backend. No resources are acquired until Init().
     *
     * @param[in]  headless  When true, `Init()` allocates an offscreen
     *                       `ID3D12Resource` render target for the primary
     *                       window instead of a real swap chain — used by
     *                       tests and CI environments without a display.
     *                       The SDL window itself is still created (hidden).
     */
    explicit SDL3DX12Backend(bool headless = false) noexcept : _primaryIsHeadless(headless) {}

    /**
     * @brief  Destructor — calls Shutdown() if still initialised.
     */
    ~SDL3DX12Backend() override;

    SDL3DX12Backend(const SDL3DX12Backend&)            = delete;
    SDL3DX12Backend& operator=(const SDL3DX12Backend&) = delete;
    SDL3DX12Backend(SDL3DX12Backend&&)                 = delete;
    SDL3DX12Backend& operator=(SDL3DX12Backend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    /**
     * @brief    Initialise DPI awareness, SDL3, the D3D12 device/queues/heaps,
     *           and bring up the ImGui DX12 and SDL3 backends.
     *
     * @param[in]  config  Window and feature options.
     * @return   Empty result on success; an `Error` on the first failure.
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Poll SDL3 events, translate to InputEvents, compute delta time.
     *
     * @return   `FrameInfo` with ShouldClose, DeltaTime, DisplayRefreshInterval,
     *           and the list of active window handles.
     * @throws   Nothing.
     */
    FrameInfo Poll() override;

    /**
     * @brief    Wait on the frame latency object, reset the frame's command
     *           allocator/list, transition the back buffer, and start an
     *           ImGui frame.
     *
     * @param[in]  handle  Window to begin. Defaults to PrimaryWindow.
     */
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Finalise the ImGui frame, render, transition back to PRESENT,
     *           and present.
     *
     * @param[in]  handle  Window to end. Defaults to PrimaryWindow.
     */
    void EndFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Release all D3D12 and SDL3 resources. Safe to call multiple times.
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

    float WindowDpiScale(WindowHandle handle = PrimaryWindow) const override;
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;
    bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const override;
    bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Create a secondary SDL3 window with its own DX12SwapChain.
     *
     * @param[in]  config  Window configuration for the new window.
     * @return   A unique `WindowHandle` ≥ 1. Returns `PrimaryWindow` on failure.
     */
    WindowHandle CreateWindow(const WindowConfig& config) override;

    /**
     * @brief    Destroy a secondary window, draining its in-flight frames first.
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
     * @brief    Returns a populated `DX12Context` for Phase 24 Viewport use.
     */
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    /**
     * @brief    Real, already-typed D3D12 handles for a backend-internal `NativeRendererDX12`
     *           (or a test) to construct itself against this backend's live device.
     *
     * @internal
     * Unlike `GetNativeGraphicsContext()` (the *public* accessor for Phase 24 Viewport's
     * application-facing use), this returns real types directly — `Backends/SDL3DX12/` is itself
     * an internal-only header directory (not `include/ImFrame/`), so there's no API-leak concern.
     * Mirrors `SDL3VulkanBackend::GetRendererHandles()`'s identical role and rationale (Phase 35.1)
     * — no command-allocator-equivalent handle is returned, unlike Vulkan's own `CommandPool`,
     * since `NativeRendererDX12` creates its own `ID3D12CommandAllocator` from just the device,
     * matching `ViewportFramebufferDX12`'s own identical convention.
     */
    struct DX12RendererHandles {
        ID3D12Device4*      Device      = nullptr;
        ID3D12CommandQueue* DirectQueue = nullptr;
    };
    [[nodiscard]] DX12RendererHandles GetRendererHandles() const noexcept;

    /**
     * @brief    Allocate a D3D12 committed render target for Viewport use.
     */
    std::unique_ptr<IViewportFramebuffer> CreateViewportFramebuffer(
        std::uint32_t width, std::uint32_t height) override;

    /**
     * @brief    Reads back the primary window's offscreen render target.
     *
     * Only supported when the backend was constructed with `headless = true`.
     * Windowed back-buffer capture is not implemented — see DECISIONS.md for
     * the scoping rationale (matches the Phase 20/21 precedent).
     *
     * Row pitch is always de-strided to tightly-packed RGBA8 rows before
     * returning, per the Phase 22 proposal's row de-striding invariant.
     *
     * @return   RGBA8 pixel data, tightly packed, top-to-bottom. Empty if not
     *           initialised or not running headless.
     */
    [[nodiscard]] std::vector<std::byte> ReadPixels() const;

private:
    // ─── Per-window resources ─────────────────────────────────────────────────

    struct WindowData {
        SDL_Window*                 sdlWindow    = nullptr;
        HWND                        hwnd         = nullptr;
        DX12SwapChain                swapChain;
        Microsoft::WRL::ComPtr<ID3D12Resource> headlessTarget; ///< Set only when headless == true.
        UINT                         headlessRtvIndex = UINT_MAX;
        bool                         headless     = false;
        std::vector<FrameResources> frames;
        uint32_t                    frameIndex   = 0;
        bool                         needsResize  = false;
        bool                         frameStarted = false;
        D3D12_RESOURCE_STATES        backBufferState = D3D12_RESOURCE_STATE_PRESENT;
        WindowConfig                 config;
    };

    // ─── Initialisation helpers ───────────────────────────────────────────────
    void       ApplyDpiAwareness();
    void       EnableDebugLayerIfDebug();
    VoidResult InitSDL(const WindowConfig& config);
    VoidResult CreateFactoryAndAdapter();
    VoidResult CreateDeviceAndQueues();
    VoidResult CreateDescriptorHeaps();
    VoidResult CreateSwapChainFor(WindowData& wd, bool headless);
    VoidResult AllocateFrameResources(WindowData& wd);
    VoidResult InitImGui(const WindowConfig& config);

    // ─── Frame helpers ────────────────────────────────────────────────────────
    void Transition(ID3D12GraphicsCommandList7* cmdList, ID3D12Resource* resource,
                     D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
    void HandleResize(WindowData& wd);

    // ─── ReadPixels (headless only) ───────────────────────────────────────────
    bool EnsureReadbackBuffer(UINT64 size);

    // ─── Secondary-window lookup ──────────────────────────────────────────────
    [[nodiscard]] WindowData* FindWindow(WindowHandle handle);
    [[nodiscard]] const WindowData* FindWindow(WindowHandle handle) const;
    [[nodiscard]] WindowHandle HandleForSDLWindow(SDL_WindowID id) const;

    // ─── Core D3D12/DXGI objects (instance lifetime) ──────────────────────────
    Microsoft::WRL::ComPtr<IDXGIFactory4>       _factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1>       _adapter;
    // ID3D12Device4 (not the base ID3D12Device) — CreateCommandList1(), used
    // by FrameResources::Create(), is declared on ID3D12Device4.
    Microsoft::WRL::ComPtr<ID3D12Device4>       _device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  _directQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  _copyQueue; ///< Created for Phase 24; unused internally this phase.
    Microsoft::WRL::ComPtr<ID3D12InfoQueue1>    _infoQueue;
    DWORD                                       _infoQueueCookie = 0;
    bool                                        _tearingSupported = false;

    // ─── Descriptor heaps (shared across all windows and frames) ──────────────
    // ImGui 1.92's DX12 backend allocates/frees its own SRV slots (font and
    // user textures alike) via the SrvDescriptorAllocFn/FreeFn callbacks
    // passed to ImGui_ImplDX12_Init() — no slot is reserved by the backend
    // itself. See DECISIONS.md for the divergence from the proposal's
    // "Expected pattern" (which assumed a single fixed font descriptor).
    DescriptorAllocator _rtvHeap;
    DescriptorAllocator _srvHeap;

    // ─── ReadPixels (headless only) ────────────────────────────────────────────
    Microsoft::WRL::ComPtr<ID3D12Resource> _readbackBuffer;
    UINT64 _readbackBufferSize = 0;
    UINT64 _readbackRowPitch   = 0; ///< 256-byte-aligned row pitch of the last readback.
    UINT   _readbackWidth      = 0;
    UINT   _readbackHeight     = 0;
    // The fence value the primary window's frame slot is signalled to once its
    // most recent EndFrame() (render + readback copy) finishes — ReadPixels()
    // waits on this before reading, matching the other backends' blocking contract.
    uint64_t _lastHeadlessFenceValue = 0;

    // ─── Per-window state ──────────────────────────────────────────────────────
    WindowData _primary;
    bool       _primaryIsHeadless = false;
    std::unordered_map<WindowHandle, WindowData>    _secondaryWindows;
    std::unordered_map<SDL_WindowID, WindowHandle>  _sdlIdToHandle;

    // ─── Input ─────────────────────────────────────────────────────────────────
    std::vector<InputEvent> _inputQueue;
    std::vector<InputEvent> _drainBuffer;

    // ─── Timing ────────────────────────────────────────────────────────────────
    uint64_t _lastPerfCount = 0;

    // ─── State ─────────────────────────────────────────────────────────────────
    bool _initialised    = false;
    bool _sdlInitialised = false; ///< See Backends/SDL3Metal's Shutdown()-guard fix (DECISIONS.md 2026-06-19).
    bool _shouldClose    = false;
    int  _framesInFlight = 2;
    WindowHandle _nextHandle = 1;
};

} // namespace ImFrame::Internal
