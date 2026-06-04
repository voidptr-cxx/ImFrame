/**
 * @file     GLFWOpenGL3Backend.cpp
 * @brief    GLFW + OpenGL 3.3 Core backend implementation
 *
 * @internal
 * GLAD must be included before GLFW — the architecture invariant
 * "GLAD before GLFW" is enforced at the top of this file.
 *
 * ImGui backend calls (ImGui_Impl*) are confined to this file and
 * GLFWOpenGL3Backend.hpp. They must never appear in src/ or include/.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
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

#include <cstdio>   // fprintf, stderr
#include <string>   // std::string (to convert std::string_view title for GLFW)

namespace ImFrame::Internal {

// ─── Internal helpers ─────────────────────────────────────────────────────────

namespace {

/**
 * @brief  GLFW error callback logging to stderr.
 *
 * Installed during Init() so that GLFW issues are visible even before the
 * Logger subsystem exists (Logger is a Phase 6 concern).
 *
 * @param[in]  errorCode  GLFW error code.
 * @param[in]  description  Human-readable message from GLFW.
 */
void GlfwErrorCallback(int errorCode, const char* description)
{
    std::fprintf(stderr, "[ImFrame] GLFW error %d: %s\n", errorCode, description);
}

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

GLFWOpenGL3Backend::~GLFWOpenGL3Backend()
{
    Shutdown();
}

// ─── IBackend::Init() ─────────────────────────────────────────────────────────

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

    // Request OpenGL 3.3 Core Profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Hide the window initially so it does not flash before ImGui is ready.
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
    glfwSwapInterval(config.VSync ? 1 : 0);

    // ── GLAD ──────────────────────────────────────────────────────────────────
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(_window);
        _window = nullptr;
        glfwTerminate();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    // Verify OpenGL 3.3 is available (gladLoadGLLoader succeeds even on older
    // drivers that expose a subset of the API).
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

    // When viewports are enabled, ImGui windows outside the main window need
    // their background to blend with the OS window background.
    if (_viewportsEnabled) {
        ImGuiStyle& style                       = ImGui::GetStyle();
        style.WindowRounding                    = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w       = 1.0f;
    }

    if (!ImGui_ImplGlfw_InitForOpenGL(_window, true)) {
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

    // Show the window now that everything is ready.
    glfwShowWindow(_window);

    _initialised = true;
    return {};
}

// ─── IBackend::Poll() ─────────────────────────────────────────────────────────

bool GLFWOpenGL3Backend::Poll()
{
    IMF_ASSERT(_initialised);
    glfwPollEvents();
    return !glfwWindowShouldClose(_window);
}

// ─── IBackend::BeginFrame() ───────────────────────────────────────────────────

void GLFWOpenGL3Backend::BeginFrame()
{
    IMF_ASSERT(_initialised);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// ─── IBackend::EndFrame() ─────────────────────────────────────────────────────

void GLFWOpenGL3Backend::EndFrame()
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
        // Restore the main window's context (it may have changed during
        // multi-viewport rendering).
        glfwMakeContextCurrent(_window);
    }

    glfwSwapBuffers(_window);
}

// ─── IBackend::Shutdown() ─────────────────────────────────────────────────────

void GLFWOpenGL3Backend::Shutdown()
{
    if (!_initialised) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    _window = nullptr;

    glfwTerminate();

    _initialised      = false;
    _viewportsEnabled = false;
}

// ─── IBackend::DpiScale() ─────────────────────────────────────────────────────

float GLFWOpenGL3Backend::DpiScale() const
{
    if (!_initialised) {
        return 1.0f;
    }

    float xScale = 1.0f;
    float yScale = 1.0f;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xScale, &yScale);
    return xScale;
}

// ─── IBackend::NativeHandle() ─────────────────────────────────────────────────

void* GLFWOpenGL3Backend::NativeHandle() const
{
    return _window;
}

// ─── IBackend::CancelClose() ──────────────────────────────────────────────────

void GLFWOpenGL3Backend::CancelClose() noexcept
{
    if (_initialised) {
        glfwSetWindowShouldClose(_window, GLFW_FALSE);
    }
}

} // namespace ImFrame::Internal
