/**
 * @file     BlurPassVulkan_test.cpp
 * @brief    Real-pixel tests for `BlurPassVulkan`'s compute-shader Gaussian blur (Phase 35.3)
 *
 * @internal
 * Mirrors `BlurPassGL3_test.cpp`'s own test scenarios (no-op below radius 0, an impulse pixel
 * spreading into its neighbours, ping-pong target reuse at a stable size) against the Vulkan
 * compute-shader implementation instead. `ScratchStorageImage` plays the combined role
 * `NativeRendererVulkan_test.cpp`'s `ScratchTexture` (upload) and `ScratchImage` (readback) split
 * into two classes — `BlurPassVulkan::Apply()`'s source and result are the same kind of resource
 * (a `VK_FORMAT_R8G8B8A8_UNORM` storage image), so one class here serves both roles.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3Vulkan/BlurPassVulkan.hpp"
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

namespace {

constexpr int WIDTH  = 32;
constexpr int HEIGHT = 32;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "BlurPassVulkan_test";
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

/// A `VK_FORMAT_R8G8B8A8_UNORM` storage image usable both as `BlurPassVulkan::Apply()`'s source
/// (already in `VK_IMAGE_LAYOUT_GENERAL`, per its own documented precondition) and as a way to
/// read back any `BlurResult` (including `Apply()`'s own return value, which shares this same
/// format/layout contract) — see this file's own header comment.
class ScratchStorageImage {
public:
    ScratchStorageImage(VkDevice device, VmaAllocator allocator, VkQueue queue, VkCommandPool cmdPool, int width,
                        int height)
        : _device(device), _allocator(allocator), _queue(queue), _cmdPool(cmdPool), _width(width), _height(height) {
        VkImageCreateInfo imgCi{};
        imgCi.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCi.imageType = VK_IMAGE_TYPE_2D;
        imgCi.format    = VK_FORMAT_R8G8B8A8_UNORM;
        imgCi.extent    = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        imgCi.mipLevels   = 1;
        imgCi.arrayLayers = 1;
        imgCi.samples     = VK_SAMPLE_COUNT_1_BIT;
        imgCi.tiling      = VK_IMAGE_TILING_OPTIMAL;
        imgCi.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        vmaCreateImage(_allocator, &imgCi, &allocCi, &_image, &_allocation, nullptr);

        VkImageViewCreateInfo viewCi{};
        viewCi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCi.image            = _image;
        viewCi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewCi.format           = VK_FORMAT_R8G8B8A8_UNORM;
        viewCi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(_device, &viewCi, nullptr, &_view);
    }

    ~ScratchStorageImage() {
        vkDestroyImageView(_device, _view, nullptr);
        vmaDestroyImage(_allocator, _image, _allocation);
    }

    ScratchStorageImage(const ScratchStorageImage&) = delete;
    ScratchStorageImage& operator=(const ScratchStorageImage&) = delete;

    [[nodiscard]] BlurResult Id() const { return {_image, _view}; }

    /// Transparent black everywhere except one fully-opaque white pixel at the exact center --
    /// known content to assert the blur spreads it. Leaves the image in `VK_IMAGE_LAYOUT_GENERAL`,
    /// the layout `BlurPassVulkan::Apply()` requires of its source.
    void UploadImpulse() {
        std::vector<std::byte> pixels(static_cast<std::size_t>(_width) * _height * 4, std::byte{0});
        const std::size_t centerOff = (static_cast<std::size_t>(_height / 2) * _width + _width / 2) * 4;
        pixels[centerOff + 0] = std::byte{255};
        pixels[centerOff + 1] = std::byte{255};
        pixels[centerOff + 2] = std::byte{255};
        pixels[centerOff + 3] = std::byte{255};
        Upload(pixels);
    }

    void Upload(const std::vector<std::byte>& pixels) {
        VkBufferCreateInfo bufCi{};
        bufCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCi.size        = static_cast<VkDeviceSize>(pixels.size());
        bufCi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        allocCi.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBuffer          stagingBuf   = VK_NULL_HANDLE;
        VmaAllocation     stagingAlloc = VK_NULL_HANDLE;
        VmaAllocationInfo stagingInfo{};
        vmaCreateBuffer(_allocator, &bufCi, &allocCi, &stagingBuf, &stagingAlloc, &stagingInfo);
        std::memcpy(stagingInfo.pMappedData, pixels.data(), pixels.size());

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

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {static_cast<std::uint32_t>(_width), static_cast<std::uint32_t>(_height), 1};
        vkCmdCopyBufferToImage(cmd, stagingBuf, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // GENERAL -- the layout BlurPassVulkan::Apply() requires of its source (see that class's
        // own .hpp comment).
        VkImageMemoryBarrier toGeneral{};
        toGeneral.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toGeneral.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.image               = _image;
        toGeneral.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toGeneral.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &toGeneral);

        OneShotEndAndWait(cmd);
        vmaDestroyBuffer(_allocator, stagingBuf, stagingAlloc);
    }

    /// Reads back an arbitrary `BlurResult` (not necessarily this instance's own image) --
    /// `BlurPassVulkan::Apply()`'s return value shares the identical format/layout contract this
    /// class's own image does. Leaves `result`'s image back in `VK_IMAGE_LAYOUT_GENERAL` in case
    /// it is `Apply()`-ed again (it is `BlurPassVulkan`-owned, so this matters if the same
    /// `BlurPassVulkan` is reused after a read-back).
    [[nodiscard]] std::vector<std::byte> ReadPixels(BlurResult result) const {
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
        toTransferSrc.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        toTransferSrc.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransferSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferSrc.image               = result.Image;
        toTransferSrc.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toTransferSrc.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        toTransferSrc.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &toTransferSrc);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {static_cast<std::uint32_t>(_width), static_cast<std::uint32_t>(_height), 1};
        vkCmdCopyImageToBuffer(cmd, result.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &region);

        VkImageMemoryBarrier backToGeneral{};
        backToGeneral.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        backToGeneral.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        backToGeneral.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        backToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        backToGeneral.image               = result.Image;
        backToGeneral.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        backToGeneral.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        backToGeneral.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &backToGeneral);

        OneShotEndAndWait(cmd);

        std::vector<std::byte> pixels(static_cast<std::size_t>(_width) * _height * 4);
        std::memcpy(pixels.data(), stagingInfo.pMappedData, pixels.size());
        vmaDestroyBuffer(_allocator, stagingBuf, stagingAlloc);
        return pixels;
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

TEST_CASE("BlurPassVulkan with radius <= 0 returns the source unchanged", "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageImage source(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool,
                                   WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassVulkan blur(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool);

        const BlurResult noOp1 = blur.Apply(source.Id(), WIDTH, HEIGHT, 0.0f);
        const BlurResult noOp2 = blur.Apply(source.Id(), WIDTH, HEIGHT, -5.0f);
        REQUIRE(noOp1.Image == source.Id().Image);
        REQUIRE(noOp1.View == source.Id().View);
        REQUIRE(noOp2.Image == source.Id().Image);
        REQUIRE(noOp2.View == source.Id().View);

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassVulkan spreads a single opaque pixel into its neighbours", "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageImage source(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool,
                                   WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassVulkan blur(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool);

        const BlurResult blurred = blur.Apply(source.Id(), WIDTH, HEIGHT, 6.0f);
        REQUIRE(blurred.Image != source.Id().Image);

        auto pixels = source.ReadPixels(blurred);
        const Pixel center    = Sample(pixels, WIDTH / 2, HEIGHT / 2, WIDTH);
        const Pixel neighbour = Sample(pixels, WIDTH / 2 + 3, HEIGHT / 2, WIDTH);
        const Pixel farAway   = Sample(pixels, 2, 2, WIDTH);

        // The center pixel's own energy spread out, so it's dimmer than the original impulse --
        // but a real Gaussian kernel still leaves it the single brightest point in the result.
        REQUIRE(center.a > 0);
        REQUIRE(center.a < 255);
        REQUIRE(center.a > neighbour.a);

        // A few pixels away, some of the impulse's energy has spread there -- it's no longer
        // exactly zero the way it was in the unblurred source.
        REQUIRE(neighbour.a > 0);

        // Far from the impulse (well outside a radius-6 kernel's reach), nothing spread there.
        REQUIRE(farAway.a == 0);

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassVulkan reuses ping-pong targets across repeated calls at the same size", "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageImage source(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool,
                                   WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassVulkan blur(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool);

        const BlurResult first  = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        const BlurResult second = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        REQUIRE(first.Image == second.Image); // same target image reused, not reallocated
        REQUIRE(first.View == second.View);

        blur.Shutdown();
    }

    backend.Shutdown();
}
