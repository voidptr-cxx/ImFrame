/**
 * @file     Application.hpp
 * @brief    Top-level application entry point managing the main loop and backend lifetime
 *
 * `Application` is the single entry point for ImFrame application code. It owns
 * the `IBackend`, a `Timer`, a `DockSpace`, and a `WindowManager`. Users build
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
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/App/DockSpace.hpp"
#include "ImFrame/App/Window.hpp"
#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Config.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Utility/Logger.hpp"
#include "ImFrame/Utility/Path.hpp"
#include "ImFrame/Utility/Timer.hpp"
#include "ImFrame/Widgets/PlotContext.hpp"

#if defined(IMF_DEV_TOOLS)
#include "ImFrame/DevTools/LogViewer.hpp"
#include "ImFrame/DevTools/PerfOverlay.hpp"
#include "ImFrame/DevTools/ThemeHotReload.hpp"
#endif

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace ImFrame::Theme       { struct Theme;            }
namespace ImFrame::Rendering  { class  Viewport;          }
namespace ImFrame::Internal   { class  ViewportRegistry;  }
namespace ImFrame::Internal   { class  Reconciler;        }
namespace ImFrame::Internal   { class  IRenderer;         }

namespace ImFrame::App {

// ─── FontConfig ───────────────────────────────────────────────────────────────

/**
 * @struct   FontConfig
 * @brief    Descriptor for a font to be loaded before the first frame
 *
 * @since    0.8.0
 *
 * @example
 * @code
 * app.WithFont({ .path = "assets/Inter-Regular.ttf", .size = 16.0f, .dpiScaled = true });
 * @endcode
 */
struct FontConfig {
    Utility::Path path;                 ///< Path to a .ttf / .otf file. Empty = skip.
    float         size         = 16.0f; ///< Logical size in pixels.
    bool          dpiScaled    = false; ///< If true, `size` is multiplied by `DpiScale()`.
    bool          isIconFont   = false; ///< If true, merged with FA6 glyph range (Phase 9).
    float         glyphOffsetY = 2.0f;  ///< Vertical glyph shift for icon fonts.
};

// ─── Application ──────────────────────────────────────────────────────────────

/**
 * @class    Application
 * @brief    Top-level entry point that owns the backend and drives the render loop
 *
 * Responsibilities:
 * - Owns the `IBackend` (windowing + ImGui integration).
 * - Drives the render loop: `Poll()` → drain input → `OnUpdate` → `BeginFrame()`
 *   → `DockSpace::Begin()` → `OnUi()` → `DockSpace::End()` → `EndFrame()`.
 * - Loads fonts and scales DPI on `Run()` startup.
 * - Manages `WindowManager`, `Timer`, and layout persistence lifetimes.
 *
 * @note     Non-copyable, non-moveable — owns resources and member addresses.
 *
 * @since    0.8.0
 *
 * @see      WindowManager, DockSpace, FontConfig
 */
class Application {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief    Construct with a backend and optional window config.
     *
     * @param[in]  backend  Concrete backend. Must not be null.
     * @param[in]  config   Window configuration. Defaults to 1280×720.
     */
    explicit Application(std::unique_ptr<Internal::IBackend> backend,
                         WindowConfig config = {});

    ~Application() noexcept;

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // ─── Fluent builder ───────────────────────────────────────────────────────

    /**
     * @brief    Queue a font for loading before the first frame.
     *
     * @param[in]  font  Font descriptor.
     * @return   Reference to this Application for chaining.
     */
    Application& WithFont(FontConfig font);

    /**
     * @brief    Set the active theme.
     *
     * @param[in]  theme  Theme to apply. Must outlive `Run()`.
     * @return   Reference to this Application for chaining.
     */
    Application& WithTheme(const ImFrame::Theme::Theme& theme);

    /**
     * @brief    Register a per-frame UI callback.
     *
     * @param[in]  callback  Zero-argument callable invoked every frame.
     * @return   Reference to this Application for chaining.
     */
    Application& OnUi(Utility::Delegate<void()> callback);

    /**
     * @brief    Set the root of the declarative widget tree (Phase 27+).
     *
     * `root.Build()` is called every frame and reconciled against the
     * existing element tree, rendered in the same scope as `OnUi()`'s
     * callback. `root` must outlive the `Application` (or until a different
     * root is set) — only a reference is captured.
     *
     * @tparam   T     A type satisfying `Tree::Component` (has `Build() const`).
     * @param[in] root  The root component instance. Must outlive this `Application`.
     * @return   Reference to this Application for chaining.
     */
    template <Tree::Component T>
    Application& SetRoot(T& root) {
        _rootBuilder = [&root]() -> Tree::Widget { return root.Build(); };
        return *this;
    }

    /**
     * @brief    Register a per-frame update callback.
     *
     * @param[in]  callback  Callable accepting `float deltaTime`.
     * @return   Reference to this Application for chaining.
     */
    Application& OnUpdate(Utility::Delegate<void(float)> callback);

    /**
     * @brief    Register a close-veto callback.
     *
     * Returning `false` vetoes the close. Returning `true` (or omitting) allows it.
     *
     * @param[in]  callback  Callable returning `bool`.
     * @return   Reference to this Application for chaining.
     */
    Application& OnClose(Utility::Delegate<bool()> callback);

    /**
     * @brief    Enable or disable the menu bar area in the dockspace window.
     *
     * @param[in]  enabled  `true` to show the menu bar.
     * @return   Reference to this Application for chaining.
     */
    Application& WithMenuBar(bool enabled = true);

    /**
     * @brief    Swap the renderer used to replay each frame's recorded draw commands.
     *
     * Defaults to `Internal::ImGuiCompatRenderer` (draws via ImGui's own draw lists)
     * if never called. Pass a concrete `Internal::IRenderer` from the backend you
     * linked — e.g. `std::make_unique<Internal::NativeRendererGL3>(Internal::
     * NativeRendererGL3::RenderMode::DeferredReplay)` from `NativeRendererGL3.hpp`
     * (opt-in, requires the `IMF_BUILD_NATIVE_RENDERER` CMake option) — exactly the
     * same "user constructs the concrete `Internal::` type their linked backend
     * provides" pattern already used for the `IBackend` constructor parameter above.
     *
     * @param[in]  renderer  Must not be null.
     * @return   Reference to this Application for chaining.
     */
    Application& UseRenderer(std::unique_ptr<Internal::IRenderer> renderer);

    // ─── Run ──────────────────────────────────────────────────────────────────

    /**
     * @brief    Initialise the backend and run the render loop until the window closes.
     *
     * On Emscripten (UNVERIFIED — see PHASE_STATUS.md), `emscripten_set_main_loop_arg()`
     * hands the render loop to the browser and never returns to this call site;
     * code after `Run()` in `main()` is unreachable there. Not declared
     * `[[noreturn]]` — `emscripten_set_main_loop_arg()` itself isn't, so the
     * compiler cannot prove the trailing `return` statement unreachable, and
     * `[[noreturn]]` on a function whose body provably returns is a build
     * error under this project's `-Werror`/`/WX`.
     *
     * @return   Empty result on success, or an `Error` if `Init()` failed.
     * @throws   Nothing.
     */
    [[nodiscard]] VoidResult Run();

    /**
     * @brief    Execute one complete frame tick.
     *
     * @return   `true` to continue; `false` to exit the render loop.
     */
    [[nodiscard]] bool RunOneFrame();

    // ─── Per-frame queries ────────────────────────────────────────────────────

    /**
     * @brief    Returns the delta time (in seconds) for the current frame.
     *
     * @return   Seconds elapsed since the previous frame. Zero on the first frame.
     */
    [[nodiscard]] float DeltaTime() const noexcept;

    /**
     * @brief    Returns the DPI content scale of the primary monitor.
     *
     * @return   Scale factor ≥ 1.0. Returns 1.0 before `Run()`.
     */
    [[nodiscard]] float DpiScale() const noexcept;

    /**
     * @brief    Returns the monitor refresh interval (seconds) for the current frame.
     *
     * Useful for frame-pacing and animation systems. Returns 1/60 before `Run()`.
     *
     * @return   Seconds per display refresh (e.g. 1/144 for a 144 Hz monitor).
     */
    [[nodiscard]] float DisplayRefreshInterval() const noexcept;

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
     * @return   Reference to the application-owned `DockSpace`.
     */
    [[nodiscard]] DockSpace& GetDockSpace() noexcept;

    // ─── Multi-window API ─────────────────────────────────────────────────────

    /**
     * @brief    Create a secondary window sharing the primary window's GPU context.
     *
     * @param[in]  config  Window configuration.
     * @return   A `WindowHandle` ≥ 1 for the new window, or `PrimaryWindow` on failure.
     */
    [[nodiscard]] WindowHandle CreateSecondaryWindow(WindowConfig config = {});

    /**
     * @brief    Destroy a secondary window.
     *
     * Has no effect if `handle == PrimaryWindow`.
     *
     * @param[in]  handle  Handle from a prior `CreateSecondaryWindow()` call.
     */
    void DestroySecondaryWindow(WindowHandle handle);

    // ─── Headless-specific API ────────────────────────────────────────────────

    /**
     * @brief    Inject an `InputEvent` into the backend's drain queue.
     *
     * Only meaningful when the backend is a `HeadlessBackend`. Silently
     * ignored for other backend types.
     *
     * @param[in]  event  The event to inject.
     */
    void InjectInputEvent(InputEvent event);

    /**
     * @brief    Read the offscreen framebuffer from a `HeadlessBackend`.
     *
     * Returns an RGBA8 pixel buffer of `Width × Height × 4` bytes, or an
     * empty vector if the backend is not a `HeadlessBackend`.
     *
     * @return   Pixel data as a `std::vector<std::byte>`.
     */
    [[nodiscard]] std::vector<std::byte> ReadHeadlessPixels() const;

    // ─── Static factories ─────────────────────────────────────────────────────

    /**
     * @brief    Create an Application backed by the `HeadlessBackend`.
     *
     * The headless backend creates an ImGui context without an OS window,
     * synthesises a 60 Hz frame clock, and supports `InjectInputEvent()` /
     * `ReadHeadlessPixels()` for automated testing.
     *
     * @param[in]  config  Window configuration (used for logical display size).
     * @return   A headless Application.
     */
    [[nodiscard]] static Application CreateHeadless(WindowConfig config = {});

private:
    friend class Rendering::Viewport; ///< Viewport::Show() accesses _viewportRegistry and _backend.

#if defined(__EMSCRIPTEN__)
    /**
     * @brief    Per-tick callback passed to `emscripten_set_main_loop_arg()`.
     *
     * UNVERIFIED — see PHASE_STATUS.md/DECISIONS.md.
     *
     * @param[in]  arg  The owning `Application*`, cast back from `void*`.
     */
    static void EmscriptenMainLoopTick(void* arg);
#endif

    std::unique_ptr<Internal::IBackend>      _backend;
    std::unique_ptr<Internal::ViewportRegistry> _viewportRegistry;
    std::unique_ptr<Internal::Reconciler>    _reconciler;
    std::uint32_t                            _frameIndex = 0;
    WindowConfig                             _config;
    Utility::Timer                          _timer;
    Utility::Config                         _layoutConfig;
    DockSpace                               _dockSpace;
    WindowManager                           _windowManager;
    Widgets::PlotContext                    _plotContext;
    Utility::Delegate<void()>               _onUi;
    Utility::Delegate<void(float)>          _onUpdate;
    Utility::Delegate<bool()>               _onClose;
    Utility::Delegate<Tree::Widget()>        _rootBuilder;
    std::vector<FontConfig>                 _pendingFonts;
    const ImFrame::Theme::Theme*            _pendingTheme            = nullptr;
    bool                                    _themeDirty              = false;
    float                                   _deltaTime               = 0.0f;
    float                                   _displayRefreshInterval  = 1.0f / 60.0f;

#if defined(IMF_DEV_TOOLS)
    std::shared_ptr<Utility::UiSink>           _uiSink;
    std::optional<DevTools::LogViewer>         _logViewer;
    DevTools::PerfOverlay                      _perfOverlay;
    DevTools::ThemeHotReload                   _themeHotReload;
    std::optional<ImFrame::Theme::Theme>       _hotTheme;
#endif
};

} // namespace ImFrame::App
