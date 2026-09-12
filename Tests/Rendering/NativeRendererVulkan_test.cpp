/**
 * @file     NativeRendererVulkan_test.cpp
 * @brief    Real-pixel tests for NativeRendererVulkan's DrawRect rendering (Phase 35.1)
 *
 * @internal
 * Mirrors `NativeRendererGL3_test.cpp`'s own pattern (a hand-created offscreen render target,
 * no `Application`/`Viewport` machinery) but for Vulkan: a self-contained `ScratchImage` allocates
 * a `COLOR_ATTACHMENT | TRANSFER_SRC` `VkImage`, transitions and clears it up front (Vulkan has no
 * implicit "currently bound" target the way `NativeRendererGL3` leans on `GL_VIEWPORT`/whatever
 * framebuffer is bound — see `NativeRendererVulkan::SetTarget()`'s own doc comment), then reads it
 * back via a one-shot command buffer copying into a mapped staging buffer.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-12
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3Vulkan/NativeRendererVulkan.hpp"
#include "Backends/SDL3Vulkan/SDL3VulkanBackend.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Rendering;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;
// Matches SDL3VulkanBackend::CreateViewportFramebuffer()'s own offscreen-render-target
// convention -- not the swap chain's own (possibly different) presentation format.
constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "NativeRendererVulkan_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {static_cast<int>(pixels[off + 0]), static_cast<int>(pixels[off + 1]), static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3])};
}

/// A small, self-contained offscreen VkImage — see this file's own header comment for why a
/// hand-rolled target is used instead of the backend's real swap chain (matches
/// `NativeRendererGL3_test.cpp`'s identical reasoning for `ScratchFramebuffer`).
class ScratchImage {
public:
    ScratchImage(VkDevice device, VmaAllocator allocator, VkQueue queue, VkCommandPool cmdPool, int width,
                int height)
        : _device(device), _allocator(allocator), _queue(queue), _cmdPool(cmdPool), _width(width), _height(height) {
        VkImageCreateInfo imgCi{};
        imgCi.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCi.imageType     = VK_IMAGE_TYPE_2D;
        imgCi.format        = kColorFormat;
        imgCi.extent        = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        imgCi.mipLevels     = 1;
        imgCi.arrayLayers   = 1;
        imgCi.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgCi.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgCi.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        vmaCreateImage(_allocator, &imgCi, &allocCi, &_image, &_allocation, nullptr);

        VkImageViewCreateInfo viewCi{};
        viewCi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCi.image            = _image;
        viewCi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewCi.format           = kColorFormat;
        viewCi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(_device, &viewCi, nullptr, &_view);

        TransitionAndClear();
    }

    ~ScratchImage() {
        vkDestroyImageView(_device, _view, nullptr);
        vmaDestroyImage(_allocator, _image, _allocation);
    }

    ScratchImage(const ScratchImage&) = delete;
    ScratchImage& operator=(const ScratchImage&) = delete;

    [[nodiscard]] VkImageView View() const noexcept { return _view; }

    [[nodiscard]] std::vector<std::byte> ReadPixels() const {
        VkBufferCreateInfo bufCi{};
        bufCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCi.size        = static_cast<VkDeviceSize>(_width) * _height * 4;
        bufCi.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        allocCi.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

        VkBuffer          stagingBuf   = VK_NULL_HANDLE;
        VmaAllocation     stagingAlloc = VK_NULL_HANDLE;
        VmaAllocationInfo stagingInfo{};
        vmaCreateBuffer(_allocator, &bufCi, &allocCi, &stagingBuf, &stagingAlloc, &stagingInfo);

        VkCommandBuffer cmd = OneShotBegin();

        VkImageMemoryBarrier toTransferSrc{};
        toTransferSrc.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransferSrc.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toTransferSrc.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransferSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferSrc.image               = _image;
        toTransferSrc.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toTransferSrc.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toTransferSrc.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                            nullptr, 0, nullptr, 1, &toTransferSrc);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {static_cast<std::uint32_t>(_width), static_cast<std::uint32_t>(_height), 1};
        vkCmdCopyImageToBuffer(cmd, _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &region);

        // Back to COLOR_ATTACHMENT_OPTIMAL -- keeps the image in the layout SetTarget()'s own
        // contract expects, in case a test issues more than one Render() call against it.
        VkImageMemoryBarrier backToColor{};
        backToColor.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        backToColor.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        backToColor.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        backToColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToColor.image               = _image;
        backToColor.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        backToColor.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        backToColor.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                            nullptr, 0, nullptr, 1, &backToColor);

        OneShotEndAndWait(cmd);

        std::vector<std::byte> result(static_cast<std::size_t>(_width) * _height * 4);
        std::memcpy(result.data(), stagingInfo.pMappedData, result.size());
        vmaDestroyBuffer(_allocator, stagingBuf, stagingAlloc);
        return result;
    }

private:
    [[nodiscard]] VkCommandBuffer OneShotBegin() const {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = _cmdPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(_device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void OneShotEndAndWait(VkCommandBuffer cmd) const {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;
        vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_queue);
        vkFreeCommandBuffers(_device, _cmdPool, 1, &cmd);
    }

    /// UNDEFINED -> TRANSFER_DST_OPTIMAL -> clear to transparent black -> COLOR_ATTACHMENT_OPTIMAL
    /// (the layout `NativeRendererVulkan::SetTarget()` expects). Clearing here (not relying on
    /// `Render()`, which uses `VK_ATTACHMENT_LOAD_OP_LOAD`, matching `NativeRendererGL3`'s own
    /// "draws into whatever's already there, never clears" convention) mirrors
    /// `ScratchFramebuffer::BindAndClear()`'s identical role in the GL3 test file.
    void TransitionAndClear() {
        VkCommandBuffer cmd = OneShotBegin();

        VkImageMemoryBarrier toTransferDst{};
        toTransferDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransferDst.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransferDst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferDst.image               = _image;
        toTransferDst.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toTransferDst.srcAccessMask       = 0;
        toTransferDst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                            nullptr, 1, &toTransferDst);

        VkClearColorValue clearColor{};
        clearColor.float32[0] = clearColor.float32[1] = clearColor.float32[2] = clearColor.float32[3] = 0.0f;
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

        VkImageMemoryBarrier toColorAttachment{};
        toColorAttachment.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColorAttachment.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toColorAttachment.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.image               = _image;
        toColorAttachment.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toColorAttachment.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toColorAttachment.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                            nullptr, 0, nullptr, 1, &toColorAttachment);

        OneShotEndAndWait(cmd);
    }

    VkDevice      _device     = VK_NULL_HANDLE;
    VmaAllocator  _allocator  = VK_NULL_HANDLE;
    VkQueue       _queue      = VK_NULL_HANDLE;
    VkCommandPool _cmdPool    = VK_NULL_HANDLE;
    int           _width;
    int           _height;
    VkImage       _image      = VK_NULL_HANDLE;
    VkImageView   _view       = VK_NULL_HANDLE;
    VmaAllocation _allocation = VK_NULL_HANDLE;
};

} // namespace

TEST_CASE("NativeRendererVulkan draws a filled DrawRect at the recorded position", "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {static_cast<float>(WIDTH) / 2.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel left  = Sample(pixels, WIDTH / 4, HEIGHT / 2, WIDTH);
        const Pixel right = Sample(pixels, WIDTH * 3 / 4, HEIGHT / 2, WIDTH);

        REQUIRE(left.r > left.b);
        REQUIRE(left.r > 200);
        REQUIRE(left.a > 200);

        REQUIRE(right.b > right.r);
        REQUIRE(right.b > 200);
        REQUIRE(right.a > 200);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan renders rounded corners: the extreme corner pixel stays outside the fill",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .Radii    = CornerRadii::All(24.0f),
            .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel corner     = Sample(pixels, 1, 1, WIDTH);
        const Pixel middleEdge = Sample(pixels, WIDTH / 2, 1, WIDTH);

        REQUIRE(corner.a < 100);
        REQUIRE(middleEdge.a > 200);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
