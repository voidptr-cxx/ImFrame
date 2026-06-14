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
 * @version  1.7.0
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

#if defined(IMF_DEV_TOOLS)
#include "ImFrame/Theme/Themes/Dracula.hpp"
#endif

namespace ImFrame::App {

// ─── Construction ─────────────────────────────────────────────────────────────

Application::Application(std::unique_ptr<Internal::IBackend> backend, WindowConfig config)
    : _backend(std::move(backend))
    , _config(config)
{}

Application::~Application() noexcept = default;

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

    // Wire the layout Config into DockSpace so SaveLayout/LoadLayout/ResetLayout work.
    _dockSpace.SetConfig(&_layoutConfig);

#if defined(IMF_DEV_TOOLS)
    // ── DevTools setup ────────────────────────────────────────────────────────
    _uiSink = std::make_shared<Utility::UiSink>(1000);
    Utility::Logger::Instance().AddSink(_uiSink);
    _logViewer.emplace(_uiSink);
    _logViewer->Register(_windowManager);

    // ThemeHotReload watches Assets/Themes/ relative to the working directory.
    // SetBaseTheme uses the active theme if one was set, else Dracula as default.
    _themeHotReload.SetBaseTheme(_pendingTheme ? *_pendingTheme : ImFrame::Themes::Dracula);
    _themeHotReload.Watch(Utility::Path{"Assets/Themes"});
#endif

    // ── Render loop ───────────────────────────────────────────────────────────
    _lastFrameTime = std::chrono::steady_clock::now();

    while (RunOneFrame()) {}

    // ── Teardown ──────────────────────────────────────────────────────────────
    _windowManager.Clear();
    // ImPlot context must be destroyed before the ImGui context.
    _plotContext.Shutdown();
#if defined(IMF_DEV_TOOLS)
    if (_uiSink) {
        Utility::Logger::Instance().RemoveSink(_uiSink);
    }
#endif
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

#if defined(IMF_DEV_TOOLS)
    _themeHotReload.Poll();
    if (auto reloaded = _themeHotReload.TakePending()) {
        _hotTheme     = std::move(reloaded);
        _pendingTheme = &*_hotTheme;
        _themeDirty   = true;
        _themeHotReload.SetBaseTheme(*_hotTheme);
    }
#endif

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
#if defined(IMF_DEV_TOOLS)
    if (_logViewer) { _logViewer->Render(); }
    _perfOverlay.Render(_deltaTime);
#endif
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

DockSpace& Application::GetDockSpace() noexcept {
    return _dockSpace;
}

// ─── Static factories ─────────────────────────────────────────────────────────

Application Application::CreateHeadless(WindowConfig config) {
    return Application(std::make_unique<Internal::HeadlessBackend>(), config);
}

} // namespace ImFrame::App
