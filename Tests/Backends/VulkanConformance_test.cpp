/**
 * @file     VulkanConformance_test.cpp
 * @brief    Pixel-level conformance checks for SDL3VulkanBackend's render + ReadPixels pipeline
 *
 * The Phase 20 proposal describes this file as a Vulkan-vs-HeadlessBackend
 * pixel comparison. That comparison is not meaningful yet: HeadlessBackend
 * (Phase 19) never actually renders anything — `ReadPixels()` returns a
 * buffer that is zero-initialised at Init() and never written to. A real
 * CPU rasterizer for HeadlessBackend is Phase 36's SoftwareRenderer (blend2d),
 * not yet implemented. See DECISIONS.md 2026-06-17 for the full reasoning.
 *
 * Until Phase 36 lands, this file instead verifies the Vulkan backend's own
 * render-to-ReadPixels pipeline is self-consistent: colors drawn via ImGui's
 * draw list survive rendering, presentation-image capture, and the BGRA8 to
 * RGBA8 channel swizzle with their identity intact. A red/blue split-screen
 * scene is used specifically because it is sensitive to a channel-order bug —
 * a uniform clear-color check would not catch a swapped R/B swizzle.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-17
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/SDL3Vulkan/SDL3VulkanBackend.hpp"

#include <imgui.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;

WindowConfig ConformanceWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "VulkanConformance_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

// Returns the RGBA8 byte offset for pixel (x, y) in a tightly-packed buffer.
std::size_t PixelOffset(int x, int y, int width) {
    return (static_cast<std::size_t>(y) * width + x) * 4;
}

} // namespace

TEST_CASE("SDL3VulkanBackend ReadPixels preserves color identity through a red/blue split scene", "[vulkan]")
{
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();

    // Left half pure red, right half pure blue — chosen because a swapped
    // R/B channel swizzle would make this test fail in an obvious way that
    // a uniform clear-color check could not detect.
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(WIDTH / 2.0f, HEIGHT), IM_COL32(255, 0, 0, 255));
    dl->AddRectFilled(ImVec2(WIDTH / 2.0f, 0), ImVec2(WIDTH, HEIGHT), IM_COL32(0, 0, 255, 255));

    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(WIDTH) * HEIGHT * 4);

    auto sample = [&](int x, int y) {
        std::size_t off = PixelOffset(x, y, WIDTH);
        struct { int r, g, b, a; } px{
            static_cast<int>(pixels[off + 0]),
            static_cast<int>(pixels[off + 1]),
            static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3]),
        };
        return px;
    };

    auto leftPx  = sample(WIDTH / 4, HEIGHT / 2);
    auto rightPx = sample(WIDTH * 3 / 4, HEIGHT / 2);

    // Left half: red-dominant.
    REQUIRE(leftPx.r > leftPx.b);
    REQUIRE(leftPx.r > 150);
    REQUIRE(leftPx.b < 100);
    REQUIRE(leftPx.a == 255);

    // Right half: blue-dominant.
    REQUIRE(rightPx.b > rightPx.r);
    REQUIRE(rightPx.b > 150);
    REQUIRE(rightPx.r < 100);
    REQUIRE(rightPx.a == 255);

    backend.Shutdown();
}

TEST_CASE("SDL3VulkanBackend ReadPixels of an empty scene is uniform and opaque", "[vulkan]")
{
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    // Deliberately no draw calls — just the swap chain's clear color.
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(WIDTH) * HEIGHT * 4);

    auto sample = [&](int x, int y) {
        std::size_t off = PixelOffset(x, y, WIDTH);
        return std::array<std::byte, 4>{ pixels[off], pixels[off + 1], pixels[off + 2], pixels[off + 3] };
    };

    auto corner = sample(0, 0);
    auto center = sample(WIDTH / 2, HEIGHT / 2);
    auto other  = sample(WIDTH - 1, HEIGHT - 1);

    // All sampled points must match exactly — the clear color is uniform
    // across the whole framebuffer when nothing is drawn.
    REQUIRE(corner == center);
    REQUIRE(corner == other);

    // Alpha is always fully opaque.
    REQUIRE(corner[3] == std::byte{255});

    backend.Shutdown();
}
