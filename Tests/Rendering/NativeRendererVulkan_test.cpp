/**
 * @file     NativeRendererVulkan_test.cpp
 * @brief    Real-pixel tests for NativeRendererVulkan's DrawRect (Phase 35.1) and DrawImage (Phase 35.2) rendering
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
        // TRANSFER_DST is needed for TransitionAndClear()'s own vkCmdClearColorImage below --
        // omitting it is undefined behaviour per the Vulkan spec (VUID-vkCmdClearColorImage-image-00002),
        // silently caught by the validation layer but otherwise easy to miss since some drivers still
        // produce a plausible-looking result in isolation.
        imgCi.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    [[nodiscard]] VkImage Image() const noexcept { return _image; }
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

/// A small, solid-colour `VkImage` — known content to assert against after compositing.
/// `NativeRendererVulkan` treats `DrawImage::Texture`'s value as a raw `VkImageView` handle (see
/// `NativeRendererVulkan.hpp`'s own file comment), so this real image view's handle is what gets
/// pushed. Uploads via a staging buffer, then transitions to `SHADER_READ_ONLY_OPTIMAL` — the
/// layout `RenderImageBatch()`'s descriptor write expects.
class ScratchTexture {
public:
    ScratchTexture(VkDevice device, VmaAllocator allocator, VkQueue queue, VkCommandPool cmdPool, std::uint8_t r,
                  std::uint8_t g, std::uint8_t b, std::uint8_t a)
        : _device(device), _allocator(allocator), _queue(queue), _cmdPool(cmdPool) {
        constexpr int kSize = 8;
        std::vector<std::byte> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = std::byte{r};
            pixels[i + 1] = std::byte{g};
            pixels[i + 2] = std::byte{b};
            pixels[i + 3] = std::byte{a};
        }

        VkImageCreateInfo imgCi{};
        imgCi.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCi.imageType     = VK_IMAGE_TYPE_2D;
        imgCi.format        = VK_FORMAT_R8G8B8A8_UNORM;
        imgCi.extent        = {kSize, kSize, 1};
        imgCi.mipLevels     = 1;
        imgCi.arrayLayers   = 1;
        imgCi.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgCi.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgCi.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

        UploadAndTransition(pixels, kSize);
    }

    ~ScratchTexture() {
        vkDestroyImageView(_device, _view, nullptr);
        vmaDestroyImage(_allocator, _image, _allocation);
    }

    ScratchTexture(const ScratchTexture&) = delete;
    ScratchTexture& operator=(const ScratchTexture&) = delete;

    [[nodiscard]] TextureId Id() const {
        return TextureId(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(_view)));
    }

private:
    void UploadAndTransition(const std::vector<std::byte>& pixels, int size) {
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

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool        = _cmdPool;
        cmdAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

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
        region.imageExtent      = {static_cast<std::uint32_t>(size), static_cast<std::uint32_t>(size), 1};
        vkCmdCopyBufferToImage(cmd, stagingBuf, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier toShaderRead{};
        toShaderRead.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaderRead.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderRead.image               = _image;
        toShaderRead.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toShaderRead.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &toShaderRead);

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;
        vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_queue);
        vkFreeCommandBuffers(_device, _cmdPool, 1, &cmd);
        vmaDestroyBuffer(_allocator, stagingBuf, stagingAlloc);
    }

    VkDevice      _device     = VK_NULL_HANDLE;
    VmaAllocator  _allocator  = VK_NULL_HANDLE;
    VkQueue       _queue      = VK_NULL_HANDLE;
    VkCommandPool _cmdPool    = VK_NULL_HANDLE;
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
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
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
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
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

TEST_CASE("NativeRendererVulkan draws a DrawImage at the recorded position, sampling the bound "
          "VkImageView and applying TintColor (Phase 35.2)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);
        ScratchTexture texture(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, 0, 0,
                              255, 255);

        CommandBuffer buffer;
        buffer.Push(DrawImage{
            .Position  = {0.0f, 0.0f},
            .Size      = {64.0f, 64.0f},
            .Texture   = texture.Id(),
            .TintColor = {0.5f, 1.0f, 1.0f, 1.0f}});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel inside  = Sample(pixels, 32, 32, WIDTH);
        const Pixel outside = Sample(pixels, WIDTH - 4, HEIGHT - 4, WIDTH);

        REQUIRE(inside.b > 200);
        REQUIRE(inside.r < 150); // TintColor.r == 0.5 darkens the source texture's zero red further
        REQUIRE(inside.a > 200);
        REQUIRE(outside.a == 0); // untouched -- still the target's transparent clear colour

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan renders a Rect and two differently-textured Image batches "
          "correctly in the same frame (Phase 35.2)",
          "[vulkan]") {
    // Directly validates the per-batch descriptor set scheme (see NativeRendererVulkan.hpp's own
    // file comment): a single reused Image descriptor set would make every draw in this frame
    // sample whichever texture was written *last*, once all these commands actually execute on
    // the GPU (recording all of them happens before any of them run).
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);
        ScratchTexture redTexture(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, 255,
                                 0, 0, 255);
        ScratchTexture greenTexture(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, 0,
                                   255, 0, 255);

        CommandBuffer buffer;
        buffer.Push(DrawRect{.Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}, .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawImage{.Position = {48.0f, 0.0f}, .Size = {32.0f, 32.0f}, .Texture = redTexture.Id()});
        buffer.Push(DrawImage{.Position = {96.0f, 96.0f}, .Size = {32.0f, 32.0f}, .Texture = greenTexture.Id()});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel rectPixel  = Sample(pixels, 16, 16, WIDTH);
        const Pixel redPixel   = Sample(pixels, 64, 16, WIDTH);
        const Pixel greenPixel = Sample(pixels, 112, 112, WIDTH);

        REQUIRE(rectPixel.b > 200);
        REQUIRE(rectPixel.r < 50);

        REQUIRE(redPixel.r > 200);
        REQUIRE(redPixel.g < 50);
        REQUIRE(redPixel.b < 50);

        REQUIRE(greenPixel.g > 200);
        REQUIRE(greenPixel.r < 50);
        REQUIRE(greenPixel.b < 50);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan renders a DrawShadow behind and offset from the shape it shadows "
          "(Phase 35.4)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // Shadow first (drawn behind), then an opaque white rect at the same (unshifted) position
        // and size on top -- per PHASE_34_PROPOSAL.md's DrawShadow section ("composite it behind
        // the shape at the specified offset"), the same scenario NativeRendererGL3_test.cpp's own
        // Phase 34.4 test covers.
        buffer.Push(DrawShadow{
            .Position = {40.0f, 40.0f},
            .Size = {48.0f, 48.0f},
            .BlurRadius = 8.0f,
            .Offset = {10.0f, 10.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(DrawRect{
            .Position = {40.0f, 40.0f}, .Size = {48.0f, 48.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        // Deep inside the shadow's offset footprint (x,y in [50,98] before blur padding) but past
        // the white rect's own edge (rect ends at x=88, y=88) -- the shadow should be visible here,
        // not occluded.
        const Pixel shadowOnly = Sample(pixels, 94, 94, WIDTH);
        // Deep inside the rect's own footprint -- drawn after the shadow, so it occludes it.
        const Pixel rectOnTop = Sample(pixels, 60, 60, WIDTH);
        // Far from both the rect and the shadow's shifted+blurred footprint -- untouched.
        const Pixel untouched = Sample(pixels, 10, 10, WIDTH);

        REQUIRE(shadowOnly.a > 100);
        REQUIRE(shadowOnly.r < 50); // ShadowColor is opaque black
        REQUIRE(rectOnTop.r > 200);
        REQUIRE(rectOnTop.a > 200);
        REQUIRE(untouched.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan composites a PushOpacityLayer at the recorded opacity (Phase 35.5)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel inside  = Sample(pixels, 40, 40, WIDTH);
        const Pixel outside = Sample(pixels, 5, 5, WIDTH);

        // Correct premultiplied-alpha compositing: an opaque red rect at Opacity=0.5 ends up with
        // both its alpha AND its stored (premultiplied) red channel scaled to roughly half -- the
        // same scenario NativeRendererGL3_test.cpp's own Phase 34.5 test covers.
        REQUIRE(inside.a > 100);
        REQUIRE(inside.a < 150);
        REQUIRE(inside.r > 100);
        REQUIRE(inside.r < 150);
        REQUIRE(inside.g < 20);
        REQUIRE(outside.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan composites a PushBlendLayer using the Multiply formula (Phase 35.5)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // Opaque light-gray backdrop filling the whole viewport, then a Multiply layer with an
        // opaque mid-gray rect over part of it -- the same scenario
        // NativeRendererGL3_test.cpp's own Phase 34.5 test covers.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel overlap      = Sample(pixels, 40, 40, WIDTH); // inside the blended rect
        const Pixel backdropOnly = Sample(pixels, 5, 5, WIDTH);  // outside it -- backdrop untouched

        // Multiply(0.8, 0.5) = 0.4 -> ~102/255. Backdrop-only area stays 0.8 -> ~204/255.
        REQUIRE(overlap.r > 90);
        REQUIRE(overlap.r < 115);
        REQUIRE(backdropOnly.r > 190);
        REQUIRE(backdropOnly.r < 215);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan renders a DrawBackdropBlur: blends across a colour seam within its "
          "own rect, leaves everything outside untouched (Phase 35.6)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // A hard horizontal colour seam at y=64: red above, blue below. Deliberately asymmetric in
        // Y (not a uniform fill) so a Y-orientation bug (like Phase 35.2's) would show up as a
        // wrong-side blend instead of passing by coincidence -- the same scenario
        // NativeRendererGL3_test.cpp's own Phase 34.6 test covers.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawBackdropBlur{.Position = {40.0f, 44.0f}, .Size = {48.0f, 40.0f}, .BlurRadius = 10.0f});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        // Exactly at the seam, well inside the blur rect (x in [40,88], y in [44,84]) -- a real
        // blur straddling red-above/blue-below should show a roughly even mix of both.
        const Pixel atSeam = Sample(pixels, 64, 64, WIDTH);
        // Just outside the blur rect's own top edge (y=40 < 44) but inside its padded copy region
        // (padding = BlurRadius = 10, so the copy reaches up to y=34) -- proves the composite was
        // cropped to the requested Size, not left showing the padding's own blurred bleed.
        const Pixel justAboveRect = Sample(pixels, 64, 40, WIDTH);
        // Far from the blur rect and the seam entirely -- untouched original colours.
        const Pixel untouchedRed  = Sample(pixels, 10, 10, WIDTH);
        const Pixel untouchedBlue = Sample(pixels, 10, 118, WIDTH);

        REQUIRE(atSeam.r > 80);
        REQUIRE(atSeam.r < 180);
        REQUIRE(atSeam.b > 80);
        REQUIRE(atSeam.b < 180);

        REQUIRE(justAboveRect.r > 200);
        REQUIRE(justAboveRect.b < 20);

        REQUIRE(untouchedRed.r > 200);
        REQUIRE(untouchedRed.b < 20);
        REQUIRE(untouchedBlue.b > 200);
        REQUIRE(untouchedBlue.r < 20);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan rate-limits DrawBackdropBlur at MaxBackdropBlurPerFrame, degrading "
          "gracefully past the limit (Phase 35.6)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        // Five non-overlapping backdrop-blur regions straddling the same seam -- default
        // MaxBackdropBlurPerFrame is 4, so the 5th should be skipped entirely.
        for (int i = 0; i < 5; ++i) {
            buffer.Push(DrawBackdropBlur{
                .Position = {10.0f + static_cast<float>(i) * 20.0f, 54.0f}, .Size = {16.0f, 20.0f}, .BlurRadius = 8.0f});
        }

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        // y=59 is 5px above the seam (y=64), inside every region's own Y range [54,74] but nowhere
        // near the geometric seam itself -- a processed (blurred) region bleeds some blue this far
        // into the red band; a skipped region leaves this pixel exactly the original pure red.
        for (int i = 0; i < 4; ++i) {
            const int x = 10 + i * 20 + 8; // center-x of region i
            const Pixel processed = Sample(pixels, x, 59, WIDTH);
            REQUIRE(processed.b > 15);
        }
        const Pixel skipped = Sample(pixels, 10 + 4 * 20 + 8, 59, WIDTH);
        REQUIRE(skipped.b == 0);
        REQUIRE(skipped.r == 255);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan keeps a Shadow composite and two sibling opacity layers independent "
          "within one Render() call (Phase 35.19)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // Three internal composite draws in one Render() call. If any two share a quad buffer or
        // descriptor that is rewritten before the command buffer executes, the earlier one reads the
        // later one's data: layer 1 takes layer 2's opacity, or the shadow composite samples a layer
        // target instead of its own blurred silhouette. Ported from NativeRendererWebGPU_test.cpp's
        // own Phase 35.17 regression test.
        buffer.Push(DrawShadow{
            .Position = {70.0f, 70.0f},
            .Size = {30.0f, 30.0f},
            .BlurRadius = 4.0f,
            .Offset = {6.0f, 6.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});
        buffer.Push(PushOpacityLayer{.Opacity = 1.0f});
        buffer.Push(DrawRect{
            .Position = {50.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel halfRed   = Sample(pixels, 25, 25, WIDTH);
        const Pixel fullBlue  = Sample(pixels, 65, 25, WIDTH);
        const Pixel shadow    = Sample(pixels, 91, 91, WIDTH);
        const Pixel untouched = Sample(pixels, 5, 120, WIDTH);

        REQUIRE(halfRed.a > 100); // layer 1 kept its OWN opacity, not layer 2's 1.0
        REQUIRE(halfRed.a < 150);
        REQUIRE(halfRed.r > 100);
        REQUIRE(halfRed.r < 150);
        REQUIRE(fullBlue.b > 200);
        REQUIRE(fullBlue.a > 200);
        REQUIRE(shadow.a > 100); // the shadow's own composite survived, in its own place
        REQUIRE(shadow.r < 50);
        REQUIRE(untouched.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan composites two sibling PushBlendLayers independently within one "
          "Render() call (Phase 35.19)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // Opaque light-gray backdrop, then TWO sibling Multiply layers (mid-gray rects, left and
        // right) in one Render() call. Each blend composite gets its own descriptors/slots, and the
        // shared backdrop copy must not be overwritten by the second layer before the first
        // composite has read it (Phase 35.19).
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {50.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel left         = Sample(pixels, 25, 25, WIDTH);
        const Pixel right        = Sample(pixels, 65, 25, WIDTH);
        const Pixel backdropOnly = Sample(pixels, 5, 90, WIDTH);

        // Multiply(0.8, 0.5) = 0.4 -> ~102/255 inside each rect; backdrop-only stays ~204/255.
        REQUIRE(left.r > 90);
        REQUIRE(left.r < 115);
        REQUIRE(right.r > 90);
        REQUIRE(right.r < 115);
        REQUIRE(backdropOnly.r > 190);
        REQUIRE(backdropOnly.r < 215);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererVulkan composites a PushBlendLayer nested inside an opacity layer against the "
          "enclosing layer's own content (Phase 35.19)",
          "[vulkan]") {
    SDL3VulkanBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchImage image(handles.Device, handles.Allocator, handles.GraphicsQueue, handles.CommandPool, WIDTH,
                          HEIGHT);

        CommandBuffer buffer;
        // A Multiply layer nested inside an opacity layer: its backdrop copy reads the ENCLOSING
        // layer's own target (light gray, drawn inside the outer layer), not the real target, which
        // stays transparent (Phase 35.19).
        buffer.Push(PushOpacityLayer{.Opacity = 1.0f});
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});
        buffer.Push(PopLayer{});

        NativeRendererVulkan renderer(handles.Device, handles.Allocator, handles.GraphicsQueue,
                                      handles.GraphicsQueueFamily, handles.CommandPool, kColorFormat);
        renderer.SetTarget(image.Image(), image.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto pixels = image.ReadPixels();
        const Pixel inside  = Sample(pixels, 25, 25, WIDTH);
        const Pixel outside = Sample(pixels, 5, 90, WIDTH);

        REQUIRE(inside.r > 90); // Multiply(0.8, 0.5) = 0.4 -> ~102/255
        REQUIRE(inside.r < 115);
        REQUIRE(inside.a > 240);
        REQUIRE(outside.r > 190); // the enclosing layer's own 0.8 gray, composited at opacity 1.0
        REQUIRE(outside.r < 215);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
