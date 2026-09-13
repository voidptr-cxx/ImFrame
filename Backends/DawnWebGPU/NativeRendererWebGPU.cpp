/**
 * @file     NativeRendererWebGPU.cpp
 * @brief    `NativeRendererWebGPU` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-14
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "NativeRendererWebGPU.hpp"

#include "ImFrame/Core/Error.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace ImFrame::Internal {

namespace {

/// SDFRect's WGSL port — see NativeRendererWebGPU.hpp's own file comment for why this is inlined
/// as a raw string rather than routed through a Shaders/*.glsl-style offline-compile CMake
/// pipeline (WGSL is compiled at runtime by Dawn, unlike SPIR-V/DXIL). Vertex shader negates Y,
/// matching Shaders/SDFRect.hlsl's own D3D convention (WebGPU's NDC is Y-up, like D3D/Metal,
/// unlike Vulkan's Y-down NDC) -- see that file's own comment for the full Vulkan-vs-D3D
/// derivation; the same reasoning applies here. RoundedBoxSdf() uses select() in place of GLSL/
/// HLSL's ternary/swizzle-assignment idiom, which WGSL lacks.
constexpr const char* kSDFRectWgsl = R"WGSL(
struct PerFrame {
    viewportSize: vec2f,
};
@group(0) @binding(0) var<uniform> uPerFrame: PerFrame;

struct VSInput {
    @location(0) position: vec2f,
    @location(1) local: vec2f,
    @location(2) halfSize: vec2f,
    @location(3) radii: vec4f,
    @location(4) fillColor: vec4f,
    @location(5) strokeColor: vec4f,
    @location(6) strokeWidth: f32,
};

struct VSOutput {
    @builtin(position) clipPosition: vec4f,
    @location(0) local: vec2f,
    @location(1) halfSize: vec2f,
    @location(2) radii: vec4f,
    @location(3) fillColor: vec4f,
    @location(4) strokeColor: vec4f,
    @location(5) strokeWidth: f32,
};

@vertex
fn VSMain(input: VSInput) -> VSOutput {
    var output: VSOutput;
    let ndc = (input.position / uPerFrame.viewportSize) * 2.0 - vec2f(1.0, 1.0);
    output.clipPosition = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    output.local        = input.local;
    output.halfSize     = input.halfSize;
    output.radii        = input.radii;
    output.fillColor    = input.fillColor;
    output.strokeColor  = input.strokeColor;
    output.strokeWidth  = input.strokeWidth;
    return output;
}

// Distance from p to a box of half-size b, with per-corner radii r = (TopRight, BottomRight,
// TopLeft, BottomLeft) -- Inigo Quilez's rounded-box SDF. Same math as SDFRect.glsl/SDFRect.hlsl's
// own RoundedBoxSdf(), ported to WGSL's immutable function parameters via select().
fn RoundedBoxSdf(p: vec2f, b: vec2f, r: vec4f) -> f32 {
    let rxy = select(r.zw, r.xy, p.x > 0.0);
    let rx  = select(rxy.y, rxy.x, p.y > 0.0);
    let q   = abs(p) - b + vec2f(rx);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2f(0.0))) - rx;
}

@fragment
fn PSMain(input: VSOutput) -> @location(0) vec4f {
    // Reorder from CornerRadii's (TopLeft, TopRight, BottomRight, BottomLeft) to the
    // (TopRight, BottomRight, TopLeft, BottomLeft) order RoundedBoxSdf expects.
    let r    = vec4f(input.radii.y, input.radii.z, input.radii.x, input.radii.w);
    let dist = RoundedBoxSdf(input.local, input.halfSize, r);

    let aa        = max(fwidth(dist) * 0.5, 1e-4);
    let fillAlpha = 1.0 - smoothstep(-aa, aa, dist);
    var color     = input.fillColor * fillAlpha;

    // fwidth() must be called from uniform control flow in WGSL (unlike GLSL/HLSL, which allow it
    // inside a varying-dependent `if`) -- so strokeAa/strokeAlpha are computed unconditionally here
    // and only the resulting blend is gated behind the branch.
    let strokeDist  = abs(dist) - input.strokeWidth * 0.5;
    let strokeAa    = max(fwidth(strokeDist) * 0.5, 1e-4);
    let strokeAlpha = 1.0 - smoothstep(-strokeAa, strokeAa, strokeDist);
    if (input.strokeWidth > 0.0) {
        color = mix(color, input.strokeColor, strokeAlpha * input.strokeColor.a);
    }

    return color;
}
)WGSL";

/// Host-side mirror of the WGSL `PerFrame` uniform, padded to 16 bytes -- WGSL itself only
/// declares `viewportSize: vec2f` (8 bytes), but the backing buffer is created a little larger to
/// stay clear of any backend-specific minimum-uniform-buffer-size edge case (Dawn translates the
/// binding size down internally; extra buffer capacity beyond what the shader reads is always
/// valid in WebGPU).
struct PerFrameUniform {
    float ViewportSizeX;
    float ViewportSizeY;
    float Pad0;
    float Pad1;
};

WGPUStringView ToStringView(const char* str) { return WGPUStringView{str, WGPU_STRLEN}; }

} // namespace

NativeRendererWebGPU::NativeRendererWebGPU(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat colorFormat)
    : _device(device), _queue(queue), _colorFormat(colorFormat) {}

NativeRendererWebGPU::~NativeRendererWebGPU() { Shutdown(); }

void NativeRendererWebGPU::SetTarget(WGPUTextureView targetView, std::uint32_t width,
                                     std::uint32_t height) noexcept {
    _targetView   = targetView;
    _targetWidth  = width;
    _targetHeight = height;
}

Result<Rendering::FontId> NativeRendererWebGPU::LoadFont(const Utility::Path& /*path*/, float /*sizePixels*/) {
    // No text pipeline exists yet -- this sub-phase's scope is BatchKind::Rect only, matching
    // NativeRendererVulkan/NativeRendererDX12's own identical "no attached text renderer" fallback.
    return std::unexpected(Error::FontLoadFailed);
}

void NativeRendererWebGPU::EnsureInitialized() {
    if (_initialized) { return; }

    // ─── Shader module: WGSL compiled at runtime by Dawn ───────────────────────────────────────
    WGPUShaderSourceWGSL wgslSource{};
    wgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslSource.code        = ToStringView(kSDFRectWgsl);

    WGPUShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = &wgslSource.chain;
    shaderDesc.label       = ToStringView("SDFRect");
    _shaderModule           = wgpuDeviceCreateShaderModule(_device, &shaderDesc);

    // ─── Bind group layout: one uniform buffer (PerFrame.viewportSize), vertex-stage only ───────
    WGPUBindGroupLayoutEntry bglEntry{};
    bglEntry.binding             = 0;
    bglEntry.visibility          = WGPUShaderStage_Vertex;
    bglEntry.buffer.type         = WGPUBufferBindingType_Uniform;
    bglEntry.buffer.minBindingSize = sizeof(PerFrameUniform);

    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries     = &bglEntry;
    _bindGroupLayout     = wgpuDeviceCreateBindGroupLayout(_device, &bglDesc);

    // ─── Pipeline layout: the one bind group above, no push-constant/root-CBV equivalent exists ─
    WGPUPipelineLayoutDescriptor plDesc{};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts      = &_bindGroupLayout;
    _pipelineLayout               = wgpuDeviceCreatePipelineLayout(_device, &plDesc);

    // ─── Rect pipeline: VSMain/PSMain above, matching RectVertex's field layout ─────────────────
    // Designated initializers (not positional) -- WGPUVertexAttribute's first member is
    // nextInChain, unlike D3D12_INPUT_ELEMENT_DESC/VkVertexInputAttributeDescription's own shapes.
    const std::array<WGPUVertexAttribute, 7> attributes{{
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(RectVertex, Position), .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(RectVertex, Local), .shaderLocation = 1},
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(RectVertex, HalfSize), .shaderLocation = 2},
        {.format = WGPUVertexFormat_Float32x4, .offset = offsetof(RectVertex, Radii), .shaderLocation = 3},
        {.format = WGPUVertexFormat_Float32x4, .offset = offsetof(RectVertex, FillColor), .shaderLocation = 4},
        {.format = WGPUVertexFormat_Float32x4, .offset = offsetof(RectVertex, StrokeColor), .shaderLocation = 5},
        {.format = WGPUVertexFormat_Float32, .offset = offsetof(RectVertex, StrokeWidth), .shaderLocation = 6},
    }};

    WGPUVertexBufferLayout vbLayout{};
    vbLayout.arrayStride    = sizeof(RectVertex);
    vbLayout.stepMode        = WGPUVertexStepMode_Vertex;
    vbLayout.attributeCount = attributes.size();
    vbLayout.attributes      = attributes.data();

    WGPUBlendComponent blendComponent{};
    blendComponent.operation = WGPUBlendOperation_Add;
    blendComponent.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendComponent.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    // Standard (non-premultiplied-alpha) "over" blend -- matches NativeRendererVulkan/
    // NativeRendererDX12's own default, for cross-backend visual consistency.
    WGPUBlendState blendState{};
    blendState.color = blendComponent;
    blendState.alpha = blendComponent;

    WGPUColorTargetState colorTarget{};
    colorTarget.format    = _colorFormat;
    colorTarget.blend      = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState{};
    fragmentState.module      = _shaderModule;
    fragmentState.entryPoint = ToStringView("PSMain");
    fragmentState.targetCount = 1;
    fragmentState.targets      = &colorTarget;

    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout                    = _pipelineLayout;
    pipelineDesc.vertex.module              = _shaderModule;
    pipelineDesc.vertex.entryPoint         = ToStringView("VSMain");
    pipelineDesc.vertex.bufferCount        = 1;
    pipelineDesc.vertex.buffers             = &vbLayout;
    pipelineDesc.primitive.topology        = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.cullMode        = WGPUCullMode_None;
    pipelineDesc.multisample.count         = 1;
    pipelineDesc.multisample.mask           = 0xFFFFFFFFu;
    pipelineDesc.fragment                   = &fragmentState;

    _rectPipeline = wgpuDeviceCreateRenderPipeline(_device, &pipelineDesc);

    // ─── PerFrame uniform buffer + its bind group ────────────────────────────────────────────────
    WGPUBufferDescriptor perFrameDesc{};
    perFrameDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    perFrameDesc.size  = sizeof(PerFrameUniform);
    _perFrameBuffer     = wgpuDeviceCreateBuffer(_device, &perFrameDesc);

    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer   = _perFrameBuffer;
    bgEntry.offset   = 0;
    bgEntry.size     = sizeof(PerFrameUniform);

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries     = &bgEntry;
    _bindGroup          = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    _initialized = true;
}

void NativeRendererWebGPU::EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes) {
    if (vertexBytes > _vertexBufferCapacityBytes) {
        if (_vertexBuffer) { wgpuBufferRelease(_vertexBuffer); }
        WGPUBufferDescriptor desc{};
        desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        desc.size  = vertexBytes;
        _vertexBuffer               = wgpuDeviceCreateBuffer(_device, &desc);
        _vertexBufferCapacityBytes = vertexBytes;
    }
    if (indexBytes > _indexBufferCapacityBytes) {
        if (_indexBuffer) { wgpuBufferRelease(_indexBuffer); }
        WGPUBufferDescriptor desc{};
        desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        desc.size  = indexBytes;
        _indexBuffer               = wgpuDeviceCreateBuffer(_device, &desc);
        _indexBufferCapacityBytes = indexBytes;
    }
}

void NativeRendererWebGPU::RenderRectBatch(WGPURenderPassEncoder pass, const Batch& batch,
                                           std::size_t& vertexByteOffset, std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    const std::size_t vertexBytes = vertices.size() * sizeof(RectVertex);
    const std::size_t indexBytes  = batch.Indices.size() * sizeof(std::uint32_t);

    wgpuQueueWriteBuffer(_queue, _vertexBuffer, vertexByteOffset, vertices.data(), vertexBytes);
    wgpuQueueWriteBuffer(_queue, _indexBuffer, indexByteOffset, batch.Indices.data(), indexBytes);

    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, _vertexBuffer, vertexByteOffset, vertexBytes);
    wgpuRenderPassEncoderSetIndexBuffer(pass, _indexBuffer, WGPUIndexFormat_Uint32, indexByteOffset, indexBytes);
    wgpuRenderPassEncoderDrawIndexed(pass, static_cast<uint32_t>(batch.Indices.size()), 1, 0, 0, 0);

    vertexByteOffset += vertexBytes;
    indexByteOffset += indexBytes;
}

void NativeRendererWebGPU::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    // A missing SetTarget() call is a caller bug, not a runtime condition to recover from --
    // matches NativeRendererVulkan/NativeRendererDX12's own equivalent contract.
    IMF_ASSERT(_targetView != nullptr);

    _batchBuilder.Build(buffer);
    const std::vector<Batch>& batches = _batchBuilder.Batches();

    std::size_t totalVertexBytes = 0;
    std::size_t totalIndexBytes  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) {
            totalVertexBytes += std::get<std::vector<RectVertex>>(batch.Vertices).size() * sizeof(RectVertex);
            totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
        }
        // Every other BatchKind (Image/Text/Shadow/Layer/BackdropBlur) is out of this sub-phase's
        // scope, matching NativeRendererVulkan/NativeRendererDX12's own identical starting point.
    }
    if (totalVertexBytes == 0) { return; }

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    const PerFrameUniform perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight), 0.0f, 0.0f};
    wgpuQueueWriteBuffer(_queue, _perFrameBuffer, 0, &perFrame, sizeof(perFrame));

    WGPUCommandEncoderDescriptor encoderDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view       = _targetView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp      = WGPULoadOp_Load;
    colorAttachment.storeOp     = WGPUStoreOp_Store;

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments      = &colorAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

    wgpuRenderPassEncoderSetViewport(pass, 0.0f, 0.0f, static_cast<float>(_targetWidth),
                                     static_cast<float>(_targetHeight), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(pass, 0, 0, _targetWidth, _targetHeight);

    wgpuRenderPassEncoderSetPipeline(pass, _rectPipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, _bindGroup, 0, nullptr);

    std::size_t vertexByteOffset = 0;
    std::size_t indexByteOffset  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) { RenderRectBatch(pass, batch, vertexByteOffset, indexByteOffset); }
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(_queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);
    // WebGPU's sequential submit model guarantees this submit's work completes before the next
    // one on this queue -- no explicit fence needed, matching ViewportWebGPU.cpp's own identical
    // finding. The streaming vertex/index/uniform buffers are safe to overwrite again on the very
    // next Render() call for the same reason.
}

void NativeRendererWebGPU::Shutdown() {
    if (!_initialized) { return; }

    if (_vertexBuffer) { wgpuBufferRelease(_vertexBuffer); _vertexBuffer = nullptr; }
    _vertexBufferCapacityBytes = 0;

    if (_indexBuffer) { wgpuBufferRelease(_indexBuffer); _indexBuffer = nullptr; }
    _indexBufferCapacityBytes = 0;

    if (_bindGroup) { wgpuBindGroupRelease(_bindGroup); _bindGroup = nullptr; }
    if (_perFrameBuffer) { wgpuBufferRelease(_perFrameBuffer); _perFrameBuffer = nullptr; }

    if (_rectPipeline) { wgpuRenderPipelineRelease(_rectPipeline); _rectPipeline = nullptr; }
    if (_pipelineLayout) { wgpuPipelineLayoutRelease(_pipelineLayout); _pipelineLayout = nullptr; }
    if (_bindGroupLayout) { wgpuBindGroupLayoutRelease(_bindGroupLayout); _bindGroupLayout = nullptr; }
    if (_shaderModule) { wgpuShaderModuleRelease(_shaderModule); _shaderModule = nullptr; }

    _initialized = false;
}

} // namespace ImFrame::Internal
