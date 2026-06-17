/**
 * @file     VulkanBackend_test.cpp
 * @brief    Integration tests for SDL3VulkanBackend against a real Vulkan-capable GPU
 *
 * These tests create a real (hidden) window and a real VkInstance/VkDevice —
 * they require a Vulkan-capable GPU and run under the `vulkan` CTest label,
 * separate from the `unit` label used by VulkanInput_test.cpp.
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

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

WindowConfig TestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "VulkanBackend_test";
    cfg.Width  = 320;
    cfg.Height = 240;
    return cfg;
}

} // namespace

// ─── Init ───────────────────────────────────────────────────────────────────

TEST_CASE("SDL3VulkanBackend Init succeeds on available hardware", "[vulkan]")
{
    SDL3VulkanBackend backend;
    auto result = backend.Init(TestWindowConfig());
    REQUIRE(result.has_value());
    backend.Shutdown();
}

TEST_CASE("SDL3VulkanBackend Init twice returns AlreadyInitialised", "[vulkan]")
{
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    auto second = backend.Init(TestWindowConfig());
    REQUIRE(!second.has_value());
    backend.Shutdown();
}

TEST_CASE("SDL3VulkanBackend Shutdown is safe to call multiple times", "[vulkan]")
{
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    backend.Shutdown();
    backend.Shutdown();
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

TEST_CASE("SDL3VulkanBackend GetNativeGraphicsContext returns a populated VulkanContext", "[vulkan]")
{
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());

    NativeGraphicsContext ctx = backend.GetNativeGraphicsContext();
    REQUIRE(std::holds_alternative<VulkanContext>(ctx));

    const auto& vk = std::get<VulkanContext>(ctx);
    REQUIRE(vk.Instance            != nullptr);
    REQUIRE(vk.PhysicalDevice      != nullptr);
    REQUIRE(vk.Device              != nullptr);
    REQUIRE(vk.GraphicsQueue       != nullptr);
    REQUIRE(vk.ViewportCommandPool != nullptr);
    REQUIRE(vk.DescriptorPool      != nullptr);
    REQUIRE(vk.SwapchainImageFormat != 0);

    backend.Shutdown();
}

// ─── Frame loop ───────────────────────────────────────────────────────────────

TEST_CASE("SDL3VulkanBackend runs several BeginFrame/EndFrame cycles without error", "[vulkan]")
{
    SDL3VulkanBackend backend;
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

// ─── Swap chain recreation ─────────────────────────────────────────────────────

TEST_CASE("SDL3VulkanBackend survives ten consecutive resize-driven swap chain recreations", "[vulkan]")
{
    SDL3VulkanBackend backend;
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

    // Backend must still be usable after repeated recreation.
    backend.Poll();
    backend.BeginFrame();
    ImGui::Begin("After resize");
    ImGui::End();
    backend.EndFrame();

    backend.Shutdown();
}

// ─── Secondary windows ──────────────────────────────────────────────────────────

TEST_CASE("SDL3VulkanBackend secondary window creation and destruction leaves the primary window functional", "[vulkan]")
{
    SDL3VulkanBackend backend;
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
