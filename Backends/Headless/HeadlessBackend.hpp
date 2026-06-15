/**
 * @file     HeadlessBackend.hpp
 * @brief    Offscreen IBackend implementation for CI tests and server-side rendering
 *
 * `HeadlessBackend` creates an ImGui context without an OS window or a GPU
 * renderer. It maintains an RGBA8 pixel buffer of the configured dimensions,
 * synthesises a 60 Hz frame clock, and exposes `InjectInputEvent()` / `ReadPixels()`
 * for automated testing.
 *
 * Backends/ is not part of the public API — consumers use `Application::CreateHeadless()`
 * (Phase 7) rather than constructing this class directly.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

#include <cstddef>
#include <vector>

namespace ImFrame::Internal {

/**
 * @class    HeadlessBackend
 * @brief    IBackend implementation that runs without an OS window or GPU
 *
 * Suitable for CI screenshot tests, headless server rendering, and unit tests
 * that need a functioning ImGui context without a display.
 *
 * Key behaviours:
 * - `Poll()` always returns `ShouldClose = false` and a synthetic 1/60 s DeltaTime.
 * - `BeginFrame()` / `EndFrame()` drive the ImGui frame lifecycle with no GPU submission.
 * - `ReadPixels()` returns the pixel buffer in RGBA8, top-to-bottom.
 * - `InjectInputEvent()` pushes events into the drain queue for testing.
 * - `DrainInputEvents()` returns and clears the injected event queue.
 *
 * @note     Prefer `Application::CreateHeadless()` over direct construction.
 *
 * @since    1.9.0
 *
 * @see      IBackend, Application::CreateHeadless
 */
class HeadlessBackend final : public IBackend {
public:
    /**
     * @brief  Default constructor. No resources are acquired until Init().
     */
    HeadlessBackend() = default;

    /**
     * @brief  Destructor. Calls Shutdown() if the backend is still initialised.
     */
    ~HeadlessBackend() override;

    HeadlessBackend(const HeadlessBackend&)            = delete;
    HeadlessBackend& operator=(const HeadlessBackend&) = delete;
    HeadlessBackend(HeadlessBackend&&)                 = delete;
    HeadlessBackend& operator=(HeadlessBackend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    /**
     * @brief    Create an ImGui context and build a minimal font atlas.
     *
     * @param[in]  config  Window configuration (used for logical display size).
     * @return   Empty result on success.
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Return synthetic FrameInfo — never signals close.
     *
     * @return   `FrameInfo` with `ShouldClose=false`, `DeltaTime=1/60`,
     *           `DisplayRefreshInterval=1/60`, and `ActiveWindows={PrimaryWindow}`.
     * @throws   Nothing.
     */
    FrameInfo Poll() override;

    /**
     * @brief    Begin an ImGui frame.
     * @param[in]  handle  Ignored (HeadlessBackend manages one context).
     */
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    End the ImGui frame and discard draw data.
     * @param[in]  handle  Ignored.
     */
    void EndFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Destroy the ImGui context and release the pixel buffer.
     */
    void Shutdown() override;

    /**
     * @brief    Returns nullptr — no OS window exists.
     * @return   nullptr always.
     */
    void* NativeHandle() const override { return nullptr; }

    /**
     * @brief    No-op — headless backend has no close state to reset.
     */
    void CancelClose() noexcept override {}

    // ─── IBackend (optional overrides) ────────────────────────────────────────

    /**
     * @brief    Returns 1.0 — headless backend assumes no DPI scaling.
     */
    float WindowDpiScale(WindowHandle = PrimaryWindow) const override { return 1.0f; }

    /**
     * @brief    Returns the configured display dimensions.
     * @param[in]  handle  Ignored.
     */
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Drain injected input events and return them as a span.
     *
     * The returned span is valid until the next call to `DrainInputEvents()`.
     */
    std::span<const InputEvent> DrainInputEvents() override;

    /**
     * @brief    Create a virtual secondary window (no OS window is created).
     *
     * Returns a unique `WindowHandle` ≥ 1 each call, suitable for testing the
     * multi-window event pipeline without a display.
     *
     * @param[in]  config  Ignored.
     * @return   A unique non-zero `WindowHandle`.
     */
    WindowHandle CreateWindow(const WindowConfig& config) override;

    /**
     * @brief    Destroy a virtual secondary window.
     *
     * No-op if the handle is `PrimaryWindow` or unknown.
     *
     * @param[in]  handle  Handle from a prior `CreateWindow()` call.
     */
    void DestroyWindow(WindowHandle handle) override;

    /**
     * @brief    Returns a `HeadlessContext` descriptor for this backend.
     */
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    // ─── Headless-specific API ────────────────────────────────────────────────

    /**
     * @brief    Push an `InputEvent` into the drain queue.
     *
     * The event will be returned by the next `DrainInputEvents()` call.
     * This is the primary mechanism for automated interaction tests.
     *
     * @param[in]  event  The event to inject.
     */
    void InjectInputEvent(InputEvent event);

    /**
     * @brief    Return the current offscreen framebuffer contents.
     *
     * The buffer contains `Width × Height × 4` bytes in RGBA8 format,
     * top-to-bottom. In Phase 19 the buffer is zero-initialised (clear colour);
     * actual pixel content requires a GPU-backed implementation.
     *
     * @return   Pixel data as a `std::vector<std::byte>`. Empty if not initialised.
     */
    [[nodiscard]] std::vector<std::byte> ReadPixels() const;

private:
    bool                    _initialised = false;
    int                     _width       = 0;
    int                     _height      = 0;
    std::vector<std::byte>  _pixelBuffer; ///< RGBA8 offscreen framebuffer.
    std::vector<InputEvent> _inputQueue;  ///< Pending injected events.
    std::vector<InputEvent> _drainBuffer; ///< Staging buffer for DrainInputEvents().
    WindowHandle            _nextHandle  = PrimaryWindow; ///< Counter for secondary window handles.
};

} // namespace ImFrame::Internal
