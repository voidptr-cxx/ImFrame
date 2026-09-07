/**
 * @file     EmscriptenWebGPUBackend.hpp
 * @brief    Browser (Emscripten) WebGPU concrete backend implementation for ImFrame
 *
 * @internal
 * UNVERIFIED — no Emscripten-target build of ImFrame has been attempted this
 * phase (no `wasm32-emscripten` vcpkg triplet for the existing dependency
 * graph — freetype/implot/catch2/imgui — was stood up; see PHASE_STATUS.md
 * and DECISIONS.md for the explicit scoping decision). Written from the
 * verified `emdawnwebgpu` package headers and `<emscripten/html5.h>`'s real
 * struct/function signatures (installed emsdk 6.0.0), but never compiled.
 *
 * A separate class from `DawnWebGPUBackend`, not the same file with
 * `#ifdef __EMSCRIPTEN__` branches — the native backend is SDL3-pervasive
 * (`SDL_Window*` stored by value, `ImGui_ImplSDL3_*` calls throughout); true
 * literal file-sharing as the Phase 23 proposal's Directory Structure
 * implies is impractical. The part of the proposal's "shared WebGPU-logic"
 * claim that *does* hold — instance/adapter/device/queue creation — is
 * factored into `WebGPUDeviceSetup.hpp`/`.cpp` and used by both classes. See
 * DECISIONS.md.
 *
 * No `ImGui_ImplSDL3`-equivalent platform backend exists for Emscripten —
 * DOM event callbacks call ImGui's event-based IO functions
 * (`io.AddMousePosEvent()` etc.) directly, the same way every existing
 * `ImGui_Impl*_ProcessEvent()` does internally.
 *
 * Single window only — `CreateWindow()`/`DestroyWindow()` are no-ops
 * returning `PrimaryWindow`, per the proposal's Multi-Window Support
 * section ("browsers have single-canvas WebGPU contexts").
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "WebGPUDeviceSetup.hpp"
#include "WebGPUSurface.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"

#include <string>
#include <vector>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @class    EmscriptenWebGPUBackend
 * @brief    IBackend implementation rendering WebGPU into an HTML canvas
 *
 * @since    2.3.0
 *
 * @see      IBackend, WindowConfig, DawnWebGPUBackend
 */
class EmscriptenWebGPUBackend final : public IBackend {
public:
    EmscriptenWebGPUBackend() noexcept = default;
    ~EmscriptenWebGPUBackend() override;

    EmscriptenWebGPUBackend(const EmscriptenWebGPUBackend&)            = delete;
    EmscriptenWebGPUBackend& operator=(const EmscriptenWebGPUBackend&) = delete;
    EmscriptenWebGPUBackend(EmscriptenWebGPUBackend&&)                 = delete;
    EmscriptenWebGPUBackend& operator=(EmscriptenWebGPUBackend&&)      = delete;

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

    /// Always returns `PrimaryWindow` — multi-canvas is out of scope this phase.
    WindowHandle CreateWindow(const WindowConfig& config) override;
    /// No-op — there is no secondary window to destroy.
    void DestroyWindow(WindowHandle handle) override;

    std::span<const InputEvent> DrainInputEvents() override;
    NativeGraphicsContext GetNativeGraphicsContext() const override;

private:
    // ─── Initialisation helpers ───────────────────────────────────────────────
    VoidResult CreateSurface();
    VoidResult InitImGui();
    void RegisterDomCallbacks();
    void UnregisterDomCallbacks();

    // ─── Frame helpers ────────────────────────────────────────────────────────
    void PollCanvasResize();
    void ReleaseFrameTexture();

    // ─── DOM event callback trampolines (static, userData == this) ───────────
    static bool OnKeyDown(int eventType, const struct EmscriptenKeyboardEvent* e, void* userData);
    static bool OnKeyUp(int eventType, const struct EmscriptenKeyboardEvent* e, void* userData);
    static bool OnMouseMove(int eventType, const struct EmscriptenMouseEvent* e, void* userData);
    static bool OnMouseDown(int eventType, const struct EmscriptenMouseEvent* e, void* userData);
    static bool OnMouseUp(int eventType, const struct EmscriptenMouseEvent* e, void* userData);
    static bool OnWheel(int eventType, const struct EmscriptenWheelEvent* e, void* userData);
    static bool OnTouchStart(int eventType, const struct EmscriptenTouchEvent* e, void* userData);
    static bool OnTouchMove(int eventType, const struct EmscriptenTouchEvent* e, void* userData);
    static bool OnTouchEnd(int eventType, const struct EmscriptenTouchEvent* e, void* userData);

    // ─── WebGPU objects ────────────────────────────────────────────────────────
    WGPUInstance      _instance              = nullptr;
    WGPUAdapter       _adapter               = nullptr;
    WGPUDevice        _device                = nullptr;
    WGPUQueue         _queue                 = nullptr;
    std::uint32_t     _maxTextureDimension2D = 0;
    WebGPUSurface     _surface;
    WGPUTexture       _currentTexture        = nullptr;
    WGPUTextureView   _currentView           = nullptr;

    // ─── Canvas / config ───────────────────────────────────────────────────────
    std::string  _canvasSelector = "#canvas";
    WindowConfig _config;
    int          _lastCssWidth  = 0;
    int          _lastCssHeight = 0;

    // ─── Input ─────────────────────────────────────────────────────────────────
    std::vector<InputEvent> _inputQueue;
    std::vector<InputEvent> _drainBuffer;

    // ─── State ─────────────────────────────────────────────────────────────────
    bool   _initialised   = false;
    bool   _frameStarted  = false;
    bool   _shouldClose   = false; ///< Never set on Emscripten — a closing tab terminates the module outright.
    double _lastFrameTime = 0.0;  ///< From emscripten_get_now(), milliseconds.
};

} // namespace ImFrame::Internal
