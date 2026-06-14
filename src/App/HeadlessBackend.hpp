/**
 * @file     HeadlessBackend.hpp
 * @brief    Null (no-window) IBackend implementation for CreateHeadless()
 *
 * @internal
 * Not part of the public API. Used only by Application::CreateHeadless().
 * This stub creates an ImGui context without a window or renderer, allowing
 * ImFrame applications to run in headless environments (CI, scripting, automation).
 *
 * Phase 19 will replace this with a fully featured HeadlessBackend that supports
 * software rendering and pixel readback.
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

namespace ImFrame::Internal {

/**
 * @class    HeadlessBackend
 * @brief    IBackend stub that creates an ImGui context without an OS window
 *
 * Runs until Poll() returns false (which never happens in the production stub —
 * the application must return true from an OnClose callback to terminate).
 *
 * @internal  Used only by Application::CreateHeadless(). Phase 19 replaces this.
 */
class HeadlessBackend final : public IBackend {
public:
    HeadlessBackend() = default;
    ~HeadlessBackend() override { Shutdown(); }

    HeadlessBackend(const HeadlessBackend&)            = delete;
    HeadlessBackend& operator=(const HeadlessBackend&) = delete;
    HeadlessBackend(HeadlessBackend&&)                 = delete;
    HeadlessBackend& operator=(HeadlessBackend&&)      = delete;

    VoidResult Init(const WindowConfig& config) override {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io         = ImGui::GetIO();
        io.ConfigFlags     |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags     |= ImGuiConfigFlags_DockingEnable;
        io.DisplaySize      = ImVec2(static_cast<float>(config.Width),
                                     static_cast<float>(config.Height));
        io.DeltaTime        = 1.0f / 60.0f;
        io.IniFilename      = nullptr; // Headless backend has no display to restore settings from.

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
        // Production stub runs forever; terminate via OnClose returning true.
        return _initialised;
    }

    void BeginFrame() override {
        ImGui::NewFrame();
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
    void  CancelClose() noexcept override {}

private:
    bool _initialised = false;
};

} // namespace ImFrame::Internal
