/**
 * @file     OpenGL3Conformance_test.cpp
 * @brief    Pixel-level conformance checks for GLFWOpenGL3Backend's render + ReadPixels pipeline (Phase 30.6)
 *
 * `GLFWOpenGL3Backend` had no `ReadPixels()` hook before this phase (unlike
 * Vulkan/DX12/WebGPU/Headless, each of which already exposed one as a
 * backend-specific, non-`IBackend`-virtual test hook) — one was added in
 * `GLFWOpenGL3Backend.{hpp,cpp}` specifically to give this file the same
 * capability. See `.claude/DECISIONS.md` "Phase 30.6" for why this is a
 * legitimate, contained backend addition rather than test-only scope creep.
 *
 * Same self-consistency + widget-tree structure as the other three backends'
 * conformance files: a red/blue split-scene identity check, an empty-scene
 * uniformity check, and (Phase 30.6's actual expansion) `ConformanceApp`
 * rendered across all four built-in themes and several window sizes. See
 * `Tests/Backends/ConformanceApp.hpp`'s file comment for what this
 * deliberately does and does not cover, and `VulkanConformance_test.cpp`'s
 * file comment for why this remains a self-consistency check rather than a
 * true cross-backend pixel-diff (blocked on Phase 36's `SoftwareRenderer`).
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-15
 * @version  2.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ConformanceApp.hpp"
#include "GLFWOpenGL3Backend.hpp"

#include "ImFrame/Theme/Themes/CatppuccinMocha.hpp"
#include "ImFrame/Theme/Themes/Dracula.hpp"
#include "ImFrame/Theme/Themes/Light.hpp"
#include "ImFrame/Theme/Themes/Nord.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <utility>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::ConformanceTest;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;

WindowConfig ConformanceWindowConfig(int width = WIDTH, int height = HEIGHT) {
    WindowConfig cfg{};
    cfg.Title  = "OpenGL3Conformance_test";
    cfg.Width  = width;
    cfg.Height = height;
    return cfg;
}

std::size_t PixelOffset(int x, int y, int width) {
    return (static_cast<std::size_t>(y) * width + x) * 4;
}

/// True if every sampled point is byte-identical (i.e. nothing was drawn beyond the clear color).
bool IsUniform(const std::vector<std::byte>& pixels, int width, int height) {
    auto sample = [&](int x, int y) {
        std::size_t off = PixelOffset(x, y, width);
        return std::array<std::byte, 4>{pixels[off], pixels[off + 1], pixels[off + 2], pixels[off + 3]};
    };
    auto first = sample(0, 0);
    for (int y = 0; y < height; y += std::max(1, height / 8)) {
        for (int x = 0; x < width; x += std::max(1, width / 8)) {
            if (sample(x, y) != first) { return false; }
        }
    }
    return true;
}

bool IsFullyOpaque(const std::vector<std::byte>& pixels, int width, int height) {
    for (int y = 0; y < height; y += std::max(1, height / 8)) {
        for (int x = 0; x < width; x += std::max(1, width / 8)) {
            if (pixels[PixelOffset(x, y, width) + 3] != std::byte{255}) { return false; }
        }
    }
    return true;
}

} // namespace

TEST_CASE("GLFWOpenGL3Backend ReadPixels preserves color identity through a red/blue split scene", "[unit]")
{
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    // The actual framebuffer does not always match the requested WindowConfig
    // exactly (observed on this platform: an OS-level minimum window size can
    // clamp the created window's client height upward) — query the real size
    // rather than assuming it equals WIDTH/HEIGHT.
    const WindowExtent extent = backend.WindowSize();
    const int          w      = extent.Width;
    const int          h      = extent.Height;

    backend.Poll();
    backend.BeginFrame();

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(w / 2.0f, static_cast<float>(h)), IM_COL32(255, 0, 0, 255));
    dl->AddRectFilled(ImVec2(w / 2.0f, 0), ImVec2(static_cast<float>(w), static_cast<float>(h)), IM_COL32(0, 0, 255, 255));

    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(w) * h * 4);

    auto sample = [&](int x, int y) {
        std::size_t off = PixelOffset(x, y, w);
        struct { int r, g, b, a; } px{
            static_cast<int>(pixels[off + 0]),
            static_cast<int>(pixels[off + 1]),
            static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3]),
        };
        return px;
    };

    auto leftPx  = sample(w / 4, h / 2);
    auto rightPx = sample(w * 3 / 4, h / 2);

    REQUIRE(leftPx.r > leftPx.b);
    REQUIRE(leftPx.r > 150);
    REQUIRE(leftPx.b < 100);
    REQUIRE(leftPx.a == 255);

    REQUIRE(rightPx.b > rightPx.r);
    REQUIRE(rightPx.b > 150);
    REQUIRE(rightPx.r < 100);
    REQUIRE(rightPx.a == 255);

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend ReadPixels of an empty scene is uniform and opaque", "[unit]")
{
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(extent.Width) * extent.Height * 4);
    REQUIRE(IsUniform(pixels, extent.Width, extent.Height));
    REQUIRE(IsFullyOpaque(pixels, extent.Width, extent.Height));

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend renders the full ConformanceApp widget tree across all built-in themes", "[unit]")
{
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    const std::array<const Theme::Theme*, 4> themes{
        &Themes::Dracula, &Themes::Nord, &Themes::CatppuccinMocha, &Themes::Light,
    };

    for (const Theme::Theme* theme : themes) {
        backend.Poll();
        backend.BeginFrame();
        theme->Apply();
        REQUIRE_NOTHROW(RenderConformanceApp(extent.Width, extent.Height));
        backend.EndFrame();

        auto pixels = backend.ReadPixels();
        REQUIRE(pixels.size() == static_cast<std::size_t>(extent.Width) * extent.Height * 4);
        REQUIRE_FALSE(IsUniform(pixels, extent.Width, extent.Height)); // the widget tree drew something
        REQUIRE(IsFullyOpaque(pixels, extent.Width, extent.Height));
    }

    backend.Shutdown();
}

TEST_CASE("GLFWOpenGL3Backend renders the full ConformanceApp widget tree across several layout sizes", "[unit]")
{
    constexpr std::array<std::pair<int, int>, 3> sizes{{{128, 128}, {256, 192}, {400, 300}}};

    for (auto [requestedWidth, requestedHeight] : sizes) {
        GLFWOpenGL3Backend backend;
        REQUIRE(backend.Init(ConformanceWindowConfig(requestedWidth, requestedHeight)).has_value());
        const WindowExtent extent = backend.WindowSize();

        backend.Poll();
        backend.BeginFrame();
        REQUIRE_NOTHROW(RenderConformanceApp(extent.Width, extent.Height));
        backend.EndFrame();

        auto pixels = backend.ReadPixels();
        REQUIRE(pixels.size() == static_cast<std::size_t>(extent.Width) * extent.Height * 4);
        REQUIRE_FALSE(IsUniform(pixels, extent.Width, extent.Height));
        REQUIRE(IsFullyOpaque(pixels, extent.Width, extent.Height));

        backend.Shutdown();
    }
}
