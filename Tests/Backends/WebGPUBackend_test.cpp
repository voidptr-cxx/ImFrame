/**
 * @file     WebGPUBackend_test.cpp
 * @brief    Integration tests for DawnWebGPUBackend against a real WebGPU-capable GPU
 *
 * These tests create a real (hidden) window and a real WGPUDevice via Dawn —
 * they require a WebGPU-capable GPU (any desktop GPU since ~2015 via Dawn's
 * D3D12/Metal/Vulkan translation) and run under the `webgpu` CTest label,
 * separate from the `unit` label used by WebGPUInput_test.cpp.
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

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

WindowConfig TestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "WebGPUBackend_test";
    cfg.Width  = 320;
    cfg.Height = 240;
    return cfg;
}

} // namespace

// ─── Init ───────────────────────────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend Init succeeds on available hardware", "[webgpu]")
{
    DawnWebGPUBackend backend;
    auto result = backend.Init(TestWindowConfig());
    REQUIRE(result.has_value());
    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend Init twice returns AlreadyInitialised", "[webgpu]")
{
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    auto second = backend.Init(TestWindowConfig());
    REQUIRE(!second.has_value());
    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend Shutdown is safe to call multiple times", "[webgpu]")
{
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    backend.Shutdown();
    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend Shutdown without a prior Init is a safe no-op", "[webgpu]")
{
    DawnWebGPUBackend backend;
    backend.Shutdown();
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend GetNativeGraphicsContext returns a populated WebGPUContext", "[webgpu]")
{
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    NativeGraphicsContext ctx = backend.GetNativeGraphicsContext();
    REQUIRE(std::holds_alternative<WebGPUContext>(ctx));

    const auto& wgpu = std::get<WebGPUContext>(ctx);
    REQUIRE(wgpu.Device                != nullptr);
    REQUIRE(wgpu.Queue                 != nullptr);
    REQUIRE(wgpu.PreferredFormat        != 0);
    REQUIRE(wgpu.MaxTextureDimension2D > 0);
    REQUIRE(wgpu.IsEmscripten          == false);

    backend.Shutdown();
}

// ─── Frame loop ───────────────────────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend runs several BeginFrame/EndFrame cycles without error", "[webgpu]")
{
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    for (int i = 0; i < 10; ++i) {
        FrameInfo info = backend.Poll();
        REQUIRE(!info.ShouldClose);
        backend.BeginFrame();
        ImGui::Begin("Test");
        ImGui::Text("Frame %d", i);
        ImGui::End();
        backend.EndFrame();
    }

    backend.Shutdown();
}

// ─── Surface resize ───────────────────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend survives ten consecutive resize-driven reconfigures", "[webgpu]")
{
    DawnWebGPUBackend backend;
    auto cfg = TestWindowConfig();
    REQUIRE(backend.Init(cfg).has_value());

    auto* window = static_cast<SDL_Window*>(backend.NativeHandle());
    REQUIRE(window != nullptr);

    for (int i = 0; i < 10; ++i) {
        int w = 320 + (i % 2 == 0 ? 64 : -64);
        SDL_SetWindowSize(window, w, 240);
        backend.Poll();
        backend.BeginFrame();
        ImGui::Begin("Resize");
        ImGui::End();
        backend.EndFrame();
    }

    // Device and queue must survive every reconfigure — reconfigure never recreates them.
    auto ctx = std::get<WebGPUContext>(backend.GetNativeGraphicsContext());
    REQUIRE(ctx.Device != nullptr);
    REQUIRE(ctx.Queue  != nullptr);

    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("After resize");
    ImGui::End();
    backend.EndFrame();

    backend.Shutdown();
}

// ─── ReadPixels (headless only) ───────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend headless ReadPixels returns RGBA8 buffer of correct dimensions", "[webgpu]")
{
    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("ReadPixels");
    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    auto cfg    = TestWindowConfig();
    REQUIRE(pixels.size() == static_cast<std::size_t>(cfg.Width) * cfg.Height * 4);

    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend headless ReadPixels captures real rendered content, not a zero buffer", "[webgpu]")
{
    DawnWebGPUBackend backend(/*headless=*/true);
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("ReadPixels");
    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(!pixels.empty());

    bool anyNonZero = false;
    for (auto b : pixels) {
        if (b != std::byte{0}) { anyNonZero = true; break; }
    }
    REQUIRE(anyNonZero);

    // Alpha channel (every 4th byte) should be fully opaque.
    REQUIRE(pixels[3] == std::byte{255});

    backend.Shutdown();
}

TEST_CASE("DawnWebGPUBackend windowed ReadPixels returns empty (headless only, see DECISIONS.md)", "[webgpu]")
{
    DawnWebGPUBackend backend; // headless = false
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.empty());

    backend.Shutdown();
}

// ─── Secondary windows ──────────────────────────────────────────────────────────

TEST_CASE("DawnWebGPUBackend secondary window creation and destruction leaves the primary window functional", "[webgpu]")
{
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    WindowConfig secondaryCfg{};
    secondaryCfg.Title  = "Secondary";
    secondaryCfg.Width  = 200;
    secondaryCfg.Height = 150;

    WindowHandle secondary = backend.CreateWindow(secondaryCfg);
    REQUIRE(secondary != PrimaryWindow);

    backend.Poll();
    backend.BeginFrame(PrimaryWindow);
    ImGui::Begin("Primary");
    ImGui::End();
    backend.EndFrame(PrimaryWindow);

    backend.BeginFrame(secondary);
    ImGui::Begin("Secondary");
    ImGui::End();
    backend.EndFrame(secondary);

    backend.DestroyWindow(secondary);

    backend.Poll();
    backend.BeginFrame(PrimaryWindow);
    ImGui::Begin("Primary again");
    ImGui::End();
    backend.EndFrame(PrimaryWindow);

    backend.Shutdown();
}
