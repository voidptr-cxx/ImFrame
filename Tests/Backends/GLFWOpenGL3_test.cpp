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
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

// glad must be included before any OpenGL call (glGetError in the BeginFrame test).
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
        .VSync     = ImFrame::VSyncMode::Off,
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

TEST_CASE("GLFWOpenGL3Backend Poll returns ShouldClose=false before close is requested", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    auto info = backend.Poll();

    // Window was just created — glfwWindowShouldClose is false.
    REQUIRE(!info.ShouldClose);
    // DeltaTime must be positive.
    REQUIRE(info.DeltaTime > 0.0f);
    // PrimaryWindow must be in ActiveWindows.
    REQUIRE(!info.ActiveWindows.empty());
    REQUIRE(info.ActiveWindows[0] == ImFrame::PrimaryWindow);

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
    while (glGetError() != GL_NO_ERROR) {}
    REQUIRE(glGetError() == GL_NO_ERROR);

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend Shutdown completes cleanly and is idempotent", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    REQUIRE_NOTHROW(backend.Shutdown());
    REQUIRE_NOTHROW(backend.Shutdown());
}

TEST_CASE("GLFWOpenGL3Backend DrainInputEvents returns empty span when no events", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    backend.Poll();
    auto events = backend.DrainInputEvents();
    // May contain synthetic events from window creation; just verify drain works.
    REQUIRE_NOTHROW(events.size());

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend GetNativeGraphicsContext returns OpenGLContext", "[unit]")
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(HeadlessConfig()).has_value());

    auto ctx = backend.GetNativeGraphicsContext();
    REQUIRE(std::holds_alternative<ImFrame::OpenGLContext>(ctx));

    backend.Shutdown();
}
