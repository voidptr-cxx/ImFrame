/**
 * @file     NativeRendererVulkan.cpp
 * @brief    `NativeRendererVulkan` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-12
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "NativeRendererVulkan.hpp"

#include "ImFrame/Core/Error.hpp"

#include "Shaders/SDFRect.frag.hpp"
#include "Shaders/SDFRect.vert.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ImFrame::Internal {

NativeRendererVulkan::NativeRendererVulkan(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue,
                                          std::uint32_t graphicsQueueFamily, VkCommandPool commandPool,
                                          VkFormat colorFormat)
    : _device(device)
    , _allocator(allocator)
    , _graphicsQueue(graphicsQueue)
    , _graphicsQueueFamily(graphicsQueueFamily)
    , _commandPool(commandPool)
    , _colorFormat(colorFormat) {}

NativeRendererVulkan::~NativeRendererVulkan() { Shutdown(); }

void NativeRendererVulkan::SetTarget(VkImageView targetView, std::uint32_t width, std::uint32_t height) noexcept {
    _targetView   = targetView;
    _targetWidth  = width;
    _targetHeight = height;
}

Result<Rendering::FontId> NativeRendererVulkan::LoadFont(const Utility::Path& /*path*/, float /*sizePixels*/) {
    // No text pipeline exists yet -- Phase 35.1's scope is BatchKind::Rect only, matching
    // NativeRendererGL3's own identical "no attached text renderer" fallback (Phase 33.7).
    return std::unexpected(Error::FontLoadFailed);
}

void NativeRendererVulkan::EnsureInitialized() {
    if (_initialized) { return; }

    // ─── Shader modules, from Shaders/SDFRect.glsl's pre-compiled SPIR-V (Phase 32.2) ──────────
    VkShaderModuleCreateInfo vertModuleCi{};
    vertModuleCi.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleCi.codeSize = Shaders::kSDFRectVertexSpirvByteCount;
    vertModuleCi.pCode    = reinterpret_cast<const std::uint32_t*>(Shaders::kSDFRectVertexSpirv);
    vkCreateShaderModule(_device, &vertModuleCi, nullptr, &_rectVertexModule);

    VkShaderModuleCreateInfo fragModuleCi{};
    fragModuleCi.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleCi.codeSize = Shaders::kSDFRectFragmentSpirvByteCount;
    fragModuleCi.pCode    = reinterpret_cast<const std::uint32_t*>(Shaders::kSDFRectFragmentSpirv);
    vkCreateShaderModule(_device, &fragModuleCi, nullptr, &_rectFragmentModule);

    // ─── Descriptor set layout / pipeline layout: binding=0, PerFrame UBO, vertex stage ────────
    // (matches SDFRect.glsl's `layout(binding=0) uniform PerFrame { vec2 ViewportSize; }`).
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutCi{};
    setLayoutCi.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCi.bindingCount = 1;
    setLayoutCi.pBindings    = &uboBinding;
    vkCreateDescriptorSetLayout(_device, &setLayoutCi, nullptr, &_rectDescriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutCi{};
    pipelineLayoutCi.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCi.setLayoutCount = 1;
    pipelineLayoutCi.pSetLayouts    = &_rectDescriptorSetLayout;
    vkCreatePipelineLayout(_device, &pipelineLayoutCi, nullptr, &_rectPipelineLayout);

    // ─── This renderer's own small descriptor pool + set for the PerFrame UBO ──────────────────
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets       = 1;
    poolCi.poolSizeCount = 1;
    poolCi.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(_device, &poolCi, nullptr, &_descriptorPool);

    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool     = _descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts        = &_rectDescriptorSetLayout;
    vkAllocateDescriptorSets(_device, &setAllocInfo, &_perFrameDescriptorSet);

    // ─── PerFrame UBO: vec2 ViewportSize (8 bytes, rounded up to 16) ───────────────────────────
    VkBufferCreateInfo uboCi{};
    uboCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uboCi.size        = 16;
    uboCi.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uboCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo uboAllocCi{};
    uboAllocCi.usage = VMA_MEMORY_USAGE_AUTO;
    uboAllocCi.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VmaAllocationInfo uboAllocInfo{};
    vmaCreateBuffer(_allocator, &uboCi, &uboAllocCi, &_perFrameUbo, &_perFrameUboAllocation, &uboAllocInfo);
    _perFrameUboMapped = uboAllocInfo.pMappedData;

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = _perFrameUbo;
    bufferInfo.offset = 0;
    bufferInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = _perFrameDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo     = &bufferInfo;
    vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

    // ─── Vertex input state — matches RectVertex's field layout exactly (BatchBuilder.hpp) ─────
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(RectVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    const std::array<VkVertexInputAttributeDescription, 7> attrs{{
        {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, Position))},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, Local))},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, HalfSize))},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, Radii))},
        {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, FillColor))},
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, StrokeColor))},
        {6, 0, VK_FORMAT_R32_SFLOAT, static_cast<std::uint32_t>(offsetof(RectVertex, StrokeWidth))},
    }};

    VkPipelineVertexInputStateCreateInfo vertexInputCi{};
    vertexInputCi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCi.vertexBindingDescriptionCount   = 1;
    vertexInputCi.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputCi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrs.size());
    vertexInputCi.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCi{};
    inputAssemblyCi.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport/scissor are dynamic (VK_DYNAMIC_STATE_VIEWPORT/SCISSOR below) so this pipeline
    // isn't tied to one target size -- SetTarget() can change size without rebuilding it.
    VkPipelineViewportStateCreateInfo viewportStateCi{};
    viewportStateCi.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCi.viewportCount = 1;
    viewportStateCi.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationCi{};
    rasterizationCi.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCi.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCi.cullMode    = VK_CULL_MODE_NONE;
    rasterizationCi.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCi.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleCi{};
    multisampleCi.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Standard (non-premultiplied-alpha) "over" blend -- matches NativeRendererGL3::DrawBatches()'s
    // own default (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), for cross-backend visual consistency.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendCi{};
    colorBlendCi.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCi.attachmentCount = 1;
    colorBlendCi.pAttachments    = &colorBlendAttachment;

    const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCi{};
    dynamicStateCi.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCi.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicStateCi.pDynamicStates    = dynamicStates.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = _rectVertexModule;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = _rectFragmentModule;
    stages[1].pName  = "main";

    // VK_KHR_dynamic_rendering (core in 1.3, already enabled by SDL3VulkanBackend) -- no
    // VkRenderPass/VkFramebuffer, matching this backend's existing style (ViewportVulkan.cpp).
    VkPipelineRenderingCreateInfo renderingCi{};
    renderingCi.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCi.colorAttachmentCount    = 1;
    renderingCi.pColorAttachmentFormats = &_colorFormat;

    VkGraphicsPipelineCreateInfo pipelineCi{};
    pipelineCi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCi.pNext               = &renderingCi;
    pipelineCi.stageCount          = static_cast<std::uint32_t>(stages.size());
    pipelineCi.pStages             = stages.data();
    pipelineCi.pVertexInputState   = &vertexInputCi;
    pipelineCi.pInputAssemblyState = &inputAssemblyCi;
    pipelineCi.pViewportState      = &viewportStateCi;
    pipelineCi.pRasterizationState = &rasterizationCi;
    pipelineCi.pMultisampleState   = &multisampleCi;
    pipelineCi.pColorBlendState    = &colorBlendCi;
    pipelineCi.pDynamicState       = &dynamicStateCi;
    pipelineCi.layout              = _rectPipelineLayout;
    pipelineCi.renderPass          = VK_NULL_HANDLE; // dynamic rendering
    vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCi, nullptr, &_rectPipeline);

    VkFenceCreateInfo fenceCi{};
    fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(_device, &fenceCi, nullptr, &_submitFence);

    _initialized = true;
}

void NativeRendererVulkan::EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes) {
    if (vertexBytes > _vertexBufferCapacityBytes) {
        if (_vertexBuffer != VK_NULL_HANDLE) { vmaDestroyBuffer(_allocator, _vertexBuffer, _vertexBufferAllocation); }

        VkBufferCreateInfo bufCi{};
        bufCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCi.size        = static_cast<VkDeviceSize>(vertexBytes);
        bufCi.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        allocCi.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationInfo allocInfo{};
        vmaCreateBuffer(_allocator, &bufCi, &allocCi, &_vertexBuffer, &_vertexBufferAllocation, &allocInfo);
        _vertexBufferMapped        = allocInfo.pMappedData;
        _vertexBufferCapacityBytes = vertexBytes;
    }

    if (indexBytes > _indexBufferCapacityBytes) {
        if (_indexBuffer != VK_NULL_HANDLE) { vmaDestroyBuffer(_allocator, _indexBuffer, _indexBufferAllocation); }

        VkBufferCreateInfo bufCi{};
        bufCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCi.size        = static_cast<VkDeviceSize>(indexBytes);
        bufCi.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        allocCi.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationInfo allocInfo{};
        vmaCreateBuffer(_allocator, &bufCi, &allocCi, &_indexBuffer, &_indexBufferAllocation, &allocInfo);
        _indexBufferMapped        = allocInfo.pMappedData;
        _indexBufferCapacityBytes = indexBytes;
    }
}

void NativeRendererVulkan::RenderRectBatch(VkCommandBuffer cmd, const Batch& batch, std::size_t& vertexByteOffset,
                                          std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    const std::size_t vertexBytes = vertices.size() * sizeof(RectVertex);
    const std::size_t indexBytes  = batch.Indices.size() * sizeof(std::uint32_t);

    std::memcpy(static_cast<std::byte*>(_vertexBufferMapped) + vertexByteOffset, vertices.data(), vertexBytes);
    std::memcpy(static_cast<std::byte*>(_indexBufferMapped) + indexByteOffset, batch.Indices.data(), indexBytes);

    const VkDeviceSize vbOffset = static_cast<VkDeviceSize>(vertexByteOffset);
    vkCmdBindVertexBuffers(cmd, 0, 1, &_vertexBuffer, &vbOffset);
    vkCmdBindIndexBuffer(cmd, _indexBuffer, static_cast<VkDeviceSize>(indexByteOffset), VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(batch.Indices.size()), 1, 0, 0, 0);

    vertexByteOffset += vertexBytes;
    indexByteOffset += indexBytes;
}

void NativeRendererVulkan::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    // A missing SetTarget() call is a caller bug, not a runtime condition to recover from --
    // matches NativeRendererGL3's own equivalent "the caller is responsible for a valid GL context/
    // viewport" contract, just made explicit here since Vulkan has no implicit "currently bound".
    IMF_ASSERT(_targetView != VK_NULL_HANDLE);

    _batchBuilder.Build(buffer);
    const std::vector<Batch>& batches = _batchBuilder.Batches();

    std::size_t totalVertexBytes = 0;
    std::size_t totalIndexBytes  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind != BatchKind::Rect) { continue; } // Phase 35.1 scope: BatchKind::Rect only
        totalVertexBytes += std::get<std::vector<RectVertex>>(batch.Vertices).size() * sizeof(RectVertex);
        totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
    }
    if (totalVertexBytes == 0) { return; }

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    struct PerFrameUbo { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameUbo perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameUboMapped, &perFrame, sizeof(perFrame));

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

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView   = _targetView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // LOAD (not CLEAR) -- matches NativeRendererGL3's own "draws into whatever's already there,
    // never clears" convention; the caller (test, or later a real viewport) owns clearing.
    colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea           = {{0, 0}, {_targetWidth, _targetHeight}};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    vkCmdBeginRendering(cmd, &renderingInfo);

    const VkViewport viewport{0.0f,
                              0.0f,
                              static_cast<float>(_targetWidth),
                              static_cast<float>(_targetHeight),
                              0.0f,
                              1.0f};
    const VkRect2D scissor{{0, 0}, {_targetWidth, _targetHeight}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _rectPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _rectPipelineLayout, 0, 1, &_perFrameDescriptorSet,
                            0, nullptr);

    std::size_t vertexByteOffset = 0;
    std::size_t indexByteOffset  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) { RenderRectBatch(cmd, batch, vertexByteOffset, indexByteOffset); }
    }

    vkCmdEndRendering(cmd);
    vkEndCommandBuffer(cmd);

    vkResetFences(_device, 1, &_submitFence);
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _submitFence);

    // Block until this Render() call's GPU work completes before returning -- a deliberate,
    // documented stopgap (see this class's own .hpp comment) so the streaming vertex/index/UBO
    // buffers are always safe to overwrite again on the very next call.
    vkWaitForFences(_device, 1, &_submitFence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(_device, _commandPool, 1, &cmd);
}

void NativeRendererVulkan::Shutdown() {
    if (!_initialized) { return; }

    // Render() already waits synchronously for its own submission before returning, so nothing
    // should be in flight here -- this is cheap insurance for Shutdown() being callable at any
    // time per IRenderer's own "safe to call multiple times" contract.
    vkDeviceWaitIdle(_device);

    if (_vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(_allocator, _vertexBuffer, _vertexBufferAllocation);
        _vertexBuffer = VK_NULL_HANDLE;
        _vertexBufferAllocation = VK_NULL_HANDLE;
        _vertexBufferMapped = nullptr;
        _vertexBufferCapacityBytes = 0;
    }
    if (_indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(_allocator, _indexBuffer, _indexBufferAllocation);
        _indexBuffer = VK_NULL_HANDLE;
        _indexBufferAllocation = VK_NULL_HANDLE;
        _indexBufferMapped = nullptr;
        _indexBufferCapacityBytes = 0;
    }
    if (_perFrameUbo != VK_NULL_HANDLE) {
        vmaDestroyBuffer(_allocator, _perFrameUbo, _perFrameUboAllocation);
        _perFrameUbo = VK_NULL_HANDLE;
        _perFrameUboAllocation = VK_NULL_HANDLE;
        _perFrameUboMapped = nullptr;
    }
    if (_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
        _descriptorPool = VK_NULL_HANDLE;
        _perFrameDescriptorSet = VK_NULL_HANDLE;
    }
    if (_rectPipeline != VK_NULL_HANDLE) { vkDestroyPipeline(_device, _rectPipeline, nullptr); _rectPipeline = VK_NULL_HANDLE; }
    if (_rectPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_device, _rectPipelineLayout, nullptr);
        _rectPipelineLayout = VK_NULL_HANDLE;
    }
    if (_rectDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(_device, _rectDescriptorSetLayout, nullptr);
        _rectDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (_rectFragmentModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(_device, _rectFragmentModule, nullptr);
        _rectFragmentModule = VK_NULL_HANDLE;
    }
    if (_rectVertexModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(_device, _rectVertexModule, nullptr);
        _rectVertexModule = VK_NULL_HANDLE;
    }
    if (_submitFence != VK_NULL_HANDLE) { vkDestroyFence(_device, _submitFence, nullptr); _submitFence = VK_NULL_HANDLE; }

    _initialized = false;
}

} // namespace ImFrame::Internal
