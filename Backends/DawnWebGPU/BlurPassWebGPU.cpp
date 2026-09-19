/**
 * @file     BlurPassWebGPU.cpp
 * @brief    `BlurPassWebGPU` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-19
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "BlurPassWebGPU.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace ImFrame::Internal {

namespace {

constexpr WGPUTextureFormat kFormat        = WGPUTextureFormat_RGBA8Unorm;
constexpr std::uint32_t     kWorkgroupSize = 16;

/// Inline WGSL compute shader -- see this class's own file comment on why `uSource`/`uDest` use
/// WGSL's separate read/write storage-texture access modes (not a single read_write binding, and
/// not a sampled-texture + linear-sampler pair), and why `Direction`/`Radius` come from a small
/// uniform buffer rather than any push-constant equivalent (WebGPU has none). Ports
/// `GaussianBlur.hlsl`'s/`.glsl`'s identical weighted-kernel math line for line.
constexpr const char* kGaussianBlurWgsl = R"WGSL(
struct PushConstants {
    direction: vec2f,
    radius: f32,
};

@group(0) @binding(0) var uSource: texture_storage_2d<rgba8unorm, read>;
@group(0) @binding(1) var uDest: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(2) var<uniform> uPushConstants: PushConstants;

@compute @workgroup_size(16, 16, 1)
fn CSMain(@builtin(global_invocation_id) globalId: vec3u) {
    let size = vec2i(textureDimensions(uSource));
    let coord = vec2i(globalId.xy);
    if (coord.x >= size.x || coord.y >= size.y) {
        return;
    }

    let radius = i32(uPushConstants.radius);
    let sigma = max(uPushConstants.radius * 0.5, 1e-4);
    let twoSigmaSq = 2.0 * sigma * sigma;

    var sum = vec4f(0.0, 0.0, 0.0, 0.0);
    var weightSum = 0.0;
    for (var i = -radius; i <= radius; i = i + 1) {
        let weight = exp(-f32(i * i) / twoSigmaSq);
        let offset = vec2i(uPushConstants.direction * f32(i));
        let sampleCoord = clamp(coord + offset, vec2i(0, 0), size - vec2i(1, 1));
        sum = sum + textureLoad(uSource, sampleCoord) * weight;
        weightSum = weightSum + weight;
    }
    textureStore(uDest, coord, sum / weightSum);
}
)WGSL";

WGPUStringView ToStringView(const char* str) { return WGPUStringView{str, WGPU_STRLEN}; }

WGPUTexture CreateStorageTexture(WGPUDevice device, std::uint32_t width, std::uint32_t height) {
    WGPUTextureDescriptor desc{};
    // CopySrc -- so a caller can read back Apply()'s own returned result (_textureB) via
    // wgpuCommandEncoderCopyTextureToBuffer()/CopyTextureToTexture(), matching BlurResult's own
    // file comment on why Texture (not just View) is exposed at all.
    desc.usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.dimension     = WGPUTextureDimension_2D;
    desc.size          = WGPUExtent3D{width, height, 1};
    desc.format        = kFormat;
    desc.mipLevelCount = 1;
    desc.sampleCount   = 1;
    return wgpuDeviceCreateTexture(device, &desc);
}

WGPUTextureView CreateView(WGPUTexture texture) {
    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = kFormat;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    return wgpuTextureCreateView(texture, &viewDesc);
}

struct PushConstants {
    float DirectionX;
    float DirectionY;
    float Radius;
    float Padding = 0.0f; // rounds the struct up to WGSL's own 16-byte alignment for vec2f+f32
};

} // namespace

BlurPassWebGPU::BlurPassWebGPU(WGPUDevice device, WGPUQueue queue) : _device(device), _queue(queue) {}

BlurPassWebGPU::~BlurPassWebGPU() { Shutdown(); }

void BlurPassWebGPU::EnsureInitialized() {
    if (_initialized) { return; }

    WGPUShaderSourceWGSL wgslSource{};
    wgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslSource.code        = ToStringView(kGaussianBlurWgsl);

    WGPUShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = &wgslSource.chain;
    shaderDesc.label       = ToStringView("GaussianBlur");
    _computeModule          = wgpuDeviceCreateShaderModule(_device, &shaderDesc);

    // ─── Bind group layout: read-only source storage texture (0) + write-only dest storage
    // texture (1) + PushConstants uniform (2), all compute-stage only ─────────────────────────────
    std::array<WGPUBindGroupLayoutEntry, 3> bglEntries{};
    bglEntries[0].binding                    = 0;
    bglEntries[0].visibility                 = WGPUShaderStage_Compute;
    bglEntries[0].storageTexture.access      = WGPUStorageTextureAccess_ReadOnly;
    bglEntries[0].storageTexture.format      = kFormat;
    bglEntries[0].storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    bglEntries[1].binding                    = 1;
    bglEntries[1].visibility                 = WGPUShaderStage_Compute;
    bglEntries[1].storageTexture.access      = WGPUStorageTextureAccess_WriteOnly;
    bglEntries[1].storageTexture.format      = kFormat;
    bglEntries[1].storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    bglEntries[2].binding                    = 2;
    bglEntries[2].visibility                 = WGPUShaderStage_Compute;
    bglEntries[2].buffer.type                = WGPUBufferBindingType_Uniform;
    bglEntries[2].buffer.minBindingSize      = sizeof(PushConstants);

    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = static_cast<size_t>(bglEntries.size());
    bglDesc.entries     = bglEntries.data();
    _bindGroupLayout     = wgpuDeviceCreateBindGroupLayout(_device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc{};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts      = &_bindGroupLayout;
    _pipelineLayout               = wgpuDeviceCreatePipelineLayout(_device, &plDesc);

    WGPUComputePipelineDescriptor pipelineDesc{};
    pipelineDesc.layout             = _pipelineLayout;
    pipelineDesc.compute.module      = _computeModule;
    pipelineDesc.compute.entryPoint = ToStringView("CSMain");
    _computePipeline                  = wgpuDeviceCreateComputePipeline(_device, &pipelineDesc);

    WGPUBufferDescriptor pass1Desc{};
    pass1Desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    pass1Desc.size  = sizeof(PushConstants);
    _pass1ConstantsBuffer = wgpuDeviceCreateBuffer(_device, &pass1Desc);

    WGPUBufferDescriptor pass2Desc{};
    pass2Desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    pass2Desc.size  = sizeof(PushConstants);
    _pass2ConstantsBuffer = wgpuDeviceCreateBuffer(_device, &pass2Desc);

    _initialized = true;
}

void BlurPassWebGPU::EnsureTargets(std::uint32_t width, std::uint32_t height) {
    if (_textureA && _targetWidth == width && _targetHeight == height) { return; }

    if (_pass2BindGroup) { wgpuBindGroupRelease(_pass2BindGroup); _pass2BindGroup = nullptr; }
    if (_viewB) { wgpuTextureViewRelease(_viewB); _viewB = nullptr; }
    if (_textureB) { wgpuTextureRelease(_textureB); _textureB = nullptr; }
    if (_viewA) { wgpuTextureViewRelease(_viewA); _viewA = nullptr; }
    if (_textureA) { wgpuTextureRelease(_textureA); _textureA = nullptr; }

    _textureA = CreateStorageTexture(_device, width, height);
    _viewA    = CreateView(_textureA);
    _textureB = CreateStorageTexture(_device, width, height);
    _viewB    = CreateView(_textureB);

    std::array<WGPUBindGroupEntry, 3> pass2Entries{};
    pass2Entries[0].binding    = 0;
    pass2Entries[0].textureView = _viewA;
    pass2Entries[1].binding    = 1;
    pass2Entries[1].textureView = _viewB;
    pass2Entries[2].binding = 2;
    pass2Entries[2].buffer   = _pass2ConstantsBuffer;
    pass2Entries[2].offset   = 0;
    pass2Entries[2].size     = sizeof(PushConstants);

    WGPUBindGroupDescriptor pass2BgDesc{};
    pass2BgDesc.layout      = _bindGroupLayout;
    pass2BgDesc.entryCount = static_cast<size_t>(pass2Entries.size());
    pass2BgDesc.entries     = pass2Entries.data();
    _pass2BindGroup           = wgpuDeviceCreateBindGroup(_device, &pass2BgDesc);

    _targetWidth  = width;
    _targetHeight = height;
}

BlurResult BlurPassWebGPU::Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius) {
    const float clampedRadius = std::min(radius, 64.0f);
    if (clampedRadius <= 0.0f) { return source; }

    EnsureInitialized();
    EnsureTargets(width, height);

    // Write both passes' PushConstants entirely before recording either dispatch -- see this
    // class's own file comment on why a single, rewritten-between-passes buffer would not work.
    const PushConstants pass1Constants{1.0f, 0.0f, clampedRadius};
    const PushConstants pass2Constants{0.0f, 1.0f, clampedRadius};
    wgpuQueueWriteBuffer(_queue, _pass1ConstantsBuffer, 0, &pass1Constants, sizeof(pass1Constants));
    wgpuQueueWriteBuffer(_queue, _pass2ConstantsBuffer, 0, &pass2Constants, sizeof(pass2Constants));

    // Pass 1's own bind group varies every call (its own source binding) -- created fresh here,
    // matching RenderImageBatch()'s own identical idiom, unlike pass 2's stable, EnsureTargets()-
    // owned bind group.
    std::array<WGPUBindGroupEntry, 3> pass1Entries{};
    pass1Entries[0].binding    = 0;
    pass1Entries[0].textureView = source.View;
    pass1Entries[1].binding    = 1;
    pass1Entries[1].textureView = _viewA;
    pass1Entries[2].binding = 2;
    pass1Entries[2].buffer   = _pass1ConstantsBuffer;
    pass1Entries[2].offset   = 0;
    pass1Entries[2].size     = sizeof(PushConstants);

    WGPUBindGroupDescriptor pass1BgDesc{};
    pass1BgDesc.layout      = _bindGroupLayout;
    pass1BgDesc.entryCount = static_cast<size_t>(pass1Entries.size());
    pass1BgDesc.entries     = pass1Entries.data();
    WGPUBindGroup pass1BindGroup = wgpuDeviceCreateBindGroup(_device, &pass1BgDesc);

    const auto workgroupsX = static_cast<std::uint32_t>((width + kWorkgroupSize - 1) / kWorkgroupSize);
    const auto workgroupsY = static_cast<std::uint32_t>((height + kWorkgroupSize - 1) / kWorkgroupSize);

    WGPUCommandEncoderDescriptor encoderDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encoderDesc);

    WGPUComputePassDescriptor passDesc{};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);

    wgpuComputePassEncoderSetPipeline(pass, _computePipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, pass1BindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, workgroupsX, workgroupsY, 1);

    wgpuComputePassEncoderSetBindGroup(pass, 0, _pass2BindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, workgroupsX, workgroupsY, 1);

    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(_queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);
    // No fence/wait needed -- WebGPU's sequential submit model guarantees this submit's work
    // completes before the next one on this queue, matching NativeRendererWebGPU::Render()'s own
    // identical finding.

    wgpuBindGroupRelease(pass1BindGroup);

    return BlurResult{_textureB, _viewB};
}

void BlurPassWebGPU::Shutdown() {
    if (!_initialized) { return; }

    if (_pass2BindGroup) { wgpuBindGroupRelease(_pass2BindGroup); _pass2BindGroup = nullptr; }
    if (_viewB) { wgpuTextureViewRelease(_viewB); _viewB = nullptr; }
    if (_textureB) { wgpuTextureRelease(_textureB); _textureB = nullptr; }
    if (_viewA) { wgpuTextureViewRelease(_viewA); _viewA = nullptr; }
    if (_textureA) { wgpuTextureRelease(_textureA); _textureA = nullptr; }
    _targetWidth  = 0;
    _targetHeight = 0;

    if (_pass1ConstantsBuffer) { wgpuBufferRelease(_pass1ConstantsBuffer); _pass1ConstantsBuffer = nullptr; }
    if (_pass2ConstantsBuffer) { wgpuBufferRelease(_pass2ConstantsBuffer); _pass2ConstantsBuffer = nullptr; }

    if (_computePipeline) { wgpuComputePipelineRelease(_computePipeline); _computePipeline = nullptr; }
    if (_pipelineLayout) { wgpuPipelineLayoutRelease(_pipelineLayout); _pipelineLayout = nullptr; }
    if (_bindGroupLayout) { wgpuBindGroupLayoutRelease(_bindGroupLayout); _bindGroupLayout = nullptr; }
    if (_computeModule) { wgpuShaderModuleRelease(_computeModule); _computeModule = nullptr; }

    _initialized = false;
}

} // namespace ImFrame::Internal
