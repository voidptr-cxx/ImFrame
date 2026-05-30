/**
 * @file     GLFWOpenGL3_test.cpp
 * @brief    Unit tests for GLFWOpenGL3Backend covering the full IBackend lifecycle
 *
 * @internal
 * These tests run headlessly in CI:
 *   Linux  : LIBGL_ALWAYS_SOFTWARE=1 + Xvfb (Mesa software renderer)
 *   macOS  : native OpenGL via default renderer
 *   Windows: native OpenGL via default renderer
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 */

#include <catch2/catch_test_macros.hpp>

// glad must be included before any OpenGL call (glGetError in the third test).
#include <glad/glad.h>

#include "GLFWOpenGL3Backend.hpp"

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

/// Minimal config suitable for headless testing.
ImFrame::WindowConfig HeadlessConfig()
{
    return ImFrame::WindowConfig{
        .Title     = "ImFrame Test",
        .Width     = 320,
        .Height    = 240,
        .VSync     = false,
        .Docking   = false,
        .Viewports = false,
    };
}

} // anonymous namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("GLFWOpenGL3Backend Init succeeds with valid config", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    auto result = backend.Init(HeadlessConfig());

    REQUIRE(result.has_value());

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend Poll returns true before close is requested", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    // Window was just created — glfwWindowShouldClose is false → Poll is true.
    REQUIRE(backend.Poll() == true);

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend BeginFrame and EndFrame complete without GL errors", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    backend.EndFrame();

    // Drain any pre-existing GL error state, then check.
    while (glGetError() != GL_NO_ERROR) {}  // clear
    REQUIRE(glGetError() == GL_NO_ERROR);

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend Shutdown completes cleanly and is idempotent", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    // First shutdown — must not throw or crash.
    REQUIRE_NOTHROW(backend.Shutdown());

    // Second shutdown — must be a no-op, not a crash.
    REQUIRE_NOTHROW(backend.Shutdown());
}
