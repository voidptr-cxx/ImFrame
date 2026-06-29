/**
 * @file     GLFWOpenGL3Backend.hpp
 * @brief    GLFW + OpenGL 3.3 Core concrete backend implementation for ImFrame
 *
 * @internal
 * This file is not part of the public ImFrame API. Consumers should use the
 * Application class (Phase 7) rather than instantiating this backend directly.
 * Include path is provided by the ImFrame_GLFWOpenGL3 CMake target.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

#include <unordered_map>
#include <vector>

// Forward-declare the GLFW types so this header does not expose GLFW.
struct GLFWwindow;
struct GLFWmonitor;

namespace ImFrame::Internal {

/**
 * @class    GLFWOpenGL3Backend
 * @brief    IBackend implementation using GLFW for windowing and OpenGL 3.3 for rendering
 *
 * Uses the GLFW platform backend and the OpenGL3 renderer backend from Dear ImGui.
 * Targets OpenGL 3.3 Core Profile with GLAD as the function loader.
 *
 * Input events are collected via GLFW callbacks (installed manually with
 * callback chaining to `ImGui_ImplGlfw_*` functions) and are available via
 * `DrainInputEvents()` each frame. Gamepad state is polled each `Poll()` call
 * and only changes are emitted as `GamepadEvent` values.
 *
 * Multi-window support creates secondary GLFW windows sharing the primary
 * window's OpenGL context. Secondary windows are identified by `WindowHandle`
 * values starting from 1.
 *
 * @since    0.2.0
 *
 * @see      IBackend, WindowConfig
 */
class GLFWOpenGL3Backend final : public IBackend {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief  Default constructor. No resources are acquired until Init().
     */
    GLFWOpenGL3Backend() = default;

    /**
     * @brief  Destructor. Calls Shutdown() if the backend is still initialised.
     */
    ~GLFWOpenGL3Backend() override;

    GLFWOpenGL3Backend(const GLFWOpenGL3Backend&)            = delete;
    GLFWOpenGL3Backend& operator=(const GLFWOpenGL3Backend&) = delete;
    GLFWOpenGL3Backend(GLFWOpenGL3Backend&&)                 = delete;
    GLFWOpenGL3Backend& operator=(GLFWOpenGL3Backend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    /**
     * @brief    Initialise GLFW, create the OpenGL window, load GLAD, and
     *           set up Dear ImGui with manual callback installation.
     *
     * @param[in]  config  Window and feature options.
     * @return   Empty result on success; an `Error` on the first failure.
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Poll OS events, compute delta time, poll gamepads, and return FrameInfo.
     *
     * @return   `FrameInfo` with ShouldClose, DeltaTime, DisplayRefreshInterval,
     *           and the list of active window handles.
     * @throws   Nothing.
     */
    FrameInfo Poll() override;

    /**
     * @brief    Start a new ImGui frame for the specified window.
     *
     * @param[in]  handle  Window to begin. Defaults to PrimaryWindow.
     */
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Render and present the ImGui frame for the specified window.
     *
     * @param[in]  handle  Window to end. Defaults to PrimaryWindow.
     */
    void EndFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Release all resources. Safe to call multiple times.
     */
    void Shutdown() override;

    /**
     * @brief    Returns the underlying GLFWwindow pointer as void*.
     *
     * @return   The GLFWwindow* cast to void*. nullptr if not initialised.
     */
    void* NativeHandle() const override;

    /**
     * @brief    Cancel the pending close request by resetting glfwWindowShouldClose.
     */
    void CancelClose() noexcept override;

    // ─── IBackend (optional overrides) ────────────────────────────────────────

    /**
     * @brief    Returns the DPI content scale of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     * @return   Scale ≥ 1.0. Returns 1.0 if not initialised.
     */
    float WindowDpiScale(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns the client-area size of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window is minimised.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window has input focus.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Create a secondary GLFW window sharing the primary GL context.
     *
     * @param[in]  config  Window configuration for the new window.
     * @return   A unique `WindowHandle` ≥ 1.
     */
    WindowHandle CreateWindow(const WindowConfig& config) override;

    /**
     * @brief    Destroy a secondary window and release its resources.
     *
     * Has no effect when called with `PrimaryWindow`.
     *
     * @param[in]  handle  Handle from a prior `CreateWindow()` call.
     */
    void DestroyWindow(WindowHandle handle) override;

    /**
     * @brief    Return all input events accumulated since the last drain.
     *
     * The span is valid until the next `DrainInputEvents()` call.
     */
    std::span<const InputEvent> DrainInputEvents() override;

    /**
     * @brief    Returns `OpenGLContext{}` — ImGui manages GL state directly.
     */
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    /**
     * @brief    Allocate an OpenGL FBO + RGBA8 texture for Viewport use.
     */
    std::unique_ptr<IViewportFramebuffer> CreateViewportFramebuffer(
        std::uint32_t width, std::uint32_t height) override;

private:
    // ─── GLFW callback thunks ─────────────────────────────────────────────────
    // Static functions retrieve the backend pointer via glfwGetWindowUserPointer
    // and delegate to the corresponding private member handlers.

    static void GlfwKeyCallback(GLFWwindow* w, int key, int sc, int action, int mods);
    static void GlfwCharCallback(GLFWwindow* w, unsigned int cp);
    static void GlfwMouseButtonCallback(GLFWwindow* w, int btn, int action, int mods);
    static void GlfwCursorPosCallback(GLFWwindow* w, double x, double y);
    static void GlfwScrollCallback(GLFWwindow* w, double dx, double dy);
    static void GlfwCursorEnterCallback(GLFWwindow* w, int entered);
    static void GlfwWindowFocusCallback(GLFWwindow* w, int focused);
    static void GlfwWindowSizeCallback(GLFWwindow* w, int width, int height);
    static void GlfwWindowCloseCallback(GLFWwindow* w);
    static void GlfwMonitorCallback(GLFWmonitor* monitor, int event);

    // ─── Gamepad polling ──────────────────────────────────────────────────────

    void PollGamepads();

    // ─── State ────────────────────────────────────────────────────────────────

    struct JoystickState {
        std::vector<float>         axes;
        std::vector<unsigned char> buttons;
    };

    GLFWwindow*  _window           = nullptr; ///< Owned primary GLFW window.
    bool         _viewportsEnabled = false;   ///< Whether multi-viewport is active.
    bool         _initialised      = false;   ///< Guards against double-init/shutdown.
    double       _lastPollTime     = 0.0;     ///< glfwGetTime() at last Poll().
    double       _lastMouseX       = 0.0;     ///< Cursor X from last move event.
    double       _lastMouseY       = 0.0;     ///< Cursor Y from last move event.
    bool         _firstMouseEvent  = true;    ///< True until first cursor-pos callback.

    std::vector<InputEvent>  _inputQueue;   ///< Events accumulated since last drain.
    std::vector<InputEvent>  _drainBuffer;  ///< Staging for DrainInputEvents().
    std::vector<JoystickState> _prevJoystickState; ///< Previous-frame gamepad state.

    std::unordered_map<WindowHandle, GLFWwindow*> _secondaryWindows; ///< Handle→window map.
    WindowHandle _nextHandle = 1; ///< Next handle to assign; PrimaryWindow=0 is reserved.
};

} // namespace ImFrame::Internal
