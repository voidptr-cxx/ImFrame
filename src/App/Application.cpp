/**
 * @file     Application.cpp
 * @brief    Application render loop and subsystem orchestration
 *
 * @internal
 * Implements the Application class: backend lifecycle (Init/Shutdown), font
 * loading, DPI scaling, input draining, and the RunOneFrame() sequence.
 * All ImGui calls are confined to this file — they must not appear in Application.hpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.9.0
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

// HeadlessBackend lives in Backends/Headless/ and is compiled into ImFrame.
// ${CMAKE_SOURCE_DIR} is a PRIVATE include dir for ImFrame, so this path resolves.
#include "Backends/Headless/HeadlessBackend.hpp"
#include "App/ApplicationContext.hpp"
#include "Rendering/ViewportRegistry.hpp"
#include "Tree/Reconciler.hpp"

#include <imgui.h>

#if defined(__EMSCRIPTEN__)
// UNVERIFIED — see PHASE_STATUS.md/DECISIONS.md. No Emscripten-target build
// of ImFrame has been attempted; this branch has never been compiled.
#include <emscripten/emscripten.h>
#endif

#if defined(IMF_DEV_TOOLS)
#include "ImFrame/Theme/Themes/Dracula.hpp"
#endif

namespace ImFrame::App {

// ─── Construction ─────────────────────────────────────────────────────────────

Application::Application(std::unique_ptr<Internal::IBackend> backend, WindowConfig config)
    : _backend(std::move(backend))
    , _viewportRegistry(std::make_unique<Internal::ViewportRegistry>())
    , _reconciler(std::make_unique<Internal::Reconciler>())
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

Application& Application::UseRenderer(std::unique_ptr<Internal::IRenderer> renderer) {
    _reconciler->SetRenderer(std::move(renderer));
    return *this;
}

// ─── Run ──────────────────────────────────────────────────────────────────────

VoidResult Application::Run() {
    // ── Backend initialisation ────────────────────────────────────────────────
    if (auto result = _backend->Init(_config); !result) {
        return result;
    }

    _plotContext.Init();

    // ── DPI style scaling (once at startup) ──────────────────────────────────
    const float dpi = _backend->WindowDpiScale();
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
            (void)Icons::IconFont::Load(ImGui::GetIO().Fonts,
                { .path         = fc.path,
                  .sizePixels   = actualSize,
                  .glyphOffsetY = fc.glyphOffsetY });
        } else {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(fc.path.ToString().c_str(), actualSize);
        }
    }
    _pendingFonts.clear();

    _dockSpace.SetConfig(&_layoutConfig);

#if defined(IMF_DEV_TOOLS)
    _uiSink = std::make_shared<Utility::UiSink>(1000);
    Utility::Logger::Instance().AddSink(_uiSink);
    _logViewer.emplace(_uiSink);
    _logViewer->Register(_windowManager);
    _themeHotReload.SetBaseTheme(_pendingTheme ? *_pendingTheme : ImFrame::Themes::Dracula);
    _themeHotReload.Watch(Utility::Path{"Assets/Themes"});
#endif

    // ── Render loop ───────────────────────────────────────────────────────────
#if defined(__EMSCRIPTEN__)
    // The browser owns the main thread's event loop — emscripten_set_main_loop_arg
    // never returns to this call site. Teardown below is therefore unreachable
    // on Emscripten; the process ends only when the browser tab closes. See the
    // Phase 23 proposal's Render Loop section and DECISIONS.md.
    emscripten_set_main_loop_arg(&Application::EmscriptenMainLoopTick, this, 0, true);
#else
    while (RunOneFrame()) {}
#endif

    // ── Teardown ──────────────────────────────────────────────────────────────
    _windowManager.Clear();
    _plotContext.Shutdown();
#if defined(IMF_DEV_TOOLS)
    if (_uiSink) {
        Utility::Logger::Instance().RemoveSink(_uiSink);
    }
#endif
    _viewportRegistry->DestroyAll();
    _backend->Shutdown();

    return {};
}

#if defined(__EMSCRIPTEN__)
void Application::EmscriptenMainLoopTick(void* arg)
{
    auto* self = static_cast<Application*>(arg);
    self->RunOneFrame(); // ShouldClose has no effect on Emscripten — see CancelClose()'s callers.
}
#endif

// ─── RunOneFrame ──────────────────────────────────────────────────────────────

bool Application::RunOneFrame() {
    Internal::SetCurrentApplication(this);

    // ── Poll for OS events and per-frame metadata ─────────────────────────────
    auto info = _backend->Poll();
    _deltaTime              = info.DeltaTime;
    _displayRefreshInterval = info.DisplayRefreshInterval;

    if (info.ShouldClose) {
        if (_onClose && !_onClose()) {
            _backend->CancelClose();
            return true; // Vetoed: continue the loop.
        }
        return false; // Allowed: exit the loop.
    }

    // ── Drain input events ────────────────────────────────────────────────────
    // Events are already forwarded to ImGui by the GLFW callback chaining
    // in the backend. Here we process application-level events (window lifecycle).
    auto events = _backend->DrainInputEvents();
    for (const auto& ev : events) {
        // Phase 19: window-close events for secondary windows are received here.
        // Full EventBus dispatch deferred to Phase 27+.
        (void)ev;
    }

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

    // ── Viewport pre-render (must run before BeginFrame) ─────────────────────
    _viewportRegistry->DispatchRenders(*_backend, _deltaTime, _frameIndex++);

    // ── ImGui frame ───────────────────────────────────────────────────────────
    _backend->BeginFrame();

    if (_themeDirty && _pendingTheme) {
        _pendingTheme->Apply();
        _themeDirty = false;
    }

    if (_pendingTheme) {
        _plotContext.ApplyTheme(*_pendingTheme);
    }

    _dockSpace.Begin();
    if (_onUi) {
        _onUi();
    }
    if (_rootBuilder) {
        const Tree::Widget rootWidget = _rootBuilder();
        _reconciler->Show(rootWidget);
    }
    Overlay::ToastManager::Instance().Render(_deltaTime);
#if defined(IMF_DEV_TOOLS)
    if (_logViewer) { _logViewer->Render(); }
    _perfOverlay.Render(_deltaTime);
#endif
    _dockSpace.End();

    _backend->EndFrame();
    _viewportRegistry->FlipShownFlags();

    return true;
}

// ─── Per-frame queries ────────────────────────────────────────────────────────

float Application::DeltaTime() const noexcept {
    return _deltaTime;
}

float Application::DpiScale() const noexcept {
    return _backend ? _backend->WindowDpiScale() : 1.0f;
}

float Application::DisplayRefreshInterval() const noexcept {
    return _displayRefreshInterval;
}

// ─── Subsystem access ─────────────────────────────────────────────────────────

WindowManager& Application::GetWindowManager() noexcept {
    return _windowManager;
}

DockSpace& Application::GetDockSpace() noexcept {
    return _dockSpace;
}

// ─── Multi-window API ─────────────────────────────────────────────────────────

WindowHandle Application::CreateSecondaryWindow(WindowConfig config) {
    return _backend ? _backend->CreateWindow(config) : PrimaryWindow;
}

void Application::DestroySecondaryWindow(WindowHandle handle) {
    if (_backend) {
        _backend->DestroyWindow(handle);
    }
}

// ─── Headless-specific API ────────────────────────────────────────────────────

void Application::InjectInputEvent(InputEvent event) {
    if (auto* hb = dynamic_cast<Internal::HeadlessBackend*>(_backend.get())) {
        hb->InjectInputEvent(std::move(event));
    }
}

std::vector<std::byte> Application::ReadHeadlessPixels() const {
    if (auto* hb = dynamic_cast<Internal::HeadlessBackend*>(_backend.get())) {
        return hb->ReadPixels();
    }
    return {};
}

// ─── Static factories ─────────────────────────────────────────────────────────

Application Application::CreateHeadless(WindowConfig config) {
    return Application(std::make_unique<Internal::HeadlessBackend>(), config);
}

} // namespace ImFrame::App
