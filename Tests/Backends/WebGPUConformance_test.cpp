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
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/DawnWebGPU/DawnWebGPUBackend.hpp"
#include "ConformanceApp.hpp"

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
    cfg.Title  = "WebGPUConformance_test";
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

// ─── Widget-tree conformance (Phase 30.6) ──────────────────────────────────────
//
// Expands this file to cover the full widget tree, per PHASE_30_PROPOSAL.md's
// "Backend Conformance Tests" section. See VulkanConformance_test.cpp's file
// comment and ConformanceApp.hpp's file comment for why this remains a
// self-consistency check rather than a true cross-backend pixel-diff against
// a reference image — that remains blocked on Phase 36's SoftwareRenderer.

TEST_CASE("DawnWebGPUBackend renders the full ConformanceApp widget tree across all built-in themes", "[webgpu]")
{
    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(ConformanceWindowConfig()).has_value());

    const std::array<const Theme::Theme*, 4> themes{
        &Themes::Dracula, &Themes::Nord, &Themes::CatppuccinMocha, &Themes::Light,
    };

    for (const Theme::Theme* theme : themes) {
        backend.Poll();
        backend.BeginFrame();
        theme->Apply();
        REQUIRE_NOTHROW(RenderConformanceApp(WIDTH, HEIGHT));
        backend.EndFrame();

        auto pixels = backend.ReadPixels();
        REQUIRE(pixels.size() == static_cast<std::size_t>(WIDTH) * HEIGHT * 4);
        REQUIRE_FALSE(IsUniform(pixels, WIDTH, HEIGHT)); // the widget tree drew something
        REQUIRE(IsFullyOpaque(pixels, WIDTH, HEIGHT));
    }

    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend renders the full ConformanceApp widget tree across several layout sizes", "[webgpu]")
{
    constexpr std::array<std::pair<int, int>, 3> sizes{{{128, 128}, {256, 192}, {400, 300}}};

    for (auto [width, height] : sizes) {
        DawnWebGPUBackend backend(/*headless=*/true);
        REQUIRE(backend.Init(ConformanceWindowConfig(width, height)).has_value());

        backend.Poll();
        backend.BeginFrame();
        REQUIRE_NOTHROW(RenderConformanceApp(width, height));
        backend.EndFrame();

        auto pixels = backend.ReadPixels();
        REQUIRE(pixels.size() == static_cast<std::size_t>(width) * height * 4);
        REQUIRE_FALSE(IsUniform(pixels, width, height));
        REQUIRE(IsFullyOpaque(pixels, width, height));

        backend.Shutdown();
    }
}
