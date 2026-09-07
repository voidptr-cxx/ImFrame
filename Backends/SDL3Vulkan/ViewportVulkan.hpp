/**
 * @file     ViewportVulkan.hpp
 * @brief    Vulkan IViewportFramebuffer — VkImage + per-frame command buffers
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace ImFrame::Internal {

/**
 * Vulkan offscreen framebuffer for Phase 24 Viewport.
 *
 * Creates a VkImage with COLOR_ATTACHMENT_BIT | SAMPLED_BIT usage, registers
 * it with ImGui via ImGui_ImplVulkan_AddTexture, and allocates one
 * VkCommandBuffer per frame-in-flight from the ViewportCommandPool.
 *
 * Synchronisation (Phase 24): EndRender() calls vkQueueWaitIdle — a CPU stall
 * that is correct but not optimal. Proper semaphore sync is a Phase 24.x task.
 */
class ViewportFramebufferVulkan final : public IViewportFramebuffer {
public:
    ViewportFramebufferVulkan(VkDevice device,
                              VmaAllocator allocator,
                              VkQueue graphicsQueue,
                              VkCommandPool viewportCommandPool,
                              VkDescriptorPool descriptorPool,
                              VkFormat imageFormat,
                              int framesInFlight,
                              uint32_t width, uint32_t height);
    ~ViewportFramebufferVulkan() noexcept override;

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    void Allocate(uint32_t width, uint32_t height);
    void Release() noexcept;

    VkDevice        _device             = VK_NULL_HANDLE;
    VmaAllocator    _allocator          = VK_NULL_HANDLE;
    VkQueue         _graphicsQueue      = VK_NULL_HANDLE;
    VkCommandPool   _viewportCmdPool    = VK_NULL_HANDLE;
    VkDescriptorPool _descriptorPool    = VK_NULL_HANDLE;
    VkFormat         _format            = VK_FORMAT_B8G8R8A8_UNORM;
    int              _framesInFlight    = 2;

    VkImage          _image             = VK_NULL_HANDLE;
    VkImageView      _imageView         = VK_NULL_HANDLE;
    VmaAllocation    _allocation        = VK_NULL_HANDLE;
    VkDescriptorSet  _descriptorSet     = VK_NULL_HANDLE; ///< ImGui texture registration.

    std::vector<VkCommandBuffer> _cmdBufs; ///< One per frame-in-flight.
    uint32_t         _activeFrame       = 0;
    uint32_t         _width             = 0;
    uint32_t         _height            = 0;
};

} // namespace ImFrame::Internal
