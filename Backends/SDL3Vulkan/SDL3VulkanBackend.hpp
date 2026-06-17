/**
 * @file     SDL3VulkanBackend.hpp
 * @brief    SDL3 + Vulkan 1.3 concrete backend implementation for ImFrame
 *
 * Implements `IBackend` using SDL3 for windowing and Vulkan 1.3 for rendering.
 * Key design properties:
 * - Dynamic rendering (no VkRenderPass / VkFramebuffer objects).
 * - Synchronisation2 (VkImageMemoryBarrier2 / vkCmdPipelineBarrier2).
 * - VulkanMemoryAllocator for all GPU memory — no direct vkAllocateMemory calls.
 * - Per-frame command pools for O(1) reset without tracking individual buffers.
 * - Swap chain recreation via `oldSwapchain` — no vkDeviceWaitIdle on resize.
 *
 * @internal
 * This file is not part of the public ImFrame API.
 * Include path is provided by the ImFrame_SDL3Vulkan CMake target.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "FrameResources.hpp"
#include "SwapChain.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace ImFrame::Internal {

/**
 * @class    SDL3VulkanBackend
 * @brief    IBackend implementation using SDL3 for windowing and Vulkan 1.3 for rendering
 *
 * Targets Vulkan 1.3 with dynamic rendering and synchronisation2. VMA handles
 * all GPU memory allocation. ImGui is integrated via `imgui_impl_sdl3` and
 * `imgui_impl_vulkan`.
 *
 * @since    2.0.0
 *
 * @see      IBackend, WindowConfig
 */
class SDL3VulkanBackend final : public IBackend {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief  Default constructor. No resources are acquired until Init().
     */
    SDL3VulkanBackend() = default;

    /**
     * @brief  Destructor — calls Shutdown() if still initialised.
     */
    ~SDL3VulkanBackend() override;

    SDL3VulkanBackend(const SDL3VulkanBackend&)            = delete;
    SDL3VulkanBackend& operator=(const SDL3VulkanBackend&) = delete;
    SDL3VulkanBackend(SDL3VulkanBackend&&)                 = delete;
    SDL3VulkanBackend& operator=(SDL3VulkanBackend&&)      = delete;

    // ─── IBackend (required) ──────────────────────────────────────────────────

    /**
     * @brief    Initialise SDL3, create the Vulkan instance/device/swap chain, and
     *           bring up the ImGui Vulkan and SDL3 backends.
     *
     * @param[in]  config  Window and feature options.
     * @return   Empty result on success; an `Error` on the first failure.
     * @throws   Nothing.
     */
    VoidResult Init(const WindowConfig& config) override;

    /**
     * @brief    Poll SDL3 events, translate to InputEvents, compute delta time.
     *
     * @return   `FrameInfo` with ShouldClose, DeltaTime, DisplayRefreshInterval,
     *           and the list of active window handles.
     * @throws   Nothing.
     */
    FrameInfo Poll() override;

    /**
     * @brief    Acquire the next swap chain image, begin the command buffer and
     *           dynamic rendering scope, then start an ImGui frame.
     *
     * @param[in]  handle  Window to begin. Defaults to PrimaryWindow.
     */
    void BeginFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Finalise the ImGui frame, render, and present the swap chain image.
     *
     * @param[in]  handle  Window to end. Defaults to PrimaryWindow.
     */
    void EndFrame(WindowHandle handle = PrimaryWindow) override;

    /**
     * @brief    Release all Vulkan and SDL3 resources. Safe to call multiple times.
     */
    void Shutdown() override;

    /**
     * @brief    Returns the primary SDL_Window* as void*.
     *
     * @return   The SDL_Window* cast to void*. nullptr if not initialised.
     */
    void* NativeHandle() const override;

    /**
     * @brief    Cancel the pending close request.
     */
    void CancelClose() noexcept override;

    // ─── IBackend (optional overrides) ────────────────────────────────────────

    /**
     * @brief    Returns the DPI content scale of the specified window.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    float WindowDpiScale(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns the client-area size of the specified window in pixels.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    WindowExtent WindowSize(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window is currently minimised.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsMinimized(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Returns true if the specified window currently has input focus.
     *
     * @param[in]  handle  Window to query. Defaults to PrimaryWindow.
     */
    bool WindowIsFocused(WindowHandle handle = PrimaryWindow) const override;

    /**
     * @brief    Create a secondary SDL3 window with its own swap chain.
     *
     * @param[in]  config  Window configuration for the new window.
     * @return   A unique `WindowHandle` ≥ 1. Returns `PrimaryWindow` on failure.
     */
    WindowHandle CreateWindow(const WindowConfig& config) override;

    /**
     * @brief    Destroy a secondary window, waiting for the GPU to be idle first.
     *
     * Has no effect when called with `PrimaryWindow`.
     *
     * @param[in]  handle  Handle from a prior `CreateWindow()` call.
     */
    void DestroyWindow(WindowHandle handle) override;

    /**
     * @brief    Drain all input events accumulated since the last call.
     */
    std::span<const InputEvent> DrainInputEvents() override;

    /**
     * @brief    Returns a populated `VulkanContext` for Phase 24 Viewport use.
     */
    NativeGraphicsContext GetNativeGraphicsContext() const override;

    /**
     * @brief    Reads back the primary window's just-presented swap chain image.
     *
     * Callable between `EndFrame()` and the next `BeginFrame()` — matches
     * `HeadlessBackend::ReadPixels()`'s contract exactly. Blocks until the GPU
     * finishes all outstanding work on the graphics queue (this is a debug/test
     * utility, not a hot-path call).
     *
     * @return   RGBA8 pixel data, tightly packed, top-to-bottom. Empty if not
     *           initialised, the window is minimized, or the surface does not
     *           support `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`. HDR swap chains
     *           (16-bit float formats) are not currently supported and also
     *           return empty.
     */
    [[nodiscard]] std::vector<std::byte> ReadPixels() const;

private:
    // ─── Per-window resources ─────────────────────────────────────────────────

    struct WindowData {
        SDL_Window*              sdlWindow    = nullptr;
        VkSurfaceKHR             surface      = VK_NULL_HANDLE;
        SwapChain                swapChain;
        std::vector<FrameResources> frames;
        uint32_t                 frameIndex   = 0;    ///< Cycles [0, framesInFlight).
        uint32_t                 imageIndex   = 0;    ///< Current acquired swap chain image.
        bool                     needsResize  = false;
        bool                     frameStarted = false; ///< True after successful AcquireNextImage.
        WindowConfig             config;
    };

    // ─── Initialisation helpers ───────────────────────────────────────────────
    VoidResult InitSDL(const WindowConfig& config);
    VoidResult CreateInstance(const WindowConfig& config);
    VoidResult CreateDebugMessenger();
    VoidResult SelectPhysicalDevice();
    VoidResult CreateDevice();
    VoidResult InitVMA();
    VoidResult CreateDescriptorPool();
    VoidResult CreateViewportCommandPool();
    VoidResult CreateSwapChainFor(WindowData& wd);
    VoidResult AllocateFrameResources(WindowData& wd);
    VoidResult InitImGui(const WindowConfig& config);

    // ─── ReadPixels staging buffer ────────────────────────────────────────────
    VoidResult EnsureReadbackBuffer(VkExtent2D extent);
    void       DestroyReadbackBuffer();

    // ─── Frame helpers ────────────────────────────────────────────────────────
    bool  AcquireNextImage(WindowData& wd);
    void  RecreateSwapChain(WindowData& wd);
    void  SubmitAndPresent(WindowData& wd);
    void  NameObject(VkObjectType type, uint64_t handle, const char* name) const;

    // ─── Secondary-window lookup ──────────────────────────────────────────────
    [[nodiscard]] WindowData* FindWindow(WindowHandle handle);
    [[nodiscard]] const WindowData* FindWindow(WindowHandle handle) const;
    [[nodiscard]] WindowHandle HandleForSDLWindow(SDL_WindowID id) const;

    // ─── Vulkan debug callback ────────────────────────────────────────────────
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void*                                       userdata);

    // ─── Core Vulkan objects (instance lifetime) ──────────────────────────────
    VkInstance               _instance           = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT _debugMessenger     = VK_NULL_HANDLE;
    VkPhysicalDevice         _physicalDevice     = VK_NULL_HANDLE;
    VkDevice                 _device             = VK_NULL_HANDLE;
    VmaAllocator             _vmaAllocator       = VK_NULL_HANDLE;
    VkDescriptorPool         _descriptorPool     = VK_NULL_HANDLE;
    VkCommandPool            _viewportCmdPool    = VK_NULL_HANDLE;
    uint32_t                 _graphicsFamily     = 0;
    uint32_t                 _presentFamily      = 0;
    VkQueue                  _graphicsQueue      = VK_NULL_HANDLE;
    VkQueue                  _presentQueue       = VK_NULL_HANDLE;

    // ─── Extension function pointers ──────────────────────────────────────────
    PFN_vkSetDebugUtilsObjectNameEXT    _pfnSetObjectName       = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT _pfnDestroyDebugMsgr    = nullptr;

    // ─── ReadPixels staging buffer (primary window only) ──────────────────────
    // Populated inside EndFrame() — while the primary window's swap chain image
    // is still owned by the application, before vkQueuePresentKHR releases it
    // back to the presentation engine. Touching a presentable image after
    // present (and before re-acquiring it) is a Vulkan spec violation, so the
    // copy cannot happen lazily inside ReadPixels() itself.
    VkBuffer      _readbackBuffer     = VK_NULL_HANDLE;
    VmaAllocation _readbackAllocation = VK_NULL_HANDLE;
    void*         _readbackMapped     = nullptr;
    VkExtent2D    _readbackExtent     = {};

    // ─── Per-window state ─────────────────────────────────────────────────────
    WindowData  _primary;
    WindowConfig _primaryConfig;
    std::unordered_map<WindowHandle,  WindowData>  _secondaryWindows;
    std::unordered_map<SDL_WindowID, WindowHandle>  _sdlIdToHandle;

    // ─── Input ────────────────────────────────────────────────────────────────
    std::vector<InputEvent> _inputQueue;
    std::vector<InputEvent> _drainBuffer;

    // ─── Timing ───────────────────────────────────────────────────────────────
    uint64_t _lastPerfCount = 0;

    // ─── State ────────────────────────────────────────────────────────────────
    bool _initialised    = false;
    bool _shouldClose    = false;
    int  _framesInFlight = 2;
    WindowHandle _nextHandle = 1;
};

} // namespace ImFrame::Internal
