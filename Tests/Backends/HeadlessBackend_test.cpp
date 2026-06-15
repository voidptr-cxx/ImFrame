/**
 * @file     HeadlessBackend_test.cpp
 * @brief    Unit tests for the HeadlessBackend
 *
 * @internal
 * Tests verify:
 * - Init / Shutdown lifecycle.
 * - Poll returns synthetic FrameInfo with PrimaryWindow active and ShouldClose=false.
 * - DeltaTime approximates 1/60 s.
 * - GetNativeGraphicsContext returns a HeadlessContext with correct dimensions.
 * - ReadPixels returns an RGBA8 buffer of the expected size.
 * - InjectInputEvent / DrainInputEvents round-trip (see also InputEvent_test.cpp).
 *
 * No GPU is needed. HeadlessBackend operates entirely in software.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Backends/InputEvent.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal;

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

WindowConfig TestConfig()
{
    return WindowConfig{
        .Title  = "HeadlessTest",
        .Width  = 320,
        .Height = 240,
    };
}

} // anonymous namespace

// ─── Lifecycle ────────────────────────────────────────────────────────────────

TEST_CASE("HeadlessBackend Init returns success", "[unit]")
{
    HeadlessBackend backend;
    auto result = backend.Init(TestConfig());
    REQUIRE(result.has_value());
    backend.Shutdown();
}

TEST_CASE("HeadlessBackend Shutdown is idempotent", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());
    REQUIRE_NOTHROW(backend.Shutdown());
    REQUIRE_NOTHROW(backend.Shutdown());
}

// ─── Poll / FrameInfo ─────────────────────────────────────────────────────────

TEST_CASE("HeadlessBackend Poll returns ShouldClose=false", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto info = backend.Poll();
    REQUIRE(!info.ShouldClose);

    backend.Shutdown();
}

TEST_CASE("HeadlessBackend Poll DeltaTime is approximately 1/60 s", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto info = backend.Poll();
    REQUIRE(info.DeltaTime == Catch::Approx(1.0f / 60.0f).epsilon(0.001));

    backend.Shutdown();
}

TEST_CASE("HeadlessBackend Poll includes PrimaryWindow in ActiveWindows", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto info = backend.Poll();
    REQUIRE(!info.ActiveWindows.empty());
    REQUIRE(info.ActiveWindows[0] == PrimaryWindow);

    backend.Shutdown();
}

TEST_CASE("HeadlessBackend Poll DisplayRefreshInterval is approximately 1/60 s", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto info = backend.Poll();
    REQUIRE(info.DisplayRefreshInterval == Catch::Approx(1.0f / 60.0f).epsilon(0.001));

    backend.Shutdown();
}

// ─── NativeGraphicsContext ────────────────────────────────────────────────────

TEST_CASE("HeadlessBackend GetNativeGraphicsContext returns HeadlessContext", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto ctx = backend.GetNativeGraphicsContext();
    REQUIRE(std::holds_alternative<HeadlessContext>(ctx));

    backend.Shutdown();
}

TEST_CASE("HeadlessContext dimensions match WindowConfig", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto ctx = backend.GetNativeGraphicsContext();
    const auto& hctx = std::get<HeadlessContext>(ctx);
    REQUIRE(hctx.Width       == 320);
    REQUIRE(hctx.Height      == 240);
    REQUIRE(hctx.PixelFormat == HeadlessPixelFormat::RGBA8);

    backend.Shutdown();
}

// ─── ReadPixels ───────────────────────────────────────────────────────────────

TEST_CASE("HeadlessBackend ReadPixels returns RGBA8 buffer of correct size", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto pixels = backend.ReadPixels();
    // 320 × 240 × 4 bytes (RGBA8)
    REQUIRE(pixels.size() == static_cast<std::size_t>(320 * 240 * 4));

    backend.Shutdown();
}

TEST_CASE("HeadlessBackend ReadPixels buffer is zero-initialised after Init", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    auto pixels = backend.ReadPixels();
    for (const auto byte : pixels) {
        REQUIRE(byte == std::byte{0});
    }

    backend.Shutdown();
}

// ─── BeginFrame / EndFrame ────────────────────────────────────────────────────

TEST_CASE("HeadlessBackend BeginFrame and EndFrame complete without crashing", "[unit]")
{
    HeadlessBackend backend;
    REQUIRE(backend.Init(TestConfig()).has_value());

    backend.Poll();
    REQUIRE_NOTHROW(backend.BeginFrame());
    REQUIRE_NOTHROW(backend.EndFrame());

    backend.Shutdown();
}
