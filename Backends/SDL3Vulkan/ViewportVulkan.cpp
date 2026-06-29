/**
 * @file     ViewportVulkan.cpp
 * @brief    Vulkan viewport framebuffer — VkImage + command buffers + ImGui registration
 *
 * Allocates a device-local VkImage via VMA, creates a VkImageView, and registers
 * the image with ImGui via ImGui_ImplVulkan_AddTexture so it can be used as an
 * ImTextureID in ImGui::Image() calls.
 *
 * Per-frame pattern:
 *   BeginRender  — reset the per-frame command buffer, transition image to
 *                  COLOR_ATTACHMENT_OPTIMAL, begin recording.
 *   OnRender     — user records into the command buffer.
 *   EndRender    — end recording, submit, vkQueueWaitIdle (CPU stall, Phase 24).
 *                  Then transition to SHADER_READ_ONLY_OPTIMAL via a one-shot cmd.
 *
 * Phase 24.x: replace the vkQueueWaitIdle with a semaphore signalled here and
 * waited on in BeginFrame(), enabling true GPU-pipeline parallelism.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <imgui_impl_vulkan.h>

#include "ViewportVulkan.hpp"

namespace ImFrame::Internal {

ViewportFramebufferVulkan::ViewportFramebufferVulkan(
    VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue,
    VkCommandPool viewportCommandPool, VkDescriptorPool descriptorPool,
    VkFormat imageFormat, int framesInFlight, uint32_t width, uint32_t height)
    : _device(device)
    , _allocator(allocator)
    , _graphicsQueue(graphicsQueue)
    , _viewportCmdPool(viewportCommandPool)
    , _descriptorPool(descriptorPool)
    , _format(imageFormat)
    , _framesInFlight(framesInFlight) {

    // Allocate per-frame command buffers from the ViewportCommandPool.
    _cmdBufs.resize(static_cast<std::size_t>(_framesInFlight), VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = _viewportCmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(_framesInFlight);
    vkAllocateCommandBuffers(_device, &allocInfo, _cmdBufs.data());

    Allocate(width, height);
}

ViewportFramebufferVulkan::~ViewportFramebufferVulkan() noexcept {
    Release();
    if (!_cmdBufs.empty() && _device && _viewportCmdPool) {
        vkFreeCommandBuffers(_device, _viewportCmdPool,
                             static_cast<uint32_t>(_cmdBufs.size()), _cmdBufs.data());
    }
}

void ViewportFramebufferVulkan::Resize(uint32_t width, uint32_t height) {
    vkQueueWaitIdle(_graphicsQueue);
    Release();
    Allocate(width, height);
}

void ViewportFramebufferVulkan::BeginRender(uint32_t frameIndex) {
    _activeFrame = frameIndex % static_cast<uint32_t>(_framesInFlight);
    VkCommandBuffer cmd = _cmdBufs[_activeFrame];

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Transition image: SHADER_READ_ONLY → COLOR_ATTACHMENT_OPTIMAL.
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = _image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ViewportFramebufferVulkan::EndRender(uint32_t /*frameIndex*/) {
    VkCommandBuffer cmd = _cmdBufs[_activeFrame];

    // Transition image: COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL.
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = _image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphicsQueue); // Phase 24 CPU stall — see file comment.
}

ViewportHandles ViewportFramebufferVulkan::GetHandles() const {
    ViewportHandles h;
    h.vkImage   = _image;
    h.vkView    = _imageView;
    h.vkCmdBuf  = _cmdBufs.empty() ? nullptr : _cmdBufs[_activeFrame];
    h.width     = _width;
    h.height    = _height;
    h.imTextureId = reinterpret_cast<uint64_t>(_descriptorSet);
    return h;
}

void ViewportFramebufferVulkan::Allocate(uint32_t width, uint32_t height) {
    _width  = width;
    _height = height;

    // Create the color image.
    VkImageCreateInfo imgCi{};
    imgCi.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCi.imageType     = VK_IMAGE_TYPE_2D;
    imgCi.format        = _format;
    imgCi.extent        = {width, height, 1};
    imgCi.mipLevels     = 1;
    imgCi.arrayLayers   = 1;
    imgCi.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgCi.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgCi.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCi{};
    allocCi.usage = VMA_MEMORY_USAGE_AUTO;
    allocCi.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    vmaCreateImage(_allocator, &imgCi, &allocCi, &_image, &_allocation, nullptr);

    // Create the image view.
    VkImageViewCreateInfo viewCi{};
    viewCi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCi.image            = _image;
    viewCi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewCi.format           = _format;
    viewCi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(_device, &viewCi, nullptr, &_imageView);

    // Transition image to SHADER_READ_ONLY_OPTIMAL so ImGui can sample it
    // from the first frame before any viewport render fires.
    VkCommandBuffer oneShotCmd = _cmdBufs[0];
    vkResetCommandBuffer(oneShotCmd, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(oneShotCmd, &bi);

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = _image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask       = 0;
    barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(oneShotCmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(oneShotCmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &oneShotCmd;
    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphicsQueue);

    // Register with ImGui for ImTextureID.
    VkSampler linearSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerCi{};
    samplerCi.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCi.magFilter    = VK_FILTER_LINEAR;
    samplerCi.minFilter    = VK_FILTER_LINEAR;
    samplerCi.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(_device, &samplerCi, nullptr, &linearSampler);

    _descriptorSet = ImGui_ImplVulkan_AddTexture(
        linearSampler, _imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // The sampler is embedded in the descriptor set — destroy our reference.
    vkDestroySampler(_device, linearSampler, nullptr);
}

void ViewportFramebufferVulkan::Release() noexcept {
    if (_device == VK_NULL_HANDLE) return;

    if (_descriptorSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(_descriptorSet);
        _descriptorSet = VK_NULL_HANDLE;
    }
    if (_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, _imageView, nullptr);
        _imageView = VK_NULL_HANDLE;
    }
    if (_image != VK_NULL_HANDLE && _allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, _image, _allocation);
        _image      = VK_NULL_HANDLE;
        _allocation = VK_NULL_HANDLE;
    }
}

} // namespace ImFrame::Internal
