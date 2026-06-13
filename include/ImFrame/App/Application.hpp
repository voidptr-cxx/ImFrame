/**
 * @file     Application.hpp
 * @brief    Top-level application entry point managing the main loop and backend lifetime
 *
 * `Application` is the single entry point for ImFrame application code. It owns
 * the `IBackend`, the `Timer`, a `DockSpace`, and a `WindowManager`. Users build
 * an `Application`, wire up callbacks with the fluent builder API, and call
 * `Run()` to start the render loop.
 *
 * For a native desktop app (GLFW + OpenGL3):
 * @code
 * #include "GLFWOpenGL3Backend.hpp"
 * #include "ImFrame/App/Application.hpp"
 *
 * auto app = ImFrame::App::Application(
 *     std::make_unique<ImFrame::Internal::GLFWOpenGL3Backend>(),
 *     ImFrame::WindowConfig{ .Title = "My App" }
 * );
 * app.OnUi([]{ ImGui::Text("Hello, ImFrame!"); });
 * app.Run();
 * @endcode
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/App/DockSpace.hpp"
#include "ImFrame/App/Window.hpp"
#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Utility/Config.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Utility/Path.hpp"
#include "ImFrame/Utility/Timer.hpp"
#include "ImFrame/Widgets/PlotContext.hpp"

#include <chrono>
#include <memory>
#include <vector>

// Phase 8 provides the full Theme definition; forward-declare here.
namespace ImFrame::Theme { struct Theme; }

namespace ImFrame::App {

// ─── FontConfig ───────────────────────────────────────────────────────────────

/**
 * @struct   FontConfig
 * @brief    Descriptor for a font to be loaded before the first frame
 *
 * Pass one or more `FontConfig` values to `Application::WithFont()` before
 * calling `Run()`. All queued fonts are merged into `ImGui::GetIO().Fonts`
 * during `Run()` startup before the first frame is rendered.
 *
 * @since    0.8.0
 *
 * @example
 * @code
 * app.WithFont({ .path = "assets/Inter-Regular.ttf", .size = 16.0f, .dpiScaled = true });
 * @endcode
 */
struct FontConfig {
    Utility::Path path;                ///< Path to a .ttf / .otf file. Empty = skip (use ImGui default).
    float         size         = 16.0f; ///< Logical size in pixels.
    bool          dpiScaled    = false; ///< If true, `size` is multiplied by `DpiScale()` at load time.
    bool          isIconFont   = false; ///< If true, font is merged with the FA6 glyph range (Phase 9).
    float         glyphOffsetY = 2.0f;  ///< Vertical glyph shift for icon fonts (FA6 needs ≈ 2 px).
};

// ─── Application ──────────────────────────────────────────────────────────────

/**
 * @class    Application
 * @brief    Top-level entry point that owns the backend and drives the render loop
 *
 * Responsibilities:
 * - Owns the `IBackend` (windowing + ImGui integration).
 * - Drives the render loop: `Poll()` → delta time → `OnUpdate` → `BeginFrame()`
 *   → `DockSpace::Begin()` → `OnUi()` → `DockSpace::End()` → `EndFrame()`.
 * - Loads fonts and scales DPI on `Run()` startup.
 * - Manages `WindowManager` and `Timer` lifetimes.
 *
 * The `OnClose` callback returns `bool`: returning `false` vetoes the close
 * request (used for "unsaved changes" dialogs). The backend's close flag is
 * reset via `IBackend::CancelClose()` and the render loop continues.
 *
 * @note     Non-copyable, non-moveable — owns resources and member addresses.
 *
 * @warning  `Application::Run()` is a blocking call that does not return until
 *           the window is closed (or `OnClose` allows it). On Emscripten
 *           (Phase 23) `Run()` never returns — code after `Run()` in `main()`
 *           is unreachable on that platform.
 *
 * @since    0.8.0
 *
 * @see      WindowManager, DockSpace, FontConfig
 */
class Application {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief    Construct an application with a backend and optional window config.
     *
     * No resources are acquired until `Run()` is called.
     *
     * @param[in]  backend  Concrete backend (e.g. `GLFWOpenGL3Backend`). Must not be null.
     * @param[in]  config   Window configuration. Defaults to a 1280×720 window.
     */
    explicit Application(std::unique_ptr<Internal::IBackend> backend,
                         WindowConfig config = {});

    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // ─── Fluent builder ───────────────────────────────────────────────────────

    /**
     * @brief    Queue a font for loading before the first frame.
     *
     * Multiple fonts may be queued; all are loaded in order during `Run()` startup.
     *
     * @param[in]  font  Font descriptor.
     * @return   Reference to this Application for chaining.
     */
    Application& WithFont(FontConfig font);

    /**
     * @brief    Set the active theme. (Phase 8 fills in the full implementation.)
     *
     * Stores the theme pointer and marks the theme as dirty so that
     * `Theme::Apply()` is called on the next frame.
     *
     * @param[in]  theme  Theme to apply. Must outlive `Run()`.
     * @return   Reference to this Application for chaining.
     */
    Application& WithTheme(const ImFrame::Theme::Theme& theme);

    /**
     * @brief    Register a per-frame UI callback.
     *
     * Called once per frame after `DockSpace::Begin()` and before `DockSpace::End()`.
     * This is where the application renders its panels and menus.
     *
     * @param[in]  callback  Zero-argument callable invoked every frame.
     * @return   Reference to this Application for chaining.
     */
    Application& OnUi(Utility::Delegate<void()> callback);

    /**
     * @brief    Register a per-frame update callback.
     *
     * Called once per frame before `BeginFrame()`, passing the delta time in
     * seconds since the previous frame. Use for game-logic, animations, and
     * time-based subsystem updates.
     *
     * @param[in]  callback  Callable accepting `float deltaTime`.
     * @return   Reference to this Application for chaining.
     */
    Application& OnUpdate(Utility::Delegate<void(float)> callback);

    /**
     * @brief    Register a close-veto callback.
     *
     * Called when the backend reports a close request (e.g. the user clicks ×).
     * Returning `false` vetoes the close and calls `IBackend::CancelClose()` so
     * the render loop continues. Returning `true` (or not registering a callback)
     * allows the close to proceed.
     *
     * @param[in]  callback  Callable returning `bool`.
     * @return   Reference to this Application for chaining.
     */
    Application& OnClose(Utility::Delegate<bool()> callback);

    /**
     * @brief    Enable or disable the menu bar area in the dockspace window.
     *
     * @param[in]  enabled  `true` to show the menu bar (default).
     * @return   Reference to this Application for chaining.
     */
    Application& WithMenuBar(bool enabled = true);

    // ─── Run ──────────────────────────────────────────────────────────────────

    /**
     * @brief    Initialise the backend and run the render loop until the window closes.
     *
     * Sequence on entry:
     * 1. `IBackend::Init(_config)`
     * 2. Scale `ImGui::GetStyle()` by `DpiScale()` (once).
     * 3. Load all queued fonts into `ImGui::GetIO().Fonts`.
     * 4. Loop: `RunOneFrame()` until it returns `false`.
     * 5. `WindowManager::Clear()`.
     * 6. `IBackend::Shutdown()`.
     *
     * @return   Empty result on success, or an `Error` if `Init()` failed.
     * @throws   Nothing.
     */
    [[nodiscard]] VoidResult Run();

    /**
     * @brief    Execute one complete frame tick.
     *
     * Exposed as public to support the Emscripten main-loop pattern
     * (`emscripten_set_main_loop_arg`) in Phase 23, and to simplify tests.
     * Do not call this directly in the native desktop path — use `Run()`.
     *
     * @return   `true` to continue; `false` to exit the render loop.
     */
    bool RunOneFrame();

    // ─── Per-frame queries ────────────────────────────────────────────────────

    /**
     * @brief    Returns the delta time (in seconds) for the current frame.
     *
     * @return   Elapsed seconds since the previous frame. Zero on the first frame.
     */
    [[nodiscard]] float DeltaTime() const noexcept;

    /**
     * @brief    Returns the DPI content scale of the primary monitor.
     *
     * @return   Scale factor from the backend (≥ 1.0). Returns 1.0 before `Run()`.
     */
    [[nodiscard]] float DpiScale() const noexcept;

    // ─── Subsystem access ─────────────────────────────────────────────────────

    /**
     * @brief    Returns the window/panel registry.
     *
     * @return   Reference to the application-owned `WindowManager`.
     */
    [[nodiscard]] WindowManager& GetWindowManager() noexcept;

    /**
     * @brief    Returns the dockspace manager.
     *
     * Provides access to `SaveLayout()`, `LoadLayout()`, `ResetLayout()`, and
     * `ListLayouts()`. All layout methods must be called on the render thread
     * inside an active ImGui frame (i.e. from an `OnUi` callback).
     *
     * @return   Reference to the application-owned `DockSpace`.
     */
    [[nodiscard]] DockSpace& GetDockSpace() noexcept;

    // ─── Static factories ─────────────────────────────────────────────────────

    /**
     * @brief    Create an Application backed by a null (headless) backend.
     *
     * The headless backend initialises an ImGui context without creating an OS
     * window or a renderer. Useful for UI automation, scripting, and unit tests.
     *
     * @note     Fully implemented in Phase 19 (`HeadlessBackend`). This stub
     *           creates a functional but minimal backend.
     *
     * @param[in]  config  Window configuration (used for logical display size).
     * @return   A headless Application.
     */
    [[nodiscard]] static Application CreateHeadless(WindowConfig config = {});

private:
    std::unique_ptr<Internal::IBackend>     _backend;
    WindowConfig                            _config;
    Utility::Timer                          _timer;
    Utility::Config                         _layoutConfig;
    DockSpace                               _dockSpace;
    WindowManager                           _windowManager;
    Widgets::PlotContext                    _plotContext;
    Utility::Delegate<void()>               _onUi;
    Utility::Delegate<void(float)>          _onUpdate;
    Utility::Delegate<bool()>               _onClose;
    std::vector<FontConfig>                 _pendingFonts;
    const ImFrame::Theme::Theme*            _pendingTheme = nullptr;
    bool                                    _themeDirty   = false;
    float                                   _deltaTime    = 0.0f;
    std::chrono::steady_clock::time_point   _lastFrameTime{};
};

} // namespace ImFrame::App
