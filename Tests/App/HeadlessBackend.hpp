/**
 * @file     HeadlessBackend.hpp
 * @brief    Test-only IBackend with a configurable frame limit
 *
 * @internal
 * Not part of the public API. Used exclusively by the Phase 7 Application and
 * DockSpace tests. Creates an ImGui context without a window or renderer so
 * tests can exercise Application::Run() without a GPU.
 *
 * Key behaviours:
 * - Poll() returns true for the first `maxFrames` frames, then false.
 * - CancelClose() advances the limit by one frame (supports OnClose veto testing).
 * - BeginFrame() increments the frame counter and calls ImGui::NewFrame().
 * - EndFrame() calls ImGui::Render().
 * - Shutdown() destroys the ImGui context.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

#include <imgui.h>

namespace ImFrame::Tests {

/**
 * @class    TestHeadlessBackend
 * @brief    Minimal IBackend for unit tests — no window, no renderer
 *
 * Initialises an ImGui context with docking enabled, builds a minimal font
 * atlas, and simulates a render loop that terminates after `maxFrames` frames.
 *
 * Construct via `std::make_unique<TestHeadlessBackend>(frameCount)` and pass
 * the result to the `Application` constructor.
 */
class TestHeadlessBackend final : public Internal::IBackend {
public:
    /**
     * @param[in]  maxFrames  Number of full frames before Poll() returns false.
     *                        Pass 0 to trigger a close request on the very first Poll().
     */
    explicit TestHeadlessBackend(int maxFrames = 3) : _maxFrames(maxFrames) {}

    ~TestHeadlessBackend() override { Shutdown(); }

    TestHeadlessBackend(const TestHeadlessBackend&)            = delete;
    TestHeadlessBackend& operator=(const TestHeadlessBackend&) = delete;
    TestHeadlessBackend(TestHeadlessBackend&&)                 = delete;
    TestHeadlessBackend& operator=(TestHeadlessBackend&&)      = delete;

    VoidResult Init(const WindowConfig& config) override {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io         = ImGui::GetIO();
        io.ConfigFlags     |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags     |= ImGuiConfigFlags_DockingEnable;
        io.DisplaySize      = ImVec2(static_cast<float>(config.Width),
                                     static_cast<float>(config.Height));
        io.DeltaTime        = 1.0f / 60.0f;

        // Build a minimal font atlas so ImGui::NewFrame() does not assert.
        io.Fonts->AddFontDefault();
        unsigned char* pixels = nullptr;
        int            w      = 0;
        int            h      = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        io.Fonts->SetTexID(static_cast<ImTextureID>(1));

        _initialised = true;
        return {};
    }

    bool Poll() override {
        return _frameCount < _maxFrames;
    }

    void BeginFrame() override {
        ImGui::NewFrame();
        ++_frameCount;
    }

    void EndFrame() override {
        ImGui::Render();
    }

    void Shutdown() override {
        if (_initialised) {
            ImGui::DestroyContext();
            _initialised = false;
        }
    }

    float DpiScale() const override { return 1.0f; }
    void* NativeHandle() const override { return nullptr; }

    /**
     * @brief  Allow one more frame before the next close signal.
     *
     * After a veto, the Application calls this. The frame counter is already at
     * `_maxFrames` (close was signalled), so we advance `_maxFrames` by one to
     * allow exactly one more RunOneFrame() body before requesting close again.
     */
    void CancelClose() noexcept override {
        _maxFrames = _frameCount + 1;
    }

    /// @return  Total number of BeginFrame() calls made so far.
    [[nodiscard]] int FrameCount() const noexcept { return _frameCount; }

private:
    int  _maxFrames   = 3;
    int  _frameCount  = 0;
    bool _initialised = false;
};

} // namespace ImFrame::Tests
