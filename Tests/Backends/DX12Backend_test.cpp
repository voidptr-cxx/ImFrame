/**
 * @file     DX12Backend_test.cpp
 * @brief    Integration tests for SDL3DX12Backend against a real D3D12-capable GPU
 *
 * These tests create a real (hidden) window and a real ID3D12Device — they
 * require a D3D12-capable GPU (or WARP) and run under the `dx12` CTest label,
 * separate from the `unit` label used by DX12Input_test.cpp.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3DX12/SDL3DX12Backend.hpp"

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

WindowConfig TestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "DX12Backend_test";
    cfg.Width  = 320;
    cfg.Height = 240;
    return cfg;
}

} // namespace

// ─── Init ───────────────────────────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend Init succeeds on available hardware", "[dx12]")
{
    SDL3DX12Backend backend;
    auto result = backend.Init(TestWindowConfig());
    REQUIRE(result.has_value());
    backend.Shutdown();
}

TEST_CASE("SDL3DX12Backend Init twice returns AlreadyInitialised", "[dx12]")
{
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    auto second = backend.Init(TestWindowConfig());
    REQUIRE(!second.has_value());
    backend.Shutdown();
}

TEST_CASE("SDL3DX12Backend Shutdown is safe to call multiple times", "[dx12]")
{
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    backend.Shutdown();
    backend.Shutdown();
}

TEST_CASE("SDL3DX12Backend Shutdown without a prior Init is a safe no-op", "[dx12]")
{
    SDL3DX12Backend backend;
    backend.Shutdown(); // must not crash — _sdlInitialised guard, see DECISIONS.md 2026-06-19.
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend GetNativeGraphicsContext returns a populated DX12Context", "[dx12]")
{
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    NativeGraphicsContext ctx = backend.GetNativeGraphicsContext();
    REQUIRE(std::holds_alternative<DX12Context>(ctx));

    const auto& dx = std::get<DX12Context>(ctx);
    REQUIRE(dx.Device            != nullptr);
    REQUIRE(dx.CommandQueue      != nullptr);
    REQUIRE(dx.CopyQueue         != nullptr);
    REQUIRE(dx.SrvHeap           != nullptr);
    REQUIRE(dx.SrvDescriptorSize > 0);
    REQUIRE(dx.SwapChainFormat   != 0);
    REQUIRE(dx.FramesInFlight    > 0);

    backend.Shutdown();
}

// ─── Frame loop ───────────────────────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend runs several BeginFrame/EndFrame cycles without error", "[dx12]")
{
    SDL3DX12Backend backend;
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

// ─── Swap chain resize ────────────────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend survives ten consecutive resize-driven ResizeBuffers calls", "[dx12]")
{
    SDL3DX12Backend backend;
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

    // Backend must still be usable after repeated resize.
    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("After resize");
    ImGui::End();
    backend.EndFrame();

    backend.Shutdown();
}

// ─── ReadPixels (headless only) ───────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend headless ReadPixels returns RGBA8 buffer of correct dimensions", "[dx12]")
{
    SDL3DX12Backend backend(/*headless=*/true);
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

TEST_CASE("SDL3DX12Backend headless ReadPixels captures real rendered content, not a zero buffer", "[dx12]")
{
    SDL3DX12Backend backend(/*headless=*/true);
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("ReadPixels");
    ImGui::End();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(!pixels.empty());

    // The configured clear color is non-black (0.06, 0.06, 0.06) — at least
    // one byte in the buffer must be non-zero if the copy captured the
    // actual render target rather than e.g. an uninitialised buffer.
    bool anyNonZero = false;
    for (auto b : pixels) {
        if (b != std::byte{0}) { anyNonZero = true; break; }
    }
    REQUIRE(anyNonZero);

    // Alpha channel (every 4th byte) should be fully opaque.
    REQUIRE(pixels[3] == std::byte{255});

    backend.Shutdown();
}

TEST_CASE("SDL3DX12Backend windowed ReadPixels returns empty (not implemented, see DECISIONS.md)", "[dx12]")
{
    SDL3DX12Backend backend; // headless = false
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    backend.Poll();
    backend.BeginFrame();
    backend.EndFrame();

    auto pixels = backend.ReadPixels();
    REQUIRE(pixels.empty());

    backend.Shutdown();
}

// ─── Secondary windows ──────────────────────────────────────────────────────────

TEST_CASE("SDL3DX12Backend secondary window creation and destruction leaves the primary window functional", "[dx12]")
{
    SDL3DX12Backend backend;
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

    // Primary window must still be usable after the secondary is destroyed.
    backend.Poll();
    backend.BeginFrame(PrimaryWindow);
    ImGui::Begin("Primary again");
    ImGui::End();
    backend.EndFrame(PrimaryWindow);

    backend.Shutdown();
}
