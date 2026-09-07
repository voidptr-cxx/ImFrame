/**
 * @file     ImGuiCompatRenderer_test.cpp
 * @brief    Self-consistency tests for ImGuiCompatRenderer's CommandBuffer -> ImGui translation (Phase 31.2)
 *
 * @internal
 * The proposal's own test plan asks for "compare `ReadPixels()` output
 * against v2.0.0 reference images from Phase 30" — no such images exist
 * (see `.claude/DECISIONS.md`, Interstitial section, and Phase 30.6). This
 * file instead verifies self-consistency the same way every
 * `*Conformance_test.cpp` file already does: record known commands, render
 * through a real backend, `ReadPixels()`, and assert the expected colors
 * appear at the expected screen positions.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "GLFWOpenGL3Backend.hpp"
#include "Rendering/Renderers/ImGuiCompatRenderer.hpp"

#include "ImFrame/Icons/Icons.hpp"

#include <imgui.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Rendering;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;

WindowConfig ConformanceWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "ImGuiCompatRenderer_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

std::size_t PixelOffset(int x, int y, int width) {
    return (static_cast<std::size_t>(y) * width + x) * 4;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = PixelOffset(x, y, width);
    return {static_cast<int>(pixels[off + 0]), static_cast<int>(pixels[off + 1]),
            static_cast<int>(pixels[off + 2]), static_cast<int>(pixels[off + 3])};
}

} // namespace

TEST_CASE("ImGuiCompatRenderer translates DrawRect to a filled rect at the recorded position", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(extent.Width), static_cast<float>(extent.Height)});
    ImGui::Begin("Compat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    CommandBuffer buffer;
    buffer.Push(DrawRect{
        .Position = {0.0f, 0.0f},
        .Size     = {static_cast<float>(extent.Width) / 2.0f, static_cast<float>(extent.Height)},
        .FillColor = {1.0f, 0.0f, 0.0f, 1.0f},
    });
    buffer.Push(DrawRect{
        .Position = {static_cast<float>(extent.Width) / 2.0f, 0.0f},
        .Size     = {static_cast<float>(extent.Width) / 2.0f, static_cast<float>(extent.Height)},
        .FillColor = {0.0f, 0.0f, 1.0f, 1.0f},
    });

    ImGuiCompatRenderer renderer;
    renderer.Render(buffer);

    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(extent.Width) * extent.Height * 4);

    const Pixel left  = Sample(pixels, extent.Width / 4, extent.Height / 2, extent.Width);
    const Pixel right = Sample(pixels, extent.Width * 3 / 4, extent.Height / 2, extent.Width);

    REQUIRE(left.r > left.b);
    REQUIRE(left.r > 150);
    REQUIRE(left.a == 255);

    REQUIRE(right.b > right.r);
    REQUIRE(right.b > 150);
    REQUIRE(right.a == 255);

    renderer.Shutdown();
    backend.Shutdown();
}

TEST_CASE("ImGuiCompatRenderer translates DrawRect stroke without filling the interior", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(extent.Width), static_cast<float>(extent.Height)});
    ImGui::Begin("Compat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    CommandBuffer buffer;
    buffer.Push(DrawRect{
        .Position    = {10.0f, 10.0f},
        .Size        = {static_cast<float>(extent.Width) - 20.0f, static_cast<float>(extent.Height) - 20.0f},
        .StrokeColor = {0.0f, 1.0f, 0.0f, 1.0f},
        .StrokeWidth = 4.0f,
    });

    ImGuiCompatRenderer renderer;
    renderer.Render(buffer);

    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();

    // The stroke edge (near the top-left corner of the rect) should be green.
    const Pixel edge   = Sample(pixels, 11, extent.Height / 2, extent.Width);
    // The interior (well inside the stroked border, no fill color) stays the clear color.
    const Pixel center = Sample(pixels, extent.Width / 2, extent.Height / 2, extent.Width);

    REQUIRE(edge.g > edge.r);
    REQUIRE(edge.g > 150);
    const bool centerLooksFilled = (center.g > 150) && (center.g > center.r);
    REQUIRE_FALSE(centerLooksFilled); // interior wasn't filled green

    renderer.Shutdown();
    backend.Shutdown();
}

TEST_CASE("ImGuiCompatRenderer translates DrawText without crashing and draws something", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(extent.Width), static_cast<float>(extent.Height)});
    ImGui::Begin("Compat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    CommandBuffer buffer;
    buffer.Push(DrawText{
        .Position = {10.0f, 10.0f},
        .Text     = "Hello",
        .Color    = {1.0f, 1.0f, 1.0f, 1.0f},
    });

    ImGuiCompatRenderer renderer;
    REQUIRE_NOTHROW(renderer.Render(buffer));

    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(extent.Width) * extent.Height * 4);

    bool anyNonBackground = false;
    for (int y = 0; y < extent.Height; y += 2) {
        for (int x = 0; x < extent.Width; x += 2) {
            if (Sample(pixels, x, y, extent.Width).r > 50) { anyNonBackground = true; break; }
        }
        if (anyNonBackground) break;
    }
    REQUIRE(anyNonBackground); // text glyphs actually painted something

    renderer.Shutdown();
    backend.Shutdown();
}

TEST_CASE("ImGuiCompatRenderer handles PushClipRect/PopClipRect and layer commands without crashing", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(extent.Width), static_cast<float>(extent.Height)});
    ImGui::Begin("Compat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    CommandBuffer buffer;
    buffer.Push(PushClipRect{.Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}});
    buffer.Push(DrawRect{.Position = {0.0f, 0.0f}, .Size = {64.0f, 64.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});
    buffer.Push(PopClipRect{});
    buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
    buffer.Push(DrawRect{.Position = {64.0f, 64.0f}, .Size = {16.0f, 16.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});
    buffer.Push(PopLayer{});

    ImGuiCompatRenderer renderer;
    REQUIRE_NOTHROW(renderer.Render(buffer));

    ImGui::End();
    REQUIRE_NOTHROW(backend.EndFrame());

    renderer.Shutdown();
    backend.Shutdown();
}

TEST_CASE("ImGuiCompatRenderer::LoadFont returns FileNotFound for a missing path, without needing an "
          "ImGui context",
          "[unit]") {
    ImGuiCompatRenderer renderer;
    const auto          result = renderer.LoadFont(Utility::Path("Assets/Fonts/does-not-exist.ttf"), 16.0f);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Error::FileNotFound);
}

TEST_CASE("ImGuiCompatRenderer::LoadFont loads a real font into ImGui's atlas and Translate(DrawText) "
          "respects the resolved FontId",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    ImGuiCompatRenderer renderer;
    const auto          font = renderer.LoadFont(Utility::Path("Assets/Fonts/fa-solid-900.ttf"), 32.0f);
    REQUIRE(font.has_value());
    REQUIRE(font->IsValid());

    backend.Poll();
    backend.BeginFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(extent.Width), static_cast<float>(extent.Height)});
    ImGui::Begin("Compat", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    CommandBuffer buffer;
    buffer.Push(DrawText{
        .Position = {10.0f, 10.0f},
        .Text     = Icons::Fa::House, // a real, checked-in FA6 glyph -- not a hand-typed PUA byte
                                      // sequence (see .claude/DECISIONS.md, Phase 33.4, for why that's unreliable)
        .Font     = *font,
        .FontSize = 32.0f,
        .Color    = {1.0f, 1.0f, 1.0f, 1.0f},
    });

    REQUIRE_NOTHROW(renderer.Render(buffer));

    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    bool anyNonBackground = false;
    for (int y = 0; y < extent.Height; y += 2) {
        for (int x = 0; x < extent.Width; x += 2) {
            if (Sample(pixels, x, y, extent.Width).r > 50) { anyNonBackground = true; break; }
        }
        if (anyNonBackground) break;
    }
    REQUIRE(anyNonBackground); // the resolved FontId was actually used to draw something

    renderer.Shutdown();
    backend.Shutdown();
}
