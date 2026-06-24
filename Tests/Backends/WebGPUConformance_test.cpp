/**
 * @file     WebGPUConformance_test.cpp
 * @brief    Pixel-level conformance checks for DawnWebGPUBackend's render + ReadPixels pipeline
 *
 * Registered under the `webgpu` CTest label rather than a separate
 * `conformance` label — matching the established Phase 20-22 precedent.
 *
 * The headless render target is created directly as `WGPUTextureFormat_RGBA8Unorm`
 * (see `DawnWebGPUBackend::_headlessFormat`), so there is no BGRA8<->RGBA8
 * swizzle step to verify — this file instead confirms color identity survives
 * render -> CopyTextureToBuffer -> row de-striding intact.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/DawnWebGPU/DawnWebGPUBackend.hpp"

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
    cfg.Title  = "WebGPUConformance_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

std::size_t PixelOffset(int x, int y, int width) {
    return (static_cast<std::size_t>(y) * width + x) * 4;
}

} // namespace

TEST_CASE("DawnWebGPUBackend headless ReadPixels preserves color identity through a red/blue split scene", "[webgpu]")
{
    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();

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

TEST_CASE("DawnWebGPUBackend headless ReadPixels of an empty scene is uniform and opaque", "[webgpu]")
{
    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
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

    REQUIRE(corner == center);
    REQUIRE(corner == other);
    REQUIRE(corner[3] == std::byte{255});

    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend headless ReadPixels de-strides a width whose row is not 256-byte aligned", "[webgpu]")
{
    // 100 px * 4 bytes = 400 bytes/row — not a multiple of WebGPU's
    // 256-byte copy alignment, so the readback buffer's actual row pitch
    // (512, the next 256-byte multiple) must be stripped back down to a
    // tightly-packed 400-byte row before this function returns.
    constexpr int oddWidth  = 100;
    constexpr int oddHeight = 50;

    WindowConfig cfg{};
    cfg.Title  = "WebGPUConformance_test_odd_width";
    cfg.Width  = oddWidth;
    cfg.Height = oddHeight;

    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(cfg).has_value());

    backend.Poll();
    backend.BeginFrame();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.size() == static_cast<std::size_t>(oddWidth) * oddHeight * 4);

    for (int y = 1; y < oddHeight; ++y) {
        std::size_t off0 = PixelOffset(0, 0, oddWidth);
        std::size_t offY = PixelOffset(0, y, oddWidth);
        REQUIRE(pixels[off0 + 0] == pixels[offY + 0]);
        REQUIRE(pixels[off0 + 1] == pixels[offY + 1]);
        REQUIRE(pixels[off0 + 2] == pixels[offY + 2]);
        REQUIRE(pixels[off0 + 3] == pixels[offY + 3]);
    }

    backend.Shutdown();
}
