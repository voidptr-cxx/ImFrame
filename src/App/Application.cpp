/**
 * @file     Application.cpp
 * @brief    Application render loop and subsystem orchestration
 *
 * @internal
 * Implements the Application class: backend lifecycle (Init/Shutdown), font
 * loading, DPI scaling, and the RunOneFrame() sequence. All ImGui calls are
 * confined to this file — they must not appear in Application.hpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Icons/IconFont.hpp"
#include "ImFrame/Overlay/Toast.hpp"
#include "ImFrame/Theme/Theme.hpp"
#include "ImFrame/Widgets/PlotContext.hpp"
#include "HeadlessBackend.hpp"

#include <imgui.h>

namespace ImFrame::App {

// ─── Construction ─────────────────────────────────────────────────────────────

Application::Application(std::unique_ptr<Internal::IBackend> backend, WindowConfig config)
    : _backend(std::move(backend))
    , _config(config)
{}

Application::~Application() = default;

// ─── Fluent builder ───────────────────────────────────────────────────────────

Application& Application::WithFont(FontConfig font) {
    _pendingFonts.push_back(std::move(font));
    return *this;
}

Application& Application::WithTheme(const ImFrame::Theme::Theme& theme) {
    _pendingTheme = &theme;
    _themeDirty   = true;
    return *this;
}

Application& Application::OnUi(Utility::Delegate<void()> callback) {
    _onUi = std::move(callback);
    return *this;
}

Application& Application::OnUpdate(Utility::Delegate<void(float)> callback) {
    _onUpdate = std::move(callback);
    return *this;
}

Application& Application::OnClose(Utility::Delegate<bool()> callback) {
    _onClose = std::move(callback);
    return *this;
}

Application& Application::WithMenuBar(bool enabled) {
    _dockSpace.WithMenuBar(enabled);
    return *this;
}

// ─── Run ──────────────────────────────────────────────────────────────────────

VoidResult Application::Run() {
    // ── Backend initialisation ────────────────────────────────────────────────
    if (auto result = _backend->Init(_config); !result) {
        return result;
    }

    // ImPlot context must be created after the ImGui context.
    _plotContext.Init();

    // ── DPI style scaling (once at startup) ──────────────────────────────────
    const float dpi = _backend->DpiScale();
    if (dpi != 1.0f) {
        ImGui::GetStyle().ScaleAllSizes(dpi);
    }

    // ── Font loading ──────────────────────────────────────────────────────────
    for (const auto& fc : _pendingFonts) {
        if (fc.path.Native().empty()) {
            continue;
        }
        const float actualSize = fc.dpiScaled ? fc.size * dpi : fc.size;
        if (fc.isIconFont) {
            // Result intentionally unused — font pointer is managed by the atlas.
            (void)Icons::IconFont::Load(ImGui::GetIO().Fonts,
                { .path          = fc.path,
                  .sizePixels    = actualSize,
                  .glyphOffsetY  = fc.glyphOffsetY });
        } else {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(fc.path.ToString().c_str(), actualSize);
        }
    }
    _pendingFonts.clear();

    // ── Render loop ───────────────────────────────────────────────────────────
    _lastFrameTime = std::chrono::steady_clock::now();

    while (RunOneFrame()) {}

    // ── Teardown ──────────────────────────────────────────────────────────────
    _windowManager.Clear();
    // ImPlot context must be destroyed before the ImGui context.
    _plotContext.Shutdown();
    _backend->Shutdown();

    return {};
}

// ─── RunOneFrame ──────────────────────────────────────────────────────────────

bool Application::RunOneFrame() {
    // ── Poll for OS events ────────────────────────────────────────────────────
    if (!_backend->Poll()) {
        // Close was requested — check for veto.
        if (_onClose && !_onClose()) {
            _backend->CancelClose();
            return true; // Vetoed: continue the loop.
        }
        return false; // Allowed: exit the loop.
    }

    // ── Delta time ────────────────────────────────────────────────────────────
    const auto now = std::chrono::steady_clock::now();
    _deltaTime     = std::chrono::duration<float>(now - _lastFrameTime).count();
    _lastFrameTime = now;

    // ── Per-frame subsystem tick ──────────────────────────────────────────────
    _timer.Tick(_deltaTime);
    if (_onUpdate) {
        _onUpdate(_deltaTime);
    }

    // ── ImGui frame ───────────────────────────────────────────────────────────
    _backend->BeginFrame();

    // Theme application — dirty flag prevents redundant Apply() calls.
    if (_themeDirty && _pendingTheme) {
        _pendingTheme->Apply();
        _themeDirty = false;
    }

    // ImPlot style is re-applied every frame to stay in sync with the active theme.
    if (_pendingTheme) {
        _plotContext.ApplyTheme(*_pendingTheme);
    }

    _dockSpace.Begin();
    if (_onUi) {
        _onUi();
    }
    Overlay::ToastManager::Instance().Render(_deltaTime);
    _dockSpace.End();

    _backend->EndFrame();

    return true;
}

// ─── Per-frame queries ────────────────────────────────────────────────────────

float Application::DeltaTime() const noexcept {
    return _deltaTime;
}

float Application::DpiScale() const noexcept {
    return _backend ? _backend->DpiScale() : 1.0f;
}

// ─── Subsystem access ─────────────────────────────────────────────────────────

WindowManager& Application::GetWindowManager() noexcept {
    return _windowManager;
}

// ─── Static factories ─────────────────────────────────────────────────────────

Application Application::CreateHeadless(WindowConfig config) {
    return Application(std::make_unique<Internal::HeadlessBackend>(), config);
}

} // namespace ImFrame::App
