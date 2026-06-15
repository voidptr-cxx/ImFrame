/**
 * @file     GLFWOpenGL3Backend.cpp
 * @brief    GLFW + OpenGL 3.3 Core backend implementation
 *
 * @internal
 * GLAD must be included before GLFW — the architecture invariant
 * "GLAD before GLFW" is enforced at the top of this file.
 *
 * Input events are collected by GLFW callbacks installed with install_callbacks=false.
 * Each callback pushes an InputEvent to the internal queue AND chains to the
 * corresponding ImGui_ImplGlfw_* function so ImGui continues to receive input.
 * The application layer drains the queue via DrainInputEvents() each frame.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

// ─── GLAD before GLFW (architecture invariant) ────────────────────────────────
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ─── Dear ImGui ───────────────────────────────────────────────────────────────
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// ─── ImFrame ──────────────────────────────────────────────────────────────────
#include "GLFWOpenGL3Backend.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ImFrame::Internal {

// ─── File-local helpers ───────────────────────────────────────────────────────

namespace {

constexpr float GAMEPAD_DEAD_ZONE  = 0.1f;
constexpr float GAMEPAD_AXIS_EPSILON = 0.001f;

void GlfwErrorCallback(int errorCode, const char* description)
{
    std::fprintf(stderr, "[ImFrame] GLFW error %d: %s\n", errorCode, description);
}

KeyAction GlfwActionToKeyAction(int action) noexcept
{
    if (action == GLFW_PRESS)   return KeyAction::Pressed;
    if (action == GLFW_RELEASE) return KeyAction::Released;
    return KeyAction::Repeated;
}

ModFlags GlfwModsToModFlags(int mods) noexcept
{
    ModFlags f = ModFlags::None;
    if (mods & GLFW_MOD_SHIFT)   f |= ModFlags::Shift;
    if (mods & GLFW_MOD_CONTROL) f |= ModFlags::Ctrl;
    if (mods & GLFW_MOD_ALT)     f |= ModFlags::Alt;
    if (mods & GLFW_MOD_SUPER)   f |= ModFlags::Super;
    return f;
}

GLFWOpenGL3Backend* BackendOf(GLFWwindow* w)
{
    return static_cast<GLFWOpenGL3Backend*>(glfwGetWindowUserPointer(w));
}

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

GLFWOpenGL3Backend::~GLFWOpenGL3Backend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult GLFWOpenGL3Backend::Init(const WindowConfig& config)
{
    if (_initialised) {
        return std::unexpected(Error::AlreadyInitialised);
    }

    if (config.Width <= 0 || config.Height <= 0) {
        return std::unexpected(Error::InvalidArgument);
    }

    // ── GLFW ──────────────────────────────────────────────────────────────────
    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit()) {
        return std::unexpected(Error::WindowCreationFailed);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    _window = glfwCreateWindow(
        config.Width,
        config.Height,
        std::string(config.Title).c_str(),
        nullptr,
        nullptr
    );

    if (!_window) {
        glfwTerminate();
        return std::unexpected(Error::WindowCreationFailed);
    }

    glfwMakeContextCurrent(_window);

    // Apply VSync mode.
    switch (config.VSync) {
        case VSyncMode::Off:      glfwSwapInterval(0);  break;
        case VSyncMode::On:       glfwSwapInterval(1);  break;
        case VSyncMode::Adaptive: glfwSwapInterval(-1); break; // falls back to 1 if unsupported
    }

    // ── GLAD ──────────────────────────────────────────────────────────────────
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(_window);
        _window = nullptr;
        glfwTerminate();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    if (GLVersion.major < 3 || (GLVersion.major == 3 && GLVersion.minor < 3)) {
        glfwDestroyWindow(_window);
        _window = nullptr;
        glfwTerminate();
        return std::unexpected(Error::UnsupportedGLVersion);
    }

    // ── Dear ImGui ────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (config.Docking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    if (config.Viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        _viewportsEnabled = true;
    }

    ImGui::StyleColorsDark();

    if (_viewportsEnabled) {
        ImGuiStyle& style             = ImGui::GetStyle();
        style.WindowRounding          = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // install_callbacks=false — we manage callbacks manually so we can also
    // push InputEvents into _inputQueue for the application layer.
    if (!ImGui_ImplGlfw_InitForOpenGL(_window, false)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(_window);
        _window = nullptr;
        glfwTerminate();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(_window);
        _window = nullptr;
        glfwTerminate();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    // ── Install GLFW callbacks ────────────────────────────────────────────────
    // The user pointer must be set AFTER ImGui_ImplGlfw_InitForOpenGL (with false)
    // because ImGui does not touch the user pointer when install_callbacks=false.
    glfwSetWindowUserPointer(_window, this);
    glfwSetKeyCallback(_window,         GlfwKeyCallback);
    glfwSetCharCallback(_window,         GlfwCharCallback);
    glfwSetMouseButtonCallback(_window,  GlfwMouseButtonCallback);
    glfwSetCursorPosCallback(_window,    GlfwCursorPosCallback);
    glfwSetScrollCallback(_window,       GlfwScrollCallback);
    glfwSetCursorEnterCallback(_window,  GlfwCursorEnterCallback);
    glfwSetWindowFocusCallback(_window,  GlfwWindowFocusCallback);
    glfwSetWindowSizeCallback(_window,   GlfwWindowSizeCallback);
    glfwSetWindowCloseCallback(_window,  GlfwWindowCloseCallback);
    glfwSetMonitorCallback(GlfwMonitorCallback);

    glfwShowWindow(_window);

    _lastPollTime = glfwGetTime();
    _initialised  = true;
    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo GLFWOpenGL3Backend::Poll()
{
    IMF_ASSERT(_initialised);
    glfwPollEvents();
    PollGamepads();

    // ── Delta time ────────────────────────────────────────────────────────────
    double now = glfwGetTime();
    float  dt  = (_lastPollTime > 0.0)
                     ? static_cast<float>(now - _lastPollTime)
                     : 1.0f / 60.0f;
    _lastPollTime = now;

    // ── Display refresh interval ──────────────────────────────────────────────
    float refreshInterval = 1.0f / 60.0f;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            if (mode->refreshRate > 0) {
                refreshInterval = 1.0f / static_cast<float>(mode->refreshRate);
            }
        }
    }

    // ── Active windows ────────────────────────────────────────────────────────
    std::vector<WindowHandle> active;
    active.reserve(1 + _secondaryWindows.size());
    active.push_back(PrimaryWindow);
    for (const auto& [handle, _] : _secondaryWindows) {
        active.push_back(handle);
    }

    return FrameInfo{
        .ShouldClose            = static_cast<bool>(glfwWindowShouldClose(_window)),
        .DeltaTime              = dt,
        .DisplayRefreshInterval = refreshInterval,
        .ActiveWindows          = std::move(active),
    };
}

// ─── BeginFrame ───────────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::BeginFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::EndFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);

    ImGui::Render();

    int displayW = 0;
    int displayH = 0;
    glfwGetFramebufferSize(_window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (_viewportsEnabled) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(_window);
    }

    glfwSwapBuffers(_window);
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::Shutdown()
{
    if (!_initialised) {
        return;
    }

    for (auto& [handle, win] : _secondaryWindows) {
        glfwDestroyWindow(win);
    }
    _secondaryWindows.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    _window = nullptr;

    glfwTerminate();

    _initialised      = false;
    _viewportsEnabled = false;
    _lastPollTime     = 0.0;
    _firstMouseEvent  = true;
    _inputQueue.clear();
    _drainBuffer.clear();
    _prevJoystickState.clear();
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float GLFWOpenGL3Backend::WindowDpiScale(WindowHandle /*handle*/) const
{
    if (!_initialised) {
        return 1.0f;
    }
    float xScale = 1.0f;
    float yScale = 1.0f;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xScale, &yScale);
    return xScale;
}

// ─── WindowSize ───────────────────────────────────────────────────────────────

WindowExtent GLFWOpenGL3Backend::WindowSize(WindowHandle handle) const
{
    if (!_initialised) {
        return {};
    }
    GLFWwindow* win = _window;
    if (handle != PrimaryWindow) {
        auto it = _secondaryWindows.find(handle);
        if (it == _secondaryWindows.end()) return {};
        win = it->second;
    }
    int w = 0;
    int h = 0;
    glfwGetWindowSize(win, &w, &h);
    return WindowExtent{ w, h };
}

// ─── WindowIsMinimized ────────────────────────────────────────────────────────

bool GLFWOpenGL3Backend::WindowIsMinimized(WindowHandle handle) const
{
    if (!_initialised) return false;
    GLFWwindow* win = _window;
    if (handle != PrimaryWindow) {
        auto it = _secondaryWindows.find(handle);
        if (it == _secondaryWindows.end()) return false;
        win = it->second;
    }
    return glfwGetWindowAttrib(win, GLFW_ICONIFIED) != 0;
}

// ─── WindowIsFocused ──────────────────────────────────────────────────────────

bool GLFWOpenGL3Backend::WindowIsFocused(WindowHandle handle) const
{
    if (!_initialised) return false;
    GLFWwindow* win = _window;
    if (handle != PrimaryWindow) {
        auto it = _secondaryWindows.find(handle);
        if (it == _secondaryWindows.end()) return false;
        win = it->second;
    }
    return glfwGetWindowAttrib(win, GLFW_FOCUSED) != 0;
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* GLFWOpenGL3Backend::NativeHandle() const
{
    return _window;
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::CancelClose() noexcept
{
    if (_initialised) {
        glfwSetWindowShouldClose(_window, GLFW_FALSE);
    }
}

// ─── CreateWindow ─────────────────────────────────────────────────────────────

WindowHandle GLFWOpenGL3Backend::CreateWindow(const WindowConfig& config)
{
    IMF_ASSERT(_initialised);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(
        config.Width,
        config.Height,
        std::string(config.Title).c_str(),
        nullptr,
        _window // share GL context with the primary window
    );
    if (!win) {
        return PrimaryWindow; // indicates failure
    }

    glfwShowWindow(win);

    WindowHandle handle = _nextHandle++;
    _secondaryWindows[handle] = win;
    _inputQueue.push_back(WindowResizeEvent{ handle, config.Width, config.Height });
    return handle;
}

// ─── DestroyWindow ────────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::DestroyWindow(WindowHandle handle)
{
    if (handle == PrimaryWindow) {
        return; // primary window is managed by Init()/Shutdown()
    }
    auto it = _secondaryWindows.find(handle);
    if (it == _secondaryWindows.end()) {
        return;
    }
    glfwDestroyWindow(it->second);
    _secondaryWindows.erase(it);
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> GLFWOpenGL3Backend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext GLFWOpenGL3Backend::GetNativeGraphicsContext() const
{
    return OpenGLContext{};
}

// ─── Gamepad polling ──────────────────────────────────────────────────────────

void GLFWOpenGL3Backend::PollGamepads()
{
    for (int joy = GLFW_JOYSTICK_1; joy <= GLFW_JOYSTICK_LAST; ++joy) {
        if (!glfwJoystickPresent(joy)) {
            continue;
        }

        if (static_cast<int>(_prevJoystickState.size()) <= joy) {
            _prevJoystickState.resize(static_cast<std::size_t>(joy) + 1);
        }
        JoystickState& prev = _prevJoystickState[static_cast<std::size_t>(joy)];

        // ── Axes ──────────────────────────────────────────────────────────────
        int          axisCount = 0;
        const float* axes      = glfwGetJoystickAxes(joy, &axisCount);
        if (axes && axisCount > 0) {
            if (static_cast<int>(prev.axes.size()) < axisCount) {
                prev.axes.resize(static_cast<std::size_t>(axisCount), 0.0f);
            }
            for (int i = 0; i < axisCount; ++i) {
                float raw = axes[i];
                float val = (std::abs(raw) < GAMEPAD_DEAD_ZONE) ? 0.0f : raw;
                if (std::abs(val - prev.axes[static_cast<std::size_t>(i)]) > GAMEPAD_AXIS_EPSILON) {
                    prev.axes[static_cast<std::size_t>(i)] = val;
                    _inputQueue.push_back(GamepadEvent{ joy, true, i, val, false });
                }
            }
        }

        // ── Buttons ───────────────────────────────────────────────────────────
        int                  buttonCount = 0;
        const unsigned char* buttons     = glfwGetJoystickButtons(joy, &buttonCount);
        if (buttons && buttonCount > 0) {
            if (static_cast<int>(prev.buttons.size()) < buttonCount) {
                prev.buttons.resize(static_cast<std::size_t>(buttonCount), 0);
            }
            for (int i = 0; i < buttonCount; ++i) {
                unsigned char state = buttons[i];
                if (state != prev.buttons[static_cast<std::size_t>(i)]) {
                    prev.buttons[static_cast<std::size_t>(i)] = state;
                    _inputQueue.push_back(GamepadEvent{ joy, false, i, 0.0f, state == GLFW_PRESS });
                }
            }
        }
    }
}

// ─── GLFW callback thunks ─────────────────────────────────────────────────────

void GLFWOpenGL3Backend::GlfwKeyCallback(GLFWwindow* w, int key, int sc, int action, int mods)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(KeyEvent{
        key, sc,
        GlfwActionToKeyAction(action),
        GlfwModsToModFlags(mods)
    });
    ImGui_ImplGlfw_KeyCallback(w, key, sc, action, mods);
}

void GLFWOpenGL3Backend::GlfwCharCallback(GLFWwindow* w, unsigned int cp)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(TextInputEvent{ static_cast<char32_t>(cp) });
    ImGui_ImplGlfw_CharCallback(w, cp);
}

void GLFWOpenGL3Backend::GlfwMouseButtonCallback(GLFWwindow* w, int btn, int action, int mods)
{
    auto* b = BackendOf(w);
    double cx = 0.0;
    double cy = 0.0;
    glfwGetCursorPos(w, &cx, &cy);
    b->_inputQueue.push_back(MouseButtonEvent{
        btn,
        GlfwActionToKeyAction(action),
        GlfwModsToModFlags(mods),
        static_cast<float>(cx),
        static_cast<float>(cy)
    });
    ImGui_ImplGlfw_MouseButtonCallback(w, btn, action, mods);
}

void GLFWOpenGL3Backend::GlfwCursorPosCallback(GLFWwindow* w, double x, double y)
{
    auto* b   = BackendOf(w);
    float dx  = b->_firstMouseEvent ? 0.0f : static_cast<float>(x - b->_lastMouseX);
    float dy  = b->_firstMouseEvent ? 0.0f : static_cast<float>(y - b->_lastMouseY);
    b->_firstMouseEvent = false;
    b->_lastMouseX = x;
    b->_lastMouseY = y;
    b->_inputQueue.push_back(MouseMoveEvent{
        static_cast<float>(x), static_cast<float>(y), dx, dy
    });
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
}

void GLFWOpenGL3Backend::GlfwScrollCallback(GLFWwindow* w, double dx, double dy)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(MouseScrollEvent{
        static_cast<float>(dx),
        static_cast<float>(dy),
        false // GLFW does not distinguish precise vs coarse scroll
    });
    ImGui_ImplGlfw_ScrollCallback(w, dx, dy);
}

void GLFWOpenGL3Backend::GlfwCursorEnterCallback(GLFWwindow* w, int entered)
{
    // No InputEvent for cursor enter/leave; just forward to ImGui.
    ImGui_ImplGlfw_CursorEnterCallback(w, entered);
}

void GLFWOpenGL3Backend::GlfwWindowFocusCallback(GLFWwindow* w, int focused)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(WindowFocusEvent{ PrimaryWindow, focused != 0 });
    ImGui_ImplGlfw_WindowFocusCallback(w, focused);
}

void GLFWOpenGL3Backend::GlfwWindowSizeCallback(GLFWwindow* w, int width, int height)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(WindowResizeEvent{ PrimaryWindow, width, height });
}

void GLFWOpenGL3Backend::GlfwWindowCloseCallback(GLFWwindow* w)
{
    auto* b = BackendOf(w);
    b->_inputQueue.push_back(WindowCloseRequestEvent{ PrimaryWindow });
    // Do not suppress the GLFW close flag — Application::RunOneFrame() checks it via FrameInfo.
}

void GLFWOpenGL3Backend::GlfwMonitorCallback(GLFWmonitor* monitor, int event)
{
    ImGui_ImplGlfw_MonitorCallback(monitor, event);
}

} // namespace ImFrame::Internal
