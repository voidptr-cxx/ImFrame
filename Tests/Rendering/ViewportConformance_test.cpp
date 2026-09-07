/**
 * @file     ViewportConformance_test.cpp
 * @brief    Pixel-level correctness tests for HeadlessViewport
 *
 * @internal
 * Tests verify:
 * - ReadPixels() returns an error before any frame is rendered.
 * - ReadPixels() returns a correctly-sized RGBA8 buffer after at least one render.
 * - An OnRender callback can fill the buffer with a solid colour and ReadPixels()
 *   reflects those writes exactly.
 * - RenderContext pixel dimensions match the explicitly-requested viewport size.
 * - RenderContext::Size matches the requested viewport size.
 * - RenderContext::DeltaTime is positive.
 *
 * Uses FrameLimitedHeadlessBackend (HeadlessBackend subclass with a frame limit)
 * so tests are finite and CreateViewportFramebuffer() is fully available.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Rendering/RenderContext.hpp"
#include "ImFrame/Rendering/Viewport.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Rendering;
using ImFrame::App::Application;

// ─── Test helper ─────────────────────────────────────────────────────────────

namespace {

/// HeadlessBackend subclass that stops the render loop after N full frames.
class FrameLimitedHeadlessBackend final : public Internal::HeadlessBackend {
public:
    explicit FrameLimitedHeadlessBackend(int maxFrames) : _maxFrames(maxFrames) {}

    FrameInfo Poll() override {
        FrameInfo info   = Internal::HeadlessBackend::Poll();
        info.ShouldClose = (_frameCount >= _maxFrames);
        return info;
    }

    void BeginFrame(WindowHandle h = PrimaryWindow) override {
        Internal::HeadlessBackend::BeginFrame(h);
        ++_frameCount;
    }

private:
    int _maxFrames;
    int _frameCount = 0;
};

WindowConfig TestConfig() {
    return WindowConfig{ .Title = "ConformanceTest", .Width = 320, .Height = 240 };
}

} // anonymous namespace

// ─── ReadPixels before first render ──────────────────────────────────────────

TEST_CASE("HeadlessViewport ReadPixels returns error before any render", "[unit]")
{
    HeadlessViewport vp("pre_render");
    auto result = vp.ReadPixels();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == Core::Error::NotInitialised);
}

// ─── Buffer size ──────────────────────────────────────────────────────────────

TEST_CASE("HeadlessViewport ReadPixels returns RGBA8 buffer of correct size", "[unit]")
{
    // FrameLimitedHeadlessBackend(2): frame 0 registers viewport, frame 1 renders.
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("size_check");
    vp.Size({32.0f, 32.0f});

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());

    auto pixels = vp.ReadPixels();
    REQUIRE(pixels.has_value());
    REQUIRE(pixels->size() == 32u * 32u * 4u);
}

// ─── Pixel content ────────────────────────────────────────────────────────────

TEST_CASE("HeadlessViewport ReadPixels reflects solid-colour fill written in OnRender", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("fill_white");
    vp.Size({16.0f, 16.0f});
    vp.OnRender([](const RenderContext& ctx) {
        const auto& hl = std::get<ViewportImageHeadless>(ctx.NativeImage);
        std::fill(hl.Pixels,
                  hl.Pixels + static_cast<std::size_t>(hl.Width) * hl.Height * 4u,
                  std::uint8_t{0xFF});
    });

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());

    auto pixels = vp.ReadPixels();
    REQUIRE(pixels.has_value());
    for (auto b : *pixels) {
        REQUIRE(b == std::byte{0xFF});
    }
}

TEST_CASE("HeadlessViewport pixel buffer is zero-initialised before OnRender writes", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("zero_init");
    vp.Size({8.0f, 8.0f});
    // OnRender registered but writes nothing — buffer starts zero-filled.
    bool renderFired = false;
    vp.OnRender([&](const RenderContext& ctx) {
        renderFired = true;
        const auto& hl = std::get<ViewportImageHeadless>(ctx.NativeImage);
        for (std::uint32_t i = 0; i < hl.Width * hl.Height * 4u; ++i) {
            REQUIRE(hl.Pixels[i] == std::uint8_t{0});
        }
    });

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());
    REQUIRE(renderFired);
}

// ─── RenderContext fields ─────────────────────────────────────────────────────

TEST_CASE("HeadlessViewport RenderContext NativeImage has correct pixel dimensions", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("dims_check");
    vp.Size({80.0f, 60.0f});

    std::uint32_t capturedW = 0;
    std::uint32_t capturedH = 0;
    vp.OnRender([&](const RenderContext& ctx) {
        const auto& hl = std::get<ViewportImageHeadless>(ctx.NativeImage);
        capturedW = hl.Width;
        capturedH = hl.Height;
    });

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());

    REQUIRE(capturedW == 80u);
    REQUIRE(capturedH == 60u);
}

TEST_CASE("HeadlessViewport RenderContext Size matches requested dimensions", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("size_ctx");
    vp.Size({48.0f, 36.0f});

    Widgets::Vec2 capturedSize{0.0f, 0.0f};
    vp.OnRender([&](const RenderContext& ctx) { capturedSize = ctx.Size; });

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());

    REQUIRE(capturedSize.x == 48.0f);
    REQUIRE(capturedSize.y == 36.0f);
}

TEST_CASE("HeadlessViewport RenderContext DeltaTime is positive", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("dt_check");
    vp.Size({32.0f, 32.0f});

    float capturedDt = -1.0f;
    vp.OnRender([&](const RenderContext& ctx) { capturedDt = ctx.DeltaTime; });

    app.OnUi([&] { vp.Show(); });
    REQUIRE(app.Run().has_value());

    REQUIRE(capturedDt > 0.0f);
}
