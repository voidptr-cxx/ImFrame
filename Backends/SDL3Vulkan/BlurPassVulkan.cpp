/**
 * @file     BlurPassVulkan.cpp
 * @brief    `BlurPassVulkan` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "BlurPassVulkan.hpp"

#include "Shaders/GaussianBlur.comp.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace ImFrame::Internal {

namespace {

/// The push-constant block `GaussianBlur.glsl`'s `PushConstants` expects: `vec2 Direction` (8
/// bytes) + `float Radius` (4 bytes), tightly packed — matches std430 push-constant layout with
/// no padding needed since every member is naturally 4-byte-aligned.
struct BlurPushConstants {
    float DirectionX;
    float DirectionY;
    float Radius;
};

constexpr std::uint32_t kWorkgroupSize = 16;

[[nodiscard]] std::uint32_t DivRoundUp(std::uint32_t value, std::uint32_t divisor) {
    return (value + divisor - 1) / divisor;
}

/// Allocates (or reallocates) one `VK_FORMAT_R8G8B8A8_UNORM` storage-image + view pair.
void CreateStorageImage(VkDevice device, VmaAllocator allocator, VkImage& image, VmaAllocation& allocation,
                        VkImageView& view, std::uint32_t width, std::uint32_t height) {
    if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
    if (image != VK_NULL_HANDLE) { vmaDestroyImage(allocator, image, allocation); image = VK_NULL_HANDLE; }

    VkImageCreateInfo imageCi{};
    imageCi.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCi.imageType     = VK_IMAGE_TYPE_2D;
    imageCi.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageCi.extent        = {width, height, 1};
    imageCi.mipLevels     = 1;
    imageCi.arrayLayers   = 1;
    imageCi.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCi.tiling        = VK_IMAGE_TILING_OPTIMAL;
    // TRANSFER_SRC lets a caller (or a test) read back / copy the result -- Apply() itself never
    // needs it, but a STORAGE_BIT-only image can't be the source of any copy at all.
    imageCi.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCi{};
    allocCi.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateImage(allocator, &imageCi, &allocCi, &image, &allocation, nullptr);

    VkImageViewCreateInfo viewCi{};
    viewCi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCi.image            = image;
    viewCi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewCi.format           = VK_FORMAT_R8G8B8A8_UNORM;
    viewCi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device, &viewCi, nullptr, &view);
}

} // namespace

BlurPassVulkan::BlurPassVulkan(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue,
                               VkCommandPool commandPool)
    : _device(device), _allocator(allocator), _queue(graphicsQueue), _commandPool(commandPool) {}

BlurPassVulkan::~BlurPassVulkan() { Shutdown(); }

void BlurPassVulkan::Shutdown() {
    if (!_initialized) { return; }

    vkQueueWaitIdle(_queue);

    if (_viewA != VK_NULL_HANDLE) { vkDestroyImageView(_device, _viewA, nullptr); _viewA = VK_NULL_HANDLE; }
    if (_imageA != VK_NULL_HANDLE) { vmaDestroyImage(_allocator, _imageA, _imageAAllocation); _imageA = VK_NULL_HANDLE; }
    if (_viewB != VK_NULL_HANDLE) { vkDestroyImageView(_device, _viewB, nullptr); _viewB = VK_NULL_HANDLE; }
    if (_imageB != VK_NULL_HANDLE) { vmaDestroyImage(_allocator, _imageB, _imageBAllocation); _imageB = VK_NULL_HANDLE; }
    _targetWidth  = 0;
    _targetHeight = 0;
    _targetsNeedInitialTransition = false;

    if (_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
        _descriptorPool  = VK_NULL_HANDLE;
        _descriptorSetA  = VK_NULL_HANDLE;
        _descriptorSetB  = VK_NULL_HANDLE;
    }
    if (_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(_device, _pipeline, nullptr); _pipeline = VK_NULL_HANDLE; }
    if (_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }
    if (_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
        _descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (_computeModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(_device, _computeModule, nullptr);
        _computeModule = VK_NULL_HANDLE;
    }
    if (_submitFence != VK_NULL_HANDLE) { vkDestroyFence(_device, _submitFence, nullptr); _submitFence = VK_NULL_HANDLE; }

    _initialized = false;
}

void BlurPassVulkan::EnsureInitialized() {
    if (_initialized) { return; }

    VkShaderModuleCreateInfo moduleCi{};
    moduleCi.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCi.codeSize = Shaders::kGaussianBlurComputeSpirvByteCount;
    moduleCi.pCode    = reinterpret_cast<const std::uint32_t*>(Shaders::kGaussianBlurComputeSpirv);
    vkCreateShaderModule(_device, &moduleCi, nullptr, &_computeModule);

    // GaussianBlur.glsl: binding=0 readonly storage image (source), binding=1 writeonly storage
    // image (destination), both compute-stage only.
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutCi{};
    setLayoutCi.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCi.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setLayoutCi.pBindings    = bindings.data();
    vkCreateDescriptorSetLayout(_device, &setLayoutCi, nullptr, &_descriptorSetLayout);

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(BlurPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutCi{};
    pipelineLayoutCi.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCi.setLayoutCount         = 1;
    pipelineLayoutCi.pSetLayouts            = &_descriptorSetLayout;
    pipelineLayoutCi.pushConstantRangeCount = 1;
    pipelineLayoutCi.pPushConstantRanges    = &pushRange;
    vkCreatePipelineLayout(_device, &pipelineLayoutCi, nullptr, &_pipelineLayout);

    VkPipelineShaderStageCreateInfo stageCi{};
    stageCi.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCi.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCi.module = _computeModule;
    stageCi.pName  = "main";

    VkComputePipelineCreateInfo pipelineCi{};
    pipelineCi.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCi.stage  = stageCi;
    pipelineCi.layout = _pipelineLayout;
    vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipelineCi, nullptr, &_pipeline);

    // Two sets, two STORAGE_IMAGE descriptors each -- see this class's own .hpp comment on why
    // only _descriptorSetA's binding 0 is ever rewritten after this point.
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets       = 2;
    poolCi.poolSizeCount = 1;
    poolCi.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(_device, &poolCi, nullptr, &_descriptorPool);

    const std::array<VkDescriptorSetLayout, 2> layouts{_descriptorSetLayout, _descriptorSetLayout};
    std::array<VkDescriptorSet, 2> sets{};
    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool     = _descriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    setAllocInfo.pSetLayouts        = layouts.data();
    vkAllocateDescriptorSets(_device, &setAllocInfo, sets.data());
    _descriptorSetA = sets[0];
    _descriptorSetB = sets[1];

    VkFenceCreateInfo fenceCi{};
    fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(_device, &fenceCi, nullptr, &_submitFence);

    _initialized = true;
}

void BlurPassVulkan::EnsureTargets(std::uint32_t width, std::uint32_t height) {
    if (_targetWidth == width && _targetHeight == height && _imageA != VK_NULL_HANDLE) { return; }

    CreateStorageImage(_device, _allocator, _imageA, _imageAAllocation, _viewA, width, height);
    CreateStorageImage(_device, _allocator, _imageB, _imageBAllocation, _viewB, width, height);
    _targetWidth  = width;
    _targetHeight = height;
    _targetsNeedInitialTransition = true;

    // _descriptorSetB never changes again at this size (_viewA -> _viewB is fixed); _descriptorSetA's
    // binding 1 (this pass's own _viewA output) is likewise fixed -- only its binding 0 (the
    // caller's source) is rewritten per Apply() call, below.
    VkDescriptorImageInfo viewAInfo{};
    viewAInfo.imageView   = _viewA;
    viewAInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo viewBInfo{};
    viewBInfo.imageView   = _viewB;
    viewBInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array<VkWriteDescriptorSet, 3> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = _descriptorSetA;
    writes[0].dstBinding      = 1;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo      = &viewAInfo;
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = _descriptorSetB;
    writes[1].dstBinding      = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo      = &viewAInfo;
    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = _descriptorSetB;
    writes[2].dstBinding      = 1;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo      = &viewBInfo;
    vkUpdateDescriptorSets(_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void BlurPassVulkan::RecordPass(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, std::uint32_t width,
                                std::uint32_t height, float directionX, float directionY, float radius) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    const BlurPushConstants pc{directionX, directionY, radius};
    vkCmdPushConstants(cmd, _pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    vkCmdDispatch(cmd, DivRoundUp(width, kWorkgroupSize), DivRoundUp(height, kWorkgroupSize), 1);
}

BlurResult BlurPassVulkan::Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius) {
    if (radius <= 0.0f) { return source; }
    const float clampedRadius = std::min(radius, 64.0f);

    EnsureInitialized();
    EnsureTargets(width, height);

    // _descriptorSetA's binding 0 is the only piece of descriptor state that changes per call --
    // this frame's own source view.
    VkDescriptorImageInfo sourceInfo{};
    sourceInfo.imageView   = source.View;
    sourceInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet sourceWrite{};
    sourceWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sourceWrite.dstSet          = _descriptorSetA;
    sourceWrite.dstBinding      = 0;
    sourceWrite.descriptorCount = 1;
    sourceWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sourceWrite.pImageInfo      = &sourceInfo;
    vkUpdateDescriptorSets(_device, 1, &sourceWrite, 0, nullptr);

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool        = _commandPool;
    cmdAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    if (_targetsNeedInitialTransition) {
        std::array<VkImageMemoryBarrier, 2> initialBarriers{};
        for (std::size_t i = 0; i < initialBarriers.size(); ++i) {
            VkImageMemoryBarrier& b = initialBarriers[i];
            b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image               = (i == 0) ? _imageA : _imageB;
            b.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            b.srcAccessMask       = 0;
            b.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                            nullptr, 0, nullptr, static_cast<std::uint32_t>(initialBarriers.size()),
                            initialBarriers.data());
        _targetsNeedInitialTransition = false;
    }

    RecordPass(cmd, _descriptorSetA, width, height, 1.0f, 0.0f, clampedRadius); // horizontal -> _imageA

    // Pass 2 depends on pass 1's writes to _imageA -- vkCmdDispatch calls on the same queue give
    // no implicit memory-visibility ordering between them, unlike a render pass's own subpass
    // dependencies, so this barrier is not optional.
    VkImageMemoryBarrier betweenPasses{};
    betweenPasses.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    betweenPasses.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    betweenPasses.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
    betweenPasses.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    betweenPasses.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    betweenPasses.image               = _imageA;
    betweenPasses.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    betweenPasses.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    betweenPasses.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                        nullptr, 0, nullptr, 1, &betweenPasses);

    RecordPass(cmd, _descriptorSetB, width, height, 0.0f, 1.0f, clampedRadius); // vertical -> _imageB

    vkEndCommandBuffer(cmd);

    vkResetFences(_device, 1, &_submitFence);
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(_queue, 1, &submitInfo, _submitFence);

    // Block until this Apply() call's GPU work completes before returning -- the same deliberate
    // stopgap NativeRendererVulkan::Render() uses (see this class's own .hpp comment).
    vkWaitForFences(_device, 1, &_submitFence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(_device, _commandPool, 1, &cmd);

    return BlurResult{_imageB, _viewB};
}

} // namespace ImFrame::Internal
