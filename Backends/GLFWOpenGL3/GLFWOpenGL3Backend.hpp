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
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

// Forward-declare the GLFW window type so this header does not expose GLFW.
struct GLFWwindow;

namespace ImFrame::Internal {

/**
 * @class    GLFWOpenGL3Backend
 * @brief    IBackend implementation using GLFW for windowing and OpenGL 3.3 for rendering
 *
 * Uses the GLFW platform backend and the OpenGL3 renderer backend from Dear ImGui.
 * Targets OpenGL 3.3 Core Profile with GLAD as the function loader.
 *
 * Lifecycle: construct → Init() → [Poll/BeginFrame/EndFrame loop] → Shutdown().
 * Shutdown() is idempotent — safe to call more than once.
 *
 * @note     A single instance manages exactly one GLFWwindow. Do not share
 *           one instance across threads.
 *
 * @warning  GLAD must be loaded (inside Init()) before any OpenGL call.
 *           This is handled automatically by Init() — do not load GLAD
 *           manually before calling Init().
 *
 * @since    0.2.0
 *
 * @see      IBackend
 * @see      WindowConfig
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

    // Non-copyable, non-movable — owns a GLFWwindow* and ImGui context.
    GLFWOpenGL3Backend(const GLFWOpenGL3Backend&)            = delete;
    GLFWOpenGL3Backend& operator=(const GLFWOpenGL3Backend&) = delete;
    GLFWOpenGL3Backend(GLFWOpenGL3Backend&&)                 = delete;
    GLFWOpenGL3Backend& operator=(GLFWOpenGL3Backend&&)      = delete;

    // ─── IBackend ─────────────────────────────────────────────────────────────

    /**
     * @brief    Initialise GLFW, create the OpenGL window, load GLAD, and
     *           set up Dear ImGui.
     *
     * @param[in]  config  Window and feature options.
     *
     * @return   Empty result on success; an `Error` describing the first
     *           failure encountered.
     *
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Poll OS events for this frame.
     *
     * @return   `true` while the window is open; `false` when close is requested.
     */
    bool Poll() override;

    /**
     * @brief    Start a new ImGui frame (must be called after Poll returns true).
     */
    void BeginFrame() override;

    /**
     * @brief    Render and present the ImGui frame.
     */
    void EndFrame() override;

    /**
     * @brief    Release all resources. Safe to call multiple times.
     */
    void Shutdown() override;

    /**
     * @brief    Returns the DPI content scale of the primary monitor.
     *
     * @return   Scale ≥ 1.0. Returns 1.0 if the backend is not initialised.
     */
    float DpiScale() const override;

    /**
     * @brief    Returns the underlying GLFWwindow pointer as void*.
     *
     * @return   The GLFWwindow* cast to void*. nullptr if not initialised.
     */
    void* NativeHandle() const override;

private:
    GLFWwindow* _window           = nullptr; ///< Owned GLFW window.
    bool        _viewportsEnabled = false;   ///< Whether multi-viewport is active.
    bool        _initialised      = false;   ///< Guards against double-init/shutdown.
};

} // namespace ImFrame::Internal
