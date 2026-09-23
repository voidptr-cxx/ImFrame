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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

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

/// `Image.hlsl`'s WGSL port, mirroring `kSDFRectWgsl`'s own inline-source convention (see this
/// file's own header comment). Unlike GLSL's combined `sampler2D`/HLSL's `Texture2D`+`SamplerState`
/// pair, WGSL also splits texture and sampler into two separate bindings — `texture_2d<f32>` (t1)
/// and `sampler` (t2) — alongside the same `PerFrame` uniform (binding 0) `kSDFRectWgsl` already
/// declares, all in one `@group(0)` bind group (`NativeRendererWebGPU::RenderImageBatch()` creates
/// a fresh one per batch — see this class's own `.hpp` file comment for why that's safe here,
/// unlike Vulkan's per-batch descriptor-*set* write or DX12's per-batch descriptor-heap-*slot*
/// write).
constexpr const char* kImageWgsl = R"WGSL(
struct PerFrame {
    viewportSize: vec2f,
};
@group(0) @binding(0) var<uniform> uPerFrame: PerFrame;
@group(0) @binding(1) var uTexture: texture_2d<f32>;
@group(0) @binding(2) var uSampler: sampler;

struct VSInput {
    @location(0) position: vec2f,
    @location(1) local: vec2f,
    @location(2) halfSize: vec2f,
    @location(3) radii: vec4f,
    @location(4) uv: vec2f,
    @location(5) tintColor: vec4f,
};

struct VSOutput {
    @builtin(position) clipPosition: vec4f,
    @location(0) local: vec2f,
    @location(1) halfSize: vec2f,
    @location(2) radii: vec4f,
    @location(3) uv: vec2f,
    @location(4) tintColor: vec4f,
};

@vertex
fn VSMain(input: VSInput) -> VSOutput {
    var output: VSOutput;
    let ndc = (input.position / uPerFrame.viewportSize) * 2.0 - vec2f(1.0, 1.0);
    output.clipPosition = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    output.local        = input.local;
    output.halfSize     = input.halfSize;
    output.radii        = input.radii;
    output.uv           = input.uv;
    output.tintColor    = input.tintColor;
    return output;
}

// Identical to kSDFRectWgsl's own RoundedBoxSdf() -- duplicated rather than shared, matching
// Image.glsl/Image.hlsl's own precedent of not sharing shader code across files in this codebase.
fn RoundedBoxSdf(p: vec2f, b: vec2f, r: vec4f) -> f32 {
    let rxy = select(r.zw, r.xy, p.x > 0.0);
    let rx  = select(rxy.y, rxy.x, p.y > 0.0);
    let q   = abs(p) - b + vec2f(rx);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2f(0.0))) - rx;
}

@fragment
fn PSMain(input: VSOutput) -> @location(0) vec4f {
    let r    = vec4f(input.radii.y, input.radii.z, input.radii.x, input.radii.w);
    let dist = RoundedBoxSdf(input.local, input.halfSize, r);

    let aa   = max(fwidth(dist) * 0.5, 1e-4);
    let mask = 1.0 - smoothstep(-aa, aa, dist);
    return textureSample(uTexture, uSampler, input.uv) * input.tintColor * mask;
}
)WGSL";

/// Blend.hlsl's/Blend.glsl's WGSL port (Phase 35.17) -- see Shaders/Blend.glsl's own header comment
/// for the unpremultiply-blend-recomposite derivation, ported here unchanged. Like Blend.hlsl (and
/// unlike Blend.glsl), the fixed fullscreen-quad UV table has every V component flipped relative to
/// Blend.glsl's own: WebGPU's NDC is Y-up (like D3D's -- see kSDFRectWgsl's own VSMain comment), so
/// position (-1,-1) is screen-BOTTOM-left here, not screen-top-left as it is under Vulkan's Y-down
/// NDC. Blend.hlsl's own first DX12 port copied Blend.glsl's table unchanged and sampled a
/// vertically mirrored row as a result (Phase 35.13); this port applies the fix from the start.
/// select() with a vec3<bool> condition replaces HLSL's lerp(lo, hi, step(...)) idiom exactly
/// (per-component pick), since WGSL has no ternary.
constexpr const char* kBlendWgsl = R"WGSL(
struct BlendParams {
    mode: i32,
};
@group(0) @binding(0) var<uniform> uParams: BlendParams;
@group(0) @binding(1) var uSourceTexture: texture_2d<f32>;
@group(0) @binding(2) var uBackdropTexture: texture_2d<f32>;
@group(0) @binding(3) var uSampler: sampler;

struct VSOutput {
    @builtin(position) clipPosition: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn VSMain(@builtin(vertex_index) vertexIndex: u32) -> VSOutput {
    var positions = array<vec2f, 6>(
        vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
        vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));
    var uvs = array<vec2f, 6>(
        vec2f(0.0, 1.0), vec2f(1.0, 1.0), vec2f(1.0, 0.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(0.0, 0.0));
    var output: VSOutput;
    output.clipPosition = vec4f(positions[vertexIndex], 0.0, 1.0);
    output.uv           = uvs[vertexIndex];
    return output;
}

fn Overlay(cb: vec3f, cs: vec3f) -> vec3f {
    let lo = 2.0 * cb * cs;
    let hi = 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs);
    return select(lo, hi, cb >= vec3f(0.5)); // branches on the BACKDROP's own brightness
}

fn HardLight(cb: vec3f, cs: vec3f) -> vec3f {
    let lo = 2.0 * cb * cs;
    let hi = 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs);
    return select(lo, hi, cs >= vec3f(0.5)); // branches on the SOURCE's own brightness
}

fn SoftLightD(x: vec3f) -> vec3f {
    let poly = ((16.0 * x - 12.0) * x + 4.0) * x;
    return select(poly, sqrt(x), x >= vec3f(0.25));
}

fn SoftLight(cb: vec3f, cs: vec3f) -> vec3f {
    let dark  = cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
    let light = cb + (2.0 * cs - 1.0) * (SoftLightD(cb) - cb);
    return select(dark, light, cs >= vec3f(0.5));
}

fn BlendColors(cb: vec3f, cs: vec3f, mode: i32) -> vec3f {
    if (mode == 1) { return cb * cs; }                                         // Multiply
    if (mode == 2) { return cb + cs - cb * cs; }                               // Screen
    if (mode == 3) { return Overlay(cb, cs); }
    if (mode == 4) { return min(cb, cs); }                                     // Darken
    if (mode == 5) { return max(cb, cs); }                                     // Lighten
    if (mode == 6) { return min(vec3f(1.0), cb / max(vec3f(1.0) - cs, vec3f(1e-4))); }       // ColorDodge
    if (mode == 7) { return vec3f(1.0) - min(vec3f(1.0), (vec3f(1.0) - cb) / max(cs, vec3f(1e-4))); } // ColorBurn
    if (mode == 8) { return HardLight(cb, cs); }
    if (mode == 9) { return SoftLight(cb, cs); }
    if (mode == 10) { return abs(cb - cs); }                                   // Difference
    if (mode == 11) { return cb + cs - 2.0 * cb * cs; }                        // Exclusion
    return cs;                                                                  // Normal (mode == 0)
}

@fragment
fn PSMain(input: VSOutput) -> @location(0) vec4f {
    let src      = textureSample(uSourceTexture, uSampler, input.uv);
    let backdrop = textureSample(uBackdropTexture, uSampler, input.uv);

    let srcAlpha      = src.a;
    let backdropAlpha = backdrop.a;
    let cs = select(vec3f(0.0), src.rgb / max(srcAlpha, 1e-6), srcAlpha > 0.0);
    let cb = select(vec3f(0.0), backdrop.rgb / max(backdropAlpha, 1e-6), backdropAlpha > 0.0);

    let blended = BlendColors(cb, cs, uParams.mode);

    let resultRgb = (1.0 - backdropAlpha) * srcAlpha * cs
                  + backdropAlpha * srcAlpha * blended
                  + (1.0 - srcAlpha) * backdropAlpha * cb;
    let resultAlpha = srcAlpha + backdropAlpha - srcAlpha * backdropAlpha;

    return vec4f(resultRgb, resultAlpha);
}
)WGSL";

/// Host-side mirror of kBlendWgsl's `BlendParams` uniform, padded to 16 bytes.
struct BlendParamsUniform {
    std::int32_t Mode;
    std::int32_t Pad0;
    std::int32_t Pad1;
    std::int32_t Pad2;
};

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

/// Builds one axis-aligned quad's `RectVertex`es — `RenderShadowBatch()`'s own silhouette pass
/// reuses the existing Rect pipeline via a direct draw call rather than a third hand-written
/// pipeline, mirroring `NativeRendererVulkan`'s/`NativeRendererDX12`'s own identical role/formula
/// (Phase 35.4/35.12).
std::vector<RectVertex> BuildRectQuadVertices(Widgets::Vec2 position, Widgets::Vec2 size, Rendering::CornerRadii radii,
                                              Widgets::Vec4 fillColor) {
    const Widgets::Vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    const Widgets::Vec2 halfSize{size.x * 0.5f, size.y * 0.5f};
    const Widgets::Vec2 corners[4] = {
        {position.x, position.y},
        {position.x + size.x, position.y},
        {position.x + size.x, position.y + size.y},
        {position.x, position.y + size.y},
    };

    std::vector<RectVertex> vertices;
    vertices.reserve(4);
    for (const Widgets::Vec2& corner : corners) {
        vertices.push_back(RectVertex{
            .Position = corner,
            .Local = {corner.x - center.x, corner.y - center.y},
            .HalfSize = halfSize,
            .Radii = radii,
            .FillColor = fillColor,
            .StrokeColor = {},
            .StrokeWidth = 0.0f,
        });
    }
    return vertices;
}

/// Builds one axis-aligned quad's `ImageVertex`es — `RenderShadowBatch()`'s own composite pass
/// reuses the existing Image pipeline to draw a renderer-produced offscreen texture (a blurred
/// shadow), tinted, as a plain rectangle. **Unflipped** UV table, matching `NativeRendererVulkan`'s/
/// `NativeRendererDX12`'s own choice, not `NativeRendererGL3`'s V-flipped one -- even though this
/// vertex shader negates Y like GL's/D3D's own convention (see `kSDFRectWgsl`'s own `VSMain`
/// comment), WebGPU's own spec defines texture coordinate (0,0) as the texture's top-left texel
/// unconditionally, regardless of which native backend (D3D12/Vulkan/Metal) Dawn actually targets
/// underneath -- the same "vertex-shader Y-handedness and texture-row order are independent
/// questions" reasoning `NativeRendererDX12`'s own Phase 35.12 finding already established, applied
/// here to a third, WebGPU-specific combination (Y-negating vertex shader + WebGPU's own backend-
/// agnostic top-down texture convention).
/// The optional `uvMin`/`uvMax`/`radii` parameters (Phase 35.18) match `NativeRendererVulkan`'s/
/// `NativeRendererDX12`'s own full signature -- `RenderBackdropBlurBatch()` crops its composite to
/// exactly the requested rect out of the larger padded, blurred copy.
std::vector<ImageVertex> BuildImageQuadVertices(Widgets::Vec2 position, Widgets::Vec2 size, Widgets::Vec4 tintColor,
                                                Widgets::Vec2 uvMin = {0.0f, 0.0f}, Widgets::Vec2 uvMax = {1.0f, 1.0f},
                                                Rendering::CornerRadii radii = {}) {
    const Widgets::Vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    const Widgets::Vec2 halfSize{size.x * 0.5f, size.y * 0.5f};
    const Widgets::Vec2 corners[4] = {
        {position.x, position.y},
        {position.x + size.x, position.y},
        {position.x + size.x, position.y + size.y},
        {position.x, position.y + size.y},
    };
    const Widgets::Vec2 uvs[4] = {{uvMin.x, uvMin.y}, {uvMax.x, uvMin.y}, {uvMax.x, uvMax.y}, {uvMin.x, uvMax.y}};

    std::vector<ImageVertex> vertices;
    vertices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        vertices.push_back(ImageVertex{
            .Position = corners[i],
            .Local = {corners[i].x - center.x, corners[i].y - center.y},
            .HalfSize = halfSize,
            .Radii = radii,
            .Uv = uvs[i],
            .TintColor = tintColor,
        });
    }
    return vertices;
}

} // namespace

NativeRendererWebGPU::NativeRendererWebGPU(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat colorFormat)
    : _device(device), _queue(queue), _colorFormat(colorFormat), _blurPass(device, queue) {}

NativeRendererWebGPU::~NativeRendererWebGPU() { Shutdown(); }

void NativeRendererWebGPU::SetTarget(WGPUTexture targetTexture, WGPUTextureView targetView, std::uint32_t width,
                                     std::uint32_t height) noexcept {
    _targetTexture = targetTexture;
    _targetView    = targetView;
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

    // ─── Image shader module ────────────────────────────────────────────────────────────────────
    WGPUShaderSourceWGSL imageWgslSource{};
    imageWgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    imageWgslSource.code        = ToStringView(kImageWgsl);

    WGPUShaderModuleDescriptor imageShaderDesc{};
    imageShaderDesc.nextInChain = &imageWgslSource.chain;
    imageShaderDesc.label       = ToStringView("Image");
    _imageShaderModule           = wgpuDeviceCreateShaderModule(_device, &imageShaderDesc);

    // ─── Image bind group layout: PerFrame uniform (0, vertex) + texture (1) + sampler (2, both
    // fragment) ──────────────────────────────────────────────────────────────────────────────────
    std::array<WGPUBindGroupLayoutEntry, 3> imageBglEntries{};
    imageBglEntries[0].binding                    = 0;
    imageBglEntries[0].visibility                 = WGPUShaderStage_Vertex;
    imageBglEntries[0].buffer.type                = WGPUBufferBindingType_Uniform;
    imageBglEntries[0].buffer.minBindingSize      = sizeof(PerFrameUniform);
    imageBglEntries[1].binding                    = 1;
    imageBglEntries[1].visibility                 = WGPUShaderStage_Fragment;
    imageBglEntries[1].texture.sampleType         = WGPUTextureSampleType_Float;
    imageBglEntries[1].texture.viewDimension      = WGPUTextureViewDimension_2D;
    imageBglEntries[2].binding                    = 2;
    imageBglEntries[2].visibility                 = WGPUShaderStage_Fragment;
    imageBglEntries[2].sampler.type               = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor imageBglDesc{};
    imageBglDesc.entryCount = static_cast<size_t>(imageBglEntries.size());
    imageBglDesc.entries     = imageBglEntries.data();
    _imageBindGroupLayout     = wgpuDeviceCreateBindGroupLayout(_device, &imageBglDesc);

    WGPUPipelineLayoutDescriptor imagePlDesc{};
    imagePlDesc.bindGroupLayoutCount = 1;
    imagePlDesc.bindGroupLayouts      = &_imageBindGroupLayout;
    _imagePipelineLayout              = wgpuDeviceCreatePipelineLayout(_device, &imagePlDesc);

    // ─── Image pipeline: VSMain/PSMain above, matching ImageVertex's field layout ───────────────
    const std::array<WGPUVertexAttribute, 6> imageAttributes{{
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(ImageVertex, Position), .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(ImageVertex, Local), .shaderLocation = 1},
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(ImageVertex, HalfSize), .shaderLocation = 2},
        {.format = WGPUVertexFormat_Float32x4, .offset = offsetof(ImageVertex, Radii), .shaderLocation = 3},
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(ImageVertex, Uv), .shaderLocation = 4},
        {.format = WGPUVertexFormat_Float32x4, .offset = offsetof(ImageVertex, TintColor), .shaderLocation = 5},
    }};

    WGPUVertexBufferLayout imageVbLayout{};
    imageVbLayout.arrayStride    = sizeof(ImageVertex);
    imageVbLayout.stepMode        = WGPUVertexStepMode_Vertex;
    imageVbLayout.attributeCount = imageAttributes.size();
    imageVbLayout.attributes      = imageAttributes.data();

    WGPUColorTargetState imageColorTarget{};
    imageColorTarget.format    = _colorFormat;
    imageColorTarget.blend      = &blendState; // same standard "over" blend as the Rect pipeline
    imageColorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState imageFragmentState{};
    imageFragmentState.module      = _imageShaderModule;
    imageFragmentState.entryPoint = ToStringView("PSMain");
    imageFragmentState.targetCount = 1;
    imageFragmentState.targets      = &imageColorTarget;

    WGPURenderPipelineDescriptor imagePipelineDesc{};
    imagePipelineDesc.layout                    = _imagePipelineLayout;
    imagePipelineDesc.vertex.module              = _imageShaderModule;
    imagePipelineDesc.vertex.entryPoint         = ToStringView("VSMain");
    imagePipelineDesc.vertex.bufferCount        = 1;
    imagePipelineDesc.vertex.buffers             = &imageVbLayout;
    imagePipelineDesc.primitive.topology        = WGPUPrimitiveTopology_TriangleList;
    imagePipelineDesc.primitive.cullMode        = WGPUCullMode_None;
    imagePipelineDesc.multisample.count         = 1;
    imagePipelineDesc.multisample.mask           = 0xFFFFFFFFu;
    imagePipelineDesc.fragment                   = &imageFragmentState;

    _imagePipeline = wgpuDeviceCreateRenderPipeline(_device, &imagePipelineDesc);

    // ─── Shared sampler -- every Image batch's bind group references this same one ─────────────
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter     = WGPUFilterMode_Linear;
    samplerDesc.minFilter     = WGPUFilterMode_Linear;
    // maxAnisotropy must be >= 1 -- a zero-initialized WGPUSamplerDescriptor leaves it at 0, which
    // Dawn rejects outright (not just "no anisotropic filtering"); 1 means "off", matching this
    // sampler's own fixed LINEAR/CLAMP intent (no anisotropic filtering requested).
    samplerDesc.maxAnisotropy = 1;
    _linearSampler             = wgpuDeviceCreateSampler(_device, &samplerDesc);

    // ─── Phase 35.17: premultiplied Image pipeline -- same layout/shaders/vertex layout as
    // _imagePipeline, only the blend factors differ (One/OneMinusSrcAlpha), for
    // CompositeOpacityLayer()'s own already-premultiplied captured render ──────────────────────────
    WGPUBlendComponent premultipliedComponent{};
    premultipliedComponent.operation = WGPUBlendOperation_Add;
    premultipliedComponent.srcFactor = WGPUBlendFactor_One;
    premultipliedComponent.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    WGPUBlendState premultipliedBlendState{};
    premultipliedBlendState.color = premultipliedComponent;
    premultipliedBlendState.alpha = premultipliedComponent;

    WGPUColorTargetState premultipliedColorTarget = imageColorTarget;
    premultipliedColorTarget.blend                = &premultipliedBlendState;

    WGPUFragmentState premultipliedFragmentState = imageFragmentState;
    premultipliedFragmentState.targets            = &premultipliedColorTarget;

    WGPURenderPipelineDescriptor premultipliedPipelineDesc = imagePipelineDesc;
    premultipliedPipelineDesc.fragment                     = &premultipliedFragmentState;
    _premultipliedImagePipeline = wgpuDeviceCreateRenderPipeline(_device, &premultipliedPipelineDesc);

    // ─── Phase 35.17: Blend shader module/layout/pipeline -- no vertex input at all (kBlendWgsl
    // generates its own fixed fullscreen quad from vertex_index), blending disabled (the shader
    // itself computes the full Porter-Duff-composited result; fixed-function blending on top would
    // double-composite it, matching NativeRendererVulkan's/NativeRendererDX12's own reasoning) ─────
    WGPUShaderSourceWGSL blendWgslSource{};
    blendWgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    blendWgslSource.code        = ToStringView(kBlendWgsl);

    WGPUShaderModuleDescriptor blendShaderDesc{};
    blendShaderDesc.nextInChain = &blendWgslSource.chain;
    blendShaderDesc.label       = ToStringView("Blend");
    _blendShaderModule           = wgpuDeviceCreateShaderModule(_device, &blendShaderDesc);

    std::array<WGPUBindGroupLayoutEntry, 4> blendBglEntries{};
    blendBglEntries[0].binding               = 0;
    blendBglEntries[0].visibility            = WGPUShaderStage_Fragment;
    blendBglEntries[0].buffer.type           = WGPUBufferBindingType_Uniform;
    blendBglEntries[0].buffer.minBindingSize = sizeof(BlendParamsUniform);
    blendBglEntries[1].binding               = 1;
    blendBglEntries[1].visibility            = WGPUShaderStage_Fragment;
    blendBglEntries[1].texture.sampleType    = WGPUTextureSampleType_Float;
    blendBglEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    blendBglEntries[2].binding               = 2;
    blendBglEntries[2].visibility            = WGPUShaderStage_Fragment;
    blendBglEntries[2].texture.sampleType    = WGPUTextureSampleType_Float;
    blendBglEntries[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    blendBglEntries[3].binding               = 3;
    blendBglEntries[3].visibility            = WGPUShaderStage_Fragment;
    blendBglEntries[3].sampler.type          = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor blendBglDesc{};
    blendBglDesc.entryCount = static_cast<size_t>(blendBglEntries.size());
    blendBglDesc.entries     = blendBglEntries.data();
    _blendBindGroupLayout     = wgpuDeviceCreateBindGroupLayout(_device, &blendBglDesc);

    WGPUPipelineLayoutDescriptor blendPlDesc{};
    blendPlDesc.bindGroupLayoutCount = 1;
    blendPlDesc.bindGroupLayouts      = &_blendBindGroupLayout;
    _blendPipelineLayout              = wgpuDeviceCreatePipelineLayout(_device, &blendPlDesc);

    WGPUColorTargetState blendColorTarget{};
    blendColorTarget.format    = _colorFormat;
    blendColorTarget.blend      = nullptr; // replace -- see the comment above
    blendColorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState blendFragmentState{};
    blendFragmentState.module      = _blendShaderModule;
    blendFragmentState.entryPoint = ToStringView("PSMain");
    blendFragmentState.targetCount = 1;
    blendFragmentState.targets      = &blendColorTarget;

    WGPURenderPipelineDescriptor blendPipelineDesc{};
    blendPipelineDesc.layout             = _blendPipelineLayout;
    blendPipelineDesc.vertex.module       = _blendShaderModule;
    blendPipelineDesc.vertex.entryPoint  = ToStringView("VSMain");
    blendPipelineDesc.vertex.bufferCount = 0;
    blendPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    blendPipelineDesc.primitive.cullMode = WGPUCullMode_None;
    blendPipelineDesc.multisample.count  = 1;
    blendPipelineDesc.multisample.mask    = 0xFFFFFFFFu;
    blendPipelineDesc.fragment            = &blendFragmentState;
    _blendPipeline = wgpuDeviceCreateRenderPipeline(_device, &blendPipelineDesc);

    WGPUBufferDescriptor blendModeDesc{};
    blendModeDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    blendModeDesc.size  = sizeof(BlendParamsUniform);
    _blendModeBuffer     = wgpuDeviceCreateBuffer(_device, &blendModeDesc);
    // ─── Phase 35.16: small, fixed-size dedicated buffers for RenderShadowBatch()'s own two
    // one-quad draws -- sized for the larger of RectVertex/ImageVertex (4 vertices), rewritten via
    // wgpuQueueWriteBuffer() before each use, never grown ────────────────────────────────────────
    constexpr std::size_t kShadowQuadVertexBytes =
        4 * (sizeof(RectVertex) > sizeof(ImageVertex) ? sizeof(RectVertex) : sizeof(ImageVertex));
    WGPUBufferDescriptor shadowVbDesc{};
    shadowVbDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    shadowVbDesc.size  = kShadowQuadVertexBytes;
    _shadowQuadVertexBuffer = wgpuDeviceCreateBuffer(_device, &shadowVbDesc);

    WGPUBufferDescriptor shadowIbDesc{};
    shadowIbDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    shadowIbDesc.size  = 6 * sizeof(std::uint32_t);
    _shadowQuadIndexBuffer = wgpuDeviceCreateBuffer(_device, &shadowIbDesc);

    _initialized = true;
}

void NativeRendererWebGPU::EnsureShadowSilhouetteTarget(std::uint32_t width, std::uint32_t height) {
    if (_shadowSilhouetteTexture && _shadowSilhouetteWidth == width && _shadowSilhouetteHeight == height) {
        return;
    }

    if (_shadowSilhouetteView) { wgpuTextureViewRelease(_shadowSilhouetteView); _shadowSilhouetteView = nullptr; }
    if (_shadowSilhouetteTexture) { wgpuTextureRelease(_shadowSilhouetteTexture); _shadowSilhouetteTexture = nullptr; }

    WGPUTextureDescriptor texDesc{};
    // Both RenderAttachment (silhouette draw) and StorageBinding (_blurPass's own read) -- used
    // directly in either role with no transition of any kind, see this class's own file comment.
    texDesc.usage         = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_StorageBinding;
    texDesc.dimension     = WGPUTextureDimension_2D;
    texDesc.size          = WGPUExtent3D{width, height, 1};
    texDesc.format        = _colorFormat;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount   = 1;
    _shadowSilhouetteTexture = wgpuDeviceCreateTexture(_device, &texDesc);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = _colorFormat;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    _shadowSilhouetteView     = wgpuTextureCreateView(_shadowSilhouetteTexture, &viewDesc);

    _shadowSilhouetteWidth  = width;
    _shadowSilhouetteHeight = height;
}

void NativeRendererWebGPU::BeginMainPass(WGPULoadOp loadOp) {
    WGPUCommandEncoderDescriptor encoderDesc{};
    _mainEncoder = wgpuDeviceCreateCommandEncoder(_device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view       = _currentTargetView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp      = loadOp;
    colorAttachment.storeOp     = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments      = &colorAttachment;

    _mainPass = wgpuCommandEncoderBeginRenderPass(_mainEncoder, &passDesc);

    // Every layer target, and the real target, is always _targetWidth x _targetHeight -- no
    // separate "current viewport" state to track, matching NativeRendererVulkan's own identical note.
    wgpuRenderPassEncoderSetViewport(_mainPass, 0.0f, 0.0f, static_cast<float>(_targetWidth),
                                     static_cast<float>(_targetHeight), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(_mainPass, 0, 0, _targetWidth, _targetHeight);
}

void NativeRendererWebGPU::SwitchMainPassTarget(WGPUTextureView view, WGPULoadOp loadOp) {
    wgpuRenderPassEncoderEnd(_mainPass);
    wgpuRenderPassEncoderRelease(_mainPass);
    _mainPass          = nullptr;
    _currentTargetView = view;

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view       = view;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp      = loadOp;
    colorAttachment.storeOp     = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments      = &colorAttachment;

    _mainPass = wgpuCommandEncoderBeginRenderPass(_mainEncoder, &passDesc);

    wgpuRenderPassEncoderSetViewport(_mainPass, 0.0f, 0.0f, static_cast<float>(_targetWidth),
                                     static_cast<float>(_targetHeight), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(_mainPass, 0, 0, _targetWidth, _targetHeight);
}

void NativeRendererWebGPU::EnsureLayerTarget(std::size_t depth, std::uint32_t width, std::uint32_t height) {
    if (_layerTargets.size() <= depth) { _layerTargets.resize(depth + 1); }

    LayerTarget& target = _layerTargets[depth];
    if (target.Texture && target.Width == width && target.Height == height) { return; }

    if (target.View) { wgpuTextureViewRelease(target.View); target.View = nullptr; }
    if (target.Texture) { wgpuTextureRelease(target.Texture); target.Texture = nullptr; }

    WGPUTextureDescriptor texDesc{};
    // RenderAttachment (while pushed) + TextureBinding (the composite draw's own sampling once
    // popped) -- used directly in either role, no transition of any kind (this class's own file
    // comment). CopySrc -- a nested PushBlendLayer's own backdrop copy reads its enclosing layer's
    // target as the copy source (LayerFrame::ParentTexture).
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    texDesc.dimension     = WGPUTextureDimension_2D;
    texDesc.size          = WGPUExtent3D{width, height, 1};
    texDesc.format        = _colorFormat;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount   = 1;
    target.Texture         = wgpuDeviceCreateTexture(_device, &texDesc);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = _colorFormat;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    target.View               = wgpuTextureCreateView(target.Texture, &viewDesc);

    target.Width  = width;
    target.Height = height;
}

void NativeRendererWebGPU::EnsureBackdropTarget(std::uint32_t width, std::uint32_t height) {
    if (_backdropTexture && _backdropWidth == width && _backdropHeight == height) { return; }

    if (_backdropView) { wgpuTextureViewRelease(_backdropView); _backdropView = nullptr; }
    if (_backdropTexture) { wgpuTextureRelease(_backdropTexture); _backdropTexture = nullptr; }

    WGPUTextureDescriptor texDesc{};
    texDesc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    texDesc.dimension     = WGPUTextureDimension_2D;
    texDesc.size          = WGPUExtent3D{width, height, 1};
    texDesc.format        = _colorFormat;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount   = 1;
    _backdropTexture       = wgpuDeviceCreateTexture(_device, &texDesc);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = _colorFormat;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    _backdropView             = wgpuTextureCreateView(_backdropTexture, &viewDesc);

    _backdropWidth  = width;
    _backdropHeight = height;
}

void NativeRendererWebGPU::EndAndSubmitMainPass() {
    wgpuRenderPassEncoderEnd(_mainPass);
    wgpuRenderPassEncoderRelease(_mainPass);
    _mainPass = nullptr;

    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(_mainEncoder, &cbDesc);
    wgpuCommandEncoderRelease(_mainEncoder);
    _mainEncoder = nullptr;

    wgpuQueueSubmit(_queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);
    // No fence/wait needed -- WebGPU's sequential submit model guarantees this submit's work
    // completes before the next one on this queue, matching this class's own file comment.
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

    // Rebinds unconditionally, regardless of what the previous batch (if any) bound -- matches
    // RenderImageBatch()'s own existing convention, needed now that a Shadow batch can end and
    // begin a fresh main pass mid-Render() call (Phase 35.16's own file comment).
    wgpuRenderPassEncoderSetPipeline(pass, _rectPipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, _bindGroup, 0, nullptr);

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

void NativeRendererWebGPU::RenderImageBatch(WGPURenderPassEncoder pass, const Batch& batch,
                                            std::size_t& vertexByteOffset, std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<ImageVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    // batch.Texture's value is a raw WGPUTextureView -- see this class's own file comment on why
    // (the WebGPU analogue of NativeRendererVulkan's raw-VkImageView convention).
    auto textureView = reinterpret_cast<WGPUTextureView>(static_cast<std::uintptr_t>(batch.Texture.Value()));

    std::array<WGPUBindGroupEntry, 3> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer   = _perFrameBuffer;
    bgEntries[0].offset   = 0;
    bgEntries[0].size     = sizeof(PerFrameUniform);
    bgEntries[1].binding    = 1;
    bgEntries[1].textureView = textureView;
    bgEntries[2].binding = 2;
    bgEntries[2].sampler  = _linearSampler;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _imageBindGroupLayout;
    bgDesc.entryCount = static_cast<size_t>(bgEntries.size());
    bgDesc.entries     = bgEntries.data();
    WGPUBindGroup imageBindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    wgpuRenderPassEncoderSetPipeline(pass, _imagePipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, imageBindGroup, 0, nullptr);

    const std::size_t vertexBytes = vertices.size() * sizeof(ImageVertex);
    const std::size_t indexBytes  = batch.Indices.size() * sizeof(std::uint32_t);

    wgpuQueueWriteBuffer(_queue, _vertexBuffer, vertexByteOffset, vertices.data(), vertexBytes);
    wgpuQueueWriteBuffer(_queue, _indexBuffer, indexByteOffset, batch.Indices.data(), indexBytes);

    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, _vertexBuffer, vertexByteOffset, vertexBytes);
    wgpuRenderPassEncoderSetIndexBuffer(pass, _indexBuffer, WGPUIndexFormat_Uint32, indexByteOffset, indexBytes);
    wgpuRenderPassEncoderDrawIndexed(pass, static_cast<uint32_t>(batch.Indices.size()), 1, 0, 0, 0);

    // Safe to release immediately -- a WGPUBindGroup is immutable from creation, and the render
    // pass encoder's own recording already captured what it needs from it via SetBindGroup(); see
    // this class's own .hpp file comment for why no Vulkan/DX12-style "keep it alive until the
    // whole command buffer has executed" concern applies here.
    wgpuBindGroupRelease(imageBindGroup);

    vertexByteOffset += vertexBytes;
    indexByteOffset += indexBytes;
}

void NativeRendererWebGPU::RenderShadowBatch(const Batch& batch) {
    const auto& shadows = std::get<std::vector<ShadowVertex>>(batch.Vertices);
    if (shadows.empty()) { return; }
    const ShadowVertex& shadow = shadows.front();

    // Spread grows the silhouette outward on all sides before blurring -- matches
    // Rendering::DrawShadow::Spread's documented meaning, mirroring NativeRendererVulkan's/
    // NativeRendererDX12's own identical formula.
    const float spreadWidth  = shadow.Size.x + 2.0f * shadow.Spread;
    const float spreadHeight = shadow.Size.y + 2.0f * shadow.Spread;
    if (spreadWidth <= 0.0f || spreadHeight <= 0.0f) { return; }

    // Pad the offscreen silhouette by the blur radius on every side so _blurPass's kernel has real
    // surrounding content to read at the silhouette's own edges, instead of clamped-edge repeats.
    const float pad = std::max(shadow.BlurRadius, 0.0f);
    const auto texWidth  = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(spreadWidth + 2.0f * pad))));
    const auto texHeight = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(spreadHeight + 2.0f * pad))));

    // End and submit the main pass as accumulated so far -- _blurPass.Apply() below is its own
    // separate submission; no fence wait is needed at this split (unlike Vulkan's/DX12's own
    // identical splits), see this class's own file comment.
    EndAndSubmitMainPass();

    EnsureShadowSilhouetteTarget(texWidth, texHeight);

    // ─── Silhouette: an opaque-white rounded rect, in the silhouette's own small coordinate space ───
    {
        const PerFrameUniform silhouettePerFrame{static_cast<float>(texWidth), static_cast<float>(texHeight), 0.0f,
                                                 0.0f};
        wgpuQueueWriteBuffer(_queue, _perFrameBuffer, 0, &silhouettePerFrame, sizeof(silhouettePerFrame));

        const std::vector<RectVertex> silhouetteVertices =
            BuildRectQuadVertices({static_cast<float>(pad), static_cast<float>(pad)}, {spreadWidth, spreadHeight},
                                 shadow.Radii, {1.0f, 1.0f, 1.0f, 1.0f});
        constexpr std::array<std::uint32_t, 6> silhouetteIndices{0, 1, 2, 0, 2, 3};
        wgpuQueueWriteBuffer(_queue, _shadowQuadVertexBuffer, 0, silhouetteVertices.data(),
                            silhouetteVertices.size() * sizeof(RectVertex));
        wgpuQueueWriteBuffer(_queue, _shadowQuadIndexBuffer, 0, silhouetteIndices.data(),
                            silhouetteIndices.size() * sizeof(std::uint32_t));

        WGPUCommandEncoderDescriptor encDesc{};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

        WGPURenderPassColorAttachment colorAttachment{};
        colorAttachment.view       = _shadowSilhouetteView;
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.loadOp      = WGPULoadOp_Clear;
        colorAttachment.storeOp     = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};

        WGPURenderPassDescriptor passDesc{};
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments      = &colorAttachment;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
        wgpuRenderPassEncoderSetViewport(pass, 0.0f, 0.0f, static_cast<float>(texWidth), static_cast<float>(texHeight),
                                         0.0f, 1.0f);
        wgpuRenderPassEncoderSetScissorRect(pass, 0, 0, texWidth, texHeight);

        wgpuRenderPassEncoderSetPipeline(pass, _rectPipeline);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, _bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, _shadowQuadVertexBuffer, 0,
                                             silhouetteVertices.size() * sizeof(RectVertex));
        wgpuRenderPassEncoderSetIndexBuffer(pass, _shadowQuadIndexBuffer, WGPUIndexFormat_Uint32, 0,
                                            silhouetteIndices.size() * sizeof(std::uint32_t));
        wgpuRenderPassEncoderDrawIndexed(pass, static_cast<uint32_t>(silhouetteIndices.size()), 1, 0, 0, 0);

        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);

        WGPUCommandBufferDescriptor cbDesc{};
        WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuCommandEncoderRelease(encoder);
        wgpuQueueSubmit(_queue, 1, &cmdBuf);
        wgpuCommandBufferRelease(cmdBuf);
    }

    const BlurResult blurred =
        _blurPass.Apply(BlurResult{_shadowSilhouetteTexture, _shadowSilhouetteView}, texWidth, texHeight,
                        shadow.BlurRadius);

    // ─── Composite: begin a new main pass, restore its own PerFrame content, draw ───────────────
    BeginMainPass();

    const PerFrameUniform mainPerFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight), 0.0f,
                                       0.0f};
    wgpuQueueWriteBuffer(_queue, _perFrameBuffer, 0, &mainPerFrame, sizeof(mainPerFrame));

    // Top-left of the padded silhouette texture, in the shape's own coordinate space, plus the
    // shadow's drop offset -- matches NativeRendererVulkan's/NativeRendererDX12's own identical
    // placement formula.
    const Widgets::Vec2 compositePosition{
        shadow.Position.x - shadow.Spread - pad + shadow.Offset.x,
        shadow.Position.y - shadow.Spread - pad + shadow.Offset.y,
    };
    const Widgets::Vec2 compositeSize{static_cast<float>(texWidth), static_cast<float>(texHeight)};
    const std::vector<ImageVertex> compositeVertices =
        BuildImageQuadVertices(compositePosition, compositeSize, shadow.ShadowColor);
    constexpr std::array<std::uint32_t, 6> compositeIndices{0, 1, 2, 0, 2, 3};
    wgpuQueueWriteBuffer(_queue, _shadowQuadVertexBuffer, 0, compositeVertices.data(),
                        compositeVertices.size() * sizeof(ImageVertex));
    wgpuQueueWriteBuffer(_queue, _shadowQuadIndexBuffer, 0, compositeIndices.data(),
                        compositeIndices.size() * sizeof(std::uint32_t));

    std::array<WGPUBindGroupEntry, 3> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer   = _perFrameBuffer;
    bgEntries[0].offset   = 0;
    bgEntries[0].size     = sizeof(PerFrameUniform);
    bgEntries[1].binding    = 1;
    bgEntries[1].textureView = blurred.View;
    bgEntries[2].binding = 2;
    bgEntries[2].sampler  = _linearSampler;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _imageBindGroupLayout;
    bgDesc.entryCount = static_cast<size_t>(bgEntries.size());
    bgDesc.entries     = bgEntries.data();
    WGPUBindGroup compositeBindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    wgpuRenderPassEncoderSetPipeline(_mainPass, _imagePipeline);
    wgpuRenderPassEncoderSetBindGroup(_mainPass, 0, compositeBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(_mainPass, 0, _shadowQuadVertexBuffer, 0,
                                         compositeVertices.size() * sizeof(ImageVertex));
    wgpuRenderPassEncoderSetIndexBuffer(_mainPass, _shadowQuadIndexBuffer, WGPUIndexFormat_Uint32, 0,
                                        compositeIndices.size() * sizeof(std::uint32_t));
    wgpuRenderPassEncoderDrawIndexed(_mainPass, static_cast<uint32_t>(compositeIndices.size()), 1, 0, 0, 0);

    wgpuBindGroupRelease(compositeBindGroup);
}

void NativeRendererWebGPU::HandleLayerMarker(const Batch& batch) {
    const auto& markers = std::get<std::vector<LayerVertex>>(batch.Vertices);
    if (markers.empty()) { return; }
    const LayerVertex& marker = markers.front();

    switch (marker.Op) {
        case LayerOp::PushOpacity:
        case LayerOp::PushBlend:
            PushLayer(marker.Op, marker.Opacity, marker.Mode);
            break;
        case LayerOp::Pop:
            PopLayer();
            break;
    }
}

void NativeRendererWebGPU::PushLayer(LayerOp op, float opacity, Rendering::BlendMode mode) {
    const std::size_t depth = _layerStack.size();

    LayerFrame frame;
    frame.Op            = op;
    frame.Opacity       = opacity;
    frame.Mode          = mode;
    frame.ParentTexture = _currentTargetTexture;
    frame.ParentView    = _currentTargetView;
    frame.TargetIndex   = depth;
    _layerStack.push_back(frame);

    EnsureLayerTarget(depth, _targetWidth, _targetHeight);
    const LayerTarget& target = _layerTargets[depth];

    // Switch rendering into this layer's own (cleared) target within the SAME, still-open
    // _mainEncoder -- no submission needed here, since nothing writes a shared buffer at this
    // point (see this class's own file comment on why only the composite draw needs isolation).
    _currentTargetTexture = target.Texture;
    SwitchMainPassTarget(target.View, WGPULoadOp_Clear);
}

void NativeRendererWebGPU::CompositeOpacityLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    // TintColor = (Opacity,Opacity,Opacity,Opacity) scales every channel of the already-
    // premultiplied source texture uniformly by Opacity -- matches NativeRendererVulkan's/
    // NativeRendererDX12's own identical reasoning.
    const std::vector<ImageVertex> vertices = BuildImageQuadVertices(
        {0.0f, 0.0f}, {static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)},
        {frame.Opacity, frame.Opacity, frame.Opacity, frame.Opacity});
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    wgpuQueueWriteBuffer(_queue, _shadowQuadVertexBuffer, 0, vertices.data(), vertices.size() * sizeof(ImageVertex));
    wgpuQueueWriteBuffer(_queue, _shadowQuadIndexBuffer, 0, indices.data(), indices.size() * sizeof(std::uint32_t));

    std::array<WGPUBindGroupEntry, 3> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer   = _perFrameBuffer;
    bgEntries[0].offset   = 0;
    bgEntries[0].size     = sizeof(PerFrameUniform);
    bgEntries[1].binding    = 1;
    bgEntries[1].textureView = target.View;
    bgEntries[2].binding = 2;
    bgEntries[2].sampler  = _linearSampler;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _imageBindGroupLayout;
    bgDesc.entryCount = static_cast<size_t>(bgEntries.size());
    bgDesc.entries     = bgEntries.data();
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    wgpuRenderPassEncoderSetPipeline(_mainPass, _premultipliedImagePipeline);
    wgpuRenderPassEncoderSetBindGroup(_mainPass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(_mainPass, 0, _shadowQuadVertexBuffer, 0, vertices.size() * sizeof(ImageVertex));
    wgpuRenderPassEncoderSetIndexBuffer(_mainPass, _shadowQuadIndexBuffer, WGPUIndexFormat_Uint32, 0,
                                        indices.size() * sizeof(std::uint32_t));
    wgpuRenderPassEncoderDrawIndexed(_mainPass, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    wgpuBindGroupRelease(bindGroup);
}

void NativeRendererWebGPU::CopyBackdropForBlend(const LayerFrame& frame) {
    EnsureBackdropTarget(_targetWidth, _targetHeight);

    WGPUTexelCopyTextureInfo src{};
    src.texture = frame.ParentTexture;
    src.aspect   = WGPUTextureAspect_All;

    WGPUTexelCopyTextureInfo dst{};
    dst.texture = _backdropTexture;
    dst.aspect   = WGPUTextureAspect_All;

    const WGPUExtent3D copySize{_targetWidth, _targetHeight, 1};

    WGPUCommandEncoderDescriptor encDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);
    wgpuCommandEncoderCopyTextureToTexture(encoder, &src, &dst, &copySize);

    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(_queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);
    // No fence/wait -- WebGPU's sequential submit model orders this after the parent's own
    // already-submitted content and before the composite draw's own later submit.
}

void NativeRendererWebGPU::CompositeBlendLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    const BlendParamsUniform params{static_cast<std::int32_t>(frame.Mode), 0, 0, 0};
    wgpuQueueWriteBuffer(_queue, _blendModeBuffer, 0, &params, sizeof(params));

    std::array<WGPUBindGroupEntry, 4> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer   = _blendModeBuffer;
    bgEntries[0].offset   = 0;
    bgEntries[0].size     = sizeof(BlendParamsUniform);
    bgEntries[1].binding    = 1;
    bgEntries[1].textureView = target.View;
    bgEntries[2].binding    = 2;
    bgEntries[2].textureView = _backdropView;
    bgEntries[3].binding = 3;
    bgEntries[3].sampler  = _linearSampler;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _blendBindGroupLayout;
    bgDesc.entryCount = static_cast<size_t>(bgEntries.size());
    bgDesc.entries     = bgEntries.data();
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    wgpuRenderPassEncoderSetPipeline(_mainPass, _blendPipeline);
    wgpuRenderPassEncoderSetBindGroup(_mainPass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(_mainPass, 6, 1, 0, 0); // no vertex buffer -- kBlendWgsl generates the quad

    wgpuBindGroupRelease(bindGroup);
}

void NativeRendererWebGPU::PopLayer() {
    // A PopLayer with no matching Push{Opacity,Blend}Layer is a malformed Rendering::CommandBuffer
    // -- a caller bug, not a runtime condition this internal renderer recovers from.
    IMF_ASSERT(!_layerStack.empty());

    const LayerFrame frame = _layerStack.back();
    _layerStack.pop_back();

    // Submit everything accumulated so far (the layer's own content, and anything recorded before
    // the push -- including, possibly, a Shadow batch's own not-yet-submitted composite draw, which
    // reads the same shared _shadowQuadVertexBuffer/_shadowQuadIndexBuffer the composite below is
    // about to overwrite) BEFORE writing any shared buffer, then isolate the composite draw itself
    // via its own submission afterward too -- see this class's own file comment.
    EndAndSubmitMainPass();

    _currentTargetTexture = frame.ParentTexture;
    _currentTargetView    = frame.ParentView;

    // Its own tiny encoder/submit -- an encoder-level copy, needing no pass open.
    if (frame.Op == LayerOp::PushBlend) { CopyBackdropForBlend(frame); }

    BeginMainPass(WGPULoadOp_Load);

    if (frame.Op == LayerOp::PushOpacity) {
        CompositeOpacityLayer(frame);
    } else {
        CompositeBlendLayer(frame);
    }

    EndAndSubmitMainPass();
    BeginMainPass(WGPULoadOp_Load); // ready for whatever batches follow, still targeting the parent
}

void NativeRendererWebGPU::EnsureBackdropBlurCopyTarget(std::uint32_t width, std::uint32_t height) {
    if (_backdropBlurCopyTexture && _backdropBlurCopyWidth == width && _backdropBlurCopyHeight == height) {
        return;
    }

    if (_backdropBlurCopyView) { wgpuTextureViewRelease(_backdropBlurCopyView); _backdropBlurCopyView = nullptr; }
    if (_backdropBlurCopyTexture) {
        wgpuTextureRelease(_backdropBlurCopyTexture);
        _backdropBlurCopyTexture = nullptr;
    }

    WGPUTextureDescriptor texDesc{};
    // StorageBinding (_blurPass's own source) + CopyDst (the region copy) -- no transitions. Format
    // must be _colorFormat (a texture-to-texture copy requires matching formats), which in turn
    // must be RGBA8Unorm for BlurPassWebGPU's own storage binding.
    texDesc.usage         = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopyDst;
    texDesc.dimension     = WGPUTextureDimension_2D;
    texDesc.size          = WGPUExtent3D{width, height, 1};
    texDesc.format        = _colorFormat;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount   = 1;
    _backdropBlurCopyTexture = wgpuDeviceCreateTexture(_device, &texDesc);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = _colorFormat;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    _backdropBlurCopyView     = wgpuTextureCreateView(_backdropBlurCopyTexture, &viewDesc);

    _backdropBlurCopyWidth  = width;
    _backdropBlurCopyHeight = height;
}

void NativeRendererWebGPU::RenderBackdropBlurBatch(const Batch& batch) {
    const auto& blurs = std::get<std::vector<BackdropBlurVertex>>(batch.Vertices);
    if (blurs.empty()) { return; }
    const BackdropBlurVertex& blur = blurs.front();
    if (blur.Size.x <= 0.0f || blur.Size.y <= 0.0f) { return; }

    if (_backdropBlurCountThisFrame >= _maxBackdropBlurPerFrame) {
        return; // matches NativeRendererVulkan's/NativeRendererDX12's own identical rate limit
    }
    ++_backdropBlurCountThisFrame;

    // Pad the copied region by the blur radius on every side (clamped to the target's own bounds)
    // so _blurPass's kernel has real surrounding content to read, then crop the composite back
    // down to exactly blur.Position/blur.Size -- the same formula as Vulkan/DX12.
    const float pad    = std::max(blur.BlurRadius, 0.0f);
    const float left   = std::max(blur.Position.x - pad, 0.0f);
    const float top    = std::max(blur.Position.y - pad, 0.0f);
    const float right  = std::min(blur.Position.x + blur.Size.x + pad, static_cast<float>(_targetWidth));
    const float bottom = std::min(blur.Position.y + blur.Size.y + pad, static_cast<float>(_targetHeight));
    if (right - left <= 0.0f || bottom - top <= 0.0f) { return; }

    const auto originX = static_cast<std::uint32_t>(left);
    const auto originY = static_cast<std::uint32_t>(top);
    // Clamp the rounded-up extent so origin + extent never exceeds the target -- a copy region
    // past the texture's own bounds is a WebGPU validation error, not a silent clamp.
    const auto copyWidth = std::min(static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(right - left)))),
                                    _targetWidth - originX);
    const auto copyHeight = std::min(static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(bottom - top)))),
                                     _targetHeight - originY);

    // Submit everything recorded so far -- the region copy below needs this Render() call's own
    // prior batches actually rendered, and the composite below rewrites the shared quad buffers
    // (see this class's own file comment on that invariant).
    EndAndSubmitMainPass();

    EnsureBackdropBlurCopyTarget(copyWidth, copyHeight);

    {
        WGPUTexelCopyTextureInfo src{};
        src.texture = _currentTargetTexture;
        src.origin   = WGPUOrigin3D{originX, originY, 0};
        src.aspect   = WGPUTextureAspect_All;

        WGPUTexelCopyTextureInfo dst{};
        dst.texture = _backdropBlurCopyTexture;
        dst.aspect   = WGPUTextureAspect_All;

        const WGPUExtent3D copySize{copyWidth, copyHeight, 1};

        WGPUCommandEncoderDescriptor encDesc{};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);
        wgpuCommandEncoderCopyTextureToTexture(encoder, &src, &dst, &copySize);

        WGPUCommandBufferDescriptor cbDesc{};
        WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuCommandEncoderRelease(encoder);
        wgpuQueueSubmit(_queue, 1, &cmdBuf);
        wgpuCommandBufferRelease(cmdBuf);
    }

    const BlurResult blurred = _blurPass.Apply(BlurResult{_backdropBlurCopyTexture, _backdropBlurCopyView}, copyWidth,
                                               copyHeight, blur.BlurRadius);

    BeginMainPass(WGPULoadOp_Load);

    // _perFrameBuffer already holds the real target's own size here -- only RenderShadowBatch()
    // ever rewrites it, and it restores the target size before returning.
    const Widgets::Vec2 uvMin{(blur.Position.x - static_cast<float>(originX)) / static_cast<float>(copyWidth),
                              (blur.Position.y - static_cast<float>(originY)) / static_cast<float>(copyHeight)};
    const Widgets::Vec2 uvMax{(blur.Position.x + blur.Size.x - static_cast<float>(originX)) / static_cast<float>(copyWidth),
                              (blur.Position.y + blur.Size.y - static_cast<float>(originY)) / static_cast<float>(copyHeight)};
    const std::vector<ImageVertex> vertices =
        BuildImageQuadVertices(blur.Position, blur.Size, blur.TintColor, uvMin, uvMax, blur.Radii);
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    wgpuQueueWriteBuffer(_queue, _shadowQuadVertexBuffer, 0, vertices.data(), vertices.size() * sizeof(ImageVertex));
    wgpuQueueWriteBuffer(_queue, _shadowQuadIndexBuffer, 0, indices.data(), indices.size() * sizeof(std::uint32_t));

    std::array<WGPUBindGroupEntry, 3> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer   = _perFrameBuffer;
    bgEntries[0].offset   = 0;
    bgEntries[0].size     = sizeof(PerFrameUniform);
    bgEntries[1].binding    = 1;
    bgEntries[1].textureView = blurred.View;
    bgEntries[2].binding = 2;
    bgEntries[2].sampler  = _linearSampler;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout      = _imageBindGroupLayout;
    bgDesc.entryCount = static_cast<size_t>(bgEntries.size());
    bgDesc.entries     = bgEntries.data();
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

    // Standard (non-premultiplied) blend -- matches RenderShadowBatch()'s and Vulkan's/DX12's own
    // identical choice (this content isn't scaled by any additional factor).
    wgpuRenderPassEncoderSetPipeline(_mainPass, _imagePipeline);
    wgpuRenderPassEncoderSetBindGroup(_mainPass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(_mainPass, 0, _shadowQuadVertexBuffer, 0, vertices.size() * sizeof(ImageVertex));
    wgpuRenderPassEncoderSetIndexBuffer(_mainPass, _shadowQuadIndexBuffer, WGPUIndexFormat_Uint32, 0,
                                        indices.size() * sizeof(std::uint32_t));
    wgpuRenderPassEncoderDrawIndexed(_mainPass, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    wgpuBindGroupRelease(bindGroup);
}

void NativeRendererWebGPU::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    // A missing SetTarget() call is a caller bug, not a runtime condition to recover from --
    // matches NativeRendererVulkan/NativeRendererDX12's own equivalent contract.
    IMF_ASSERT(_targetView != nullptr);

    // "Per frame" == "per Render() call" -- matches NativeRendererVulkan's/NativeRendererDX12's own
    // identical reset point.
    _backdropBlurCountThisFrame = 0;

    _batchBuilder.Build(buffer);
    const std::vector<Batch>& batches = _batchBuilder.Batches();

    std::size_t totalVertexBytes = 0;
    std::size_t totalIndexBytes  = 0;
    std::size_t shadowBatchCount       = 0;
    std::size_t layerBatchCount        = 0;
    std::size_t backdropBlurBatchCount = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) {
            totalVertexBytes += std::get<std::vector<RectVertex>>(batch.Vertices).size() * sizeof(RectVertex);
            totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
        } else if (batch.Kind == BatchKind::Image) {
            totalVertexBytes += std::get<std::vector<ImageVertex>>(batch.Vertices).size() * sizeof(ImageVertex);
            totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
        } else if (batch.Kind == BatchKind::Shadow) {
            ++shadowBatchCount; // uses its own dedicated buffers, not totalVertexBytes/totalIndexBytes
        } else if (batch.Kind == BatchKind::Layer) {
            ++layerBatchCount; // push/pop markers only -- no vertex/index data of their own either
        } else if (batch.Kind == BatchKind::BackdropBlur) {
            ++backdropBlurBatchCount; // uses the dedicated quad buffers too
        }
        // BatchKind::Text is out of scope, matching NativeRendererVulkan's/NativeRendererDX12's own
        // identical Phase 35.6/35.14 end state.
    }
    if (totalVertexBytes == 0 && shadowBatchCount == 0 && layerBatchCount == 0 && backdropBlurBatchCount == 0) {
        return;
    }

    _currentTargetTexture = _targetTexture;
    _currentTargetView    = _targetView;

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    const PerFrameUniform perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight), 0.0f, 0.0f};
    wgpuQueueWriteBuffer(_queue, _perFrameBuffer, 0, &perFrame, sizeof(perFrame));

    BeginMainPass();

    std::size_t vertexByteOffset = 0;
    std::size_t indexByteOffset  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) {
            RenderRectBatch(_mainPass, batch, vertexByteOffset, indexByteOffset);
        } else if (batch.Kind == BatchKind::Image) {
            RenderImageBatch(_mainPass, batch, vertexByteOffset, indexByteOffset);
        } else if (batch.Kind == BatchKind::Shadow) {
            // Ends and re-begins the main pass internally -- see this method's own comment on why
            // (BlurPassWebGPU::Apply() is its own separate submission).
            RenderShadowBatch(batch);
        } else if (batch.Kind == BatchKind::Layer) {
            HandleLayerMarker(batch);
        } else if (batch.Kind == BatchKind::BackdropBlur) {
            // Ends and re-begins the main pass internally, same reason as RenderShadowBatch().
            RenderBackdropBlurBatch(batch);
        }
    }

    EndAndSubmitMainPass();
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

    if (_linearSampler) { wgpuSamplerRelease(_linearSampler); _linearSampler = nullptr; }
    if (_imagePipeline) { wgpuRenderPipelineRelease(_imagePipeline); _imagePipeline = nullptr; }
    if (_imagePipelineLayout) { wgpuPipelineLayoutRelease(_imagePipelineLayout); _imagePipelineLayout = nullptr; }
    if (_imageBindGroupLayout) {
        wgpuBindGroupLayoutRelease(_imageBindGroupLayout);
        _imageBindGroupLayout = nullptr;
    }
    if (_imageShaderModule) { wgpuShaderModuleRelease(_imageShaderModule); _imageShaderModule = nullptr; }

    if (_shadowQuadVertexBuffer) { wgpuBufferRelease(_shadowQuadVertexBuffer); _shadowQuadVertexBuffer = nullptr; }
    if (_shadowQuadIndexBuffer) { wgpuBufferRelease(_shadowQuadIndexBuffer); _shadowQuadIndexBuffer = nullptr; }
    if (_shadowSilhouetteView) { wgpuTextureViewRelease(_shadowSilhouetteView); _shadowSilhouetteView = nullptr; }
    if (_shadowSilhouetteTexture) {
        wgpuTextureRelease(_shadowSilhouetteTexture);
        _shadowSilhouetteTexture = nullptr;
    }
    _shadowSilhouetteWidth  = 0;
    _shadowSilhouetteHeight = 0;
    _blurPass.Shutdown();

    _layerStack.clear();
    for (LayerTarget& target : _layerTargets) {
        if (target.View) { wgpuTextureViewRelease(target.View); }
        if (target.Texture) { wgpuTextureRelease(target.Texture); }
    }
    _layerTargets.clear();
    if (_backdropView) { wgpuTextureViewRelease(_backdropView); _backdropView = nullptr; }
    if (_backdropTexture) { wgpuTextureRelease(_backdropTexture); _backdropTexture = nullptr; }
    _backdropWidth  = 0;
    _backdropHeight = 0;

    if (_backdropBlurCopyView) { wgpuTextureViewRelease(_backdropBlurCopyView); _backdropBlurCopyView = nullptr; }
    if (_backdropBlurCopyTexture) {
        wgpuTextureRelease(_backdropBlurCopyTexture);
        _backdropBlurCopyTexture = nullptr;
    }
    _backdropBlurCopyWidth      = 0;
    _backdropBlurCopyHeight     = 0;
    _backdropBlurCountThisFrame = 0;

    if (_blendModeBuffer) { wgpuBufferRelease(_blendModeBuffer); _blendModeBuffer = nullptr; }
    if (_blendPipeline) { wgpuRenderPipelineRelease(_blendPipeline); _blendPipeline = nullptr; }
    if (_blendPipelineLayout) { wgpuPipelineLayoutRelease(_blendPipelineLayout); _blendPipelineLayout = nullptr; }
    if (_blendBindGroupLayout) {
        wgpuBindGroupLayoutRelease(_blendBindGroupLayout);
        _blendBindGroupLayout = nullptr;
    }
    if (_blendShaderModule) { wgpuShaderModuleRelease(_blendShaderModule); _blendShaderModule = nullptr; }
    if (_premultipliedImagePipeline) {
        wgpuRenderPipelineRelease(_premultipliedImagePipeline);
        _premultipliedImagePipeline = nullptr;
    }

    _initialized = false;
}

} // namespace ImFrame::Internal
