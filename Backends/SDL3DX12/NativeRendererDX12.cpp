/**
 * @file     NativeRendererDX12.cpp
 * @brief    `NativeRendererDX12` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "NativeRendererDX12.hpp"

#include "ImFrame/Core/Error.hpp"

#include "Shaders/Blend.Pixel.hpp"
#include "Shaders/Blend.Vertex.hpp"
#include "Shaders/Image.Pixel.hpp"
#include "Shaders/Image.Vertex.hpp"
#include "Shaders/SDFRect.Pixel.hpp"
#include "Shaders/SDFRect.Vertex.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace ImFrame::Internal {

namespace {

/// Rounds `size` up to the next multiple of `alignment` (a power of two) — D3D12 upload-heap
/// buffers backing a root CBV don't strictly need 256-byte-aligned *sizes* (only descriptor-based
/// CBVs do), but committed resources are always at least this well aligned regardless, so this
/// just documents the guarantee rather than working around a real constraint.
constexpr std::size_t kConstantBufferAlignment = 256;

[[nodiscard]] std::size_t AlignUp(std::size_t size, std::size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/// Creates an `D3D12_HEAP_TYPE_UPLOAD` buffer of exactly `sizeBytes`, persistently mapped —
/// the D3D12 analogue of `NativeRendererVulkan`'s own VMA `HOST_ACCESS_SEQUENTIAL_WRITE` buffers.
ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device4* device, std::size_t sizeBytes, void** outMapped) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width            = static_cast<UINT64>(sizeBytes);
    desc.Height           = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&resource));

    const D3D12_RANGE noRead{0, 0}; // never read back through this mapping
    resource->Map(0, &noRead, outMapped);
    return resource;
}

/// Builds one axis-aligned quad's `RectVertex`es — `RenderShadowBatch()`'s own silhouette pass
/// reuses the existing Rect pipeline via a direct draw call rather than a third hand-written
/// pipeline, mirroring `NativeRendererVulkan::BuildRectQuadVertices()`'s identical role/formula.
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

/// Builds one axis-aligned quad's `ImageVertex`es — `RenderShadowBatch()`'s/`CompositeOpacityLayer()`'s/
/// `RenderBackdropBlurBatch()`'s own composite passes all reuse the existing Image pipeline to draw a
/// renderer-produced offscreen texture (a blurred shadow, a captured layer, a blurred backdrop
/// region), tinted, as a plain rectangle. Mirrors `NativeRendererVulkan::BuildImageQuadVertices()`'s
/// role and full signature (Phase 35.6 added the optional `uvMin`/`uvMax`/`radii` parameters there,
/// needed by `RenderBackdropBlurBatch()`'s own crop-to-requested-rect step), including its
/// **unflipped** default UV table -- not because this backend's own vertex shader matches Vulkan's
/// (it doesn't; `Image.hlsl`'s `VSMain` negates Y like GL, Phase 35.9), but because a D3D12 texture's
/// row 0 is its top row (the same top-down memory convention Vulkan's own images use), independent of
/// whichever way a vertex shader happens to negate Y for clip-space purposes -- the two questions
/// (screen-position handedness vs. texture-row order) are unrelated, and this backend's own DX12
/// texture-upload convention (already exercised correctly by every `BatchKind::Image` test, Phase
/// 35.9) is top-down like Vulkan's, not bottom-up like GL's. A plain, unflipped default mapping is
/// therefore the correct choice here too.
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

NativeRendererDX12::NativeRendererDX12(ID3D12Device4* device, ID3D12CommandQueue* directQueue,
                                       DXGI_FORMAT colorFormat)
    : _device(device), _directQueue(directQueue), _colorFormat(colorFormat), _blurPass(device, directQueue) {}

NativeRendererDX12::~NativeRendererDX12() { Shutdown(); }

void NativeRendererDX12::SetTarget(ID3D12Resource* targetResource, D3D12_CPU_DESCRIPTOR_HANDLE targetRtv,
                                  std::uint32_t width, std::uint32_t height) noexcept {
    _targetResource = targetResource;
    _targetRtv      = targetRtv;
    _targetWidth    = width;
    _targetHeight   = height;
}

Result<Rendering::FontId> NativeRendererDX12::LoadFont(const Utility::Path& /*path*/, float /*sizePixels*/) {
    // No text pipeline exists yet -- this sub-phase's scope is BatchKind::Rect only, matching
    // NativeRendererVulkan's own identical "no attached text renderer" fallback (Phase 35.1).
    return std::unexpected(Error::FontLoadFailed);
}

void NativeRendererDX12::EnsureInitialized() {
    if (_initialized) { return; }

    // ─── Root signature: one root CBV (SDFRect.hlsl's PerFrame, vertex-stage only) ─────────────
    D3D12_ROOT_PARAMETER1 rootParam{};
    rootParam.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace  = 0;
    rootParam.Descriptor.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParam.ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Version                = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = 1;
    rootSigDesc.Desc_1_1.pParameters   = &rootParam;
    rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&rootSigDesc, &serialized, &error);
    _device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                 IID_PPV_ARGS(&_rootSignature));

    // ─── Rect pipeline state: SDFRect.hlsl's VSMain/PSMain, matching RectVertex's field layout ──
    const std::array<D3D12_INPUT_ELEMENT_DESC, 7> inputElements{{
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, Position)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"LOCAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, Local)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"HALFSIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, HalfSize)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"RADII", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, Radii)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"FILLCOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, FillColor)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"STROKECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, StrokeColor)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"STROKEWIDTH", 0, DXGI_FORMAT_R32_FLOAT, 0, static_cast<UINT>(offsetof(RectVertex, StrokeWidth)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    }};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout.pInputElementDescs = inputElements.data();
    psoDesc.InputLayout.NumElements        = static_cast<UINT>(inputElements.size());
    psoDesc.pRootSignature = _rootSignature.Get();
    psoDesc.VS = {Shaders::kSDFRectVertexDxil, Shaders::kSDFRectVertexDxilByteCount};
    psoDesc.PS = {Shaders::kSDFRectPixelDxil, Shaders::kSDFRectPixelDxilByteCount};

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Standard (non-premultiplied-alpha) "over" blend -- matches NativeRendererVulkan's own
    // default (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), for cross-backend visual consistency.
    D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
    blendDesc.BlendEnable           = TRUE;
    blendDesc.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blendDesc.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.BlendOp               = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha         = D3D12_BLEND_SRC_ALPHA;
    blendDesc.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0] = blendDesc;

    psoDesc.DepthStencilState.DepthEnable   = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask                      = UINT_MAX;
    psoDesc.PrimitiveTopologyType            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                = 1;
    psoDesc.RTVFormats[0]                    = _colorFormat;
    psoDesc.SampleDesc.Count                 = 1;

    _device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_rectPipelineState));

    // ─── Image root signature: root CBV (b0) + one-SRV descriptor table (t0) + static sampler (s0) ─
    D3D12_DESCRIPTOR_RANGE1 srvRange{};
    srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors     = 1;
    srvRange.BaseShaderRegister = 0;

    std::array<D3D12_ROOT_PARAMETER1, 2> imageRootParams{};
    imageRootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    imageRootParams[0].Descriptor.ShaderRegister = 0;
    imageRootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;
    imageRootParams[1].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    imageRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    imageRootParams[1].DescriptorTable.pDescriptorRanges    = &srvRange;
    imageRootParams[1].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

    // The D3D12 analogue of NativeRendererVulkan's own one shared VkSampler (Phase 35.2) -- a
    // static sampler needs no descriptor heap slot at all, unlike a real sampler descriptor would.
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ShaderRegister   = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC imageRootSigDesc{};
    imageRootSigDesc.Version                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    imageRootSigDesc.Desc_1_1.NumParameters      = static_cast<UINT>(imageRootParams.size());
    imageRootSigDesc.Desc_1_1.pParameters        = imageRootParams.data();
    imageRootSigDesc.Desc_1_1.NumStaticSamplers = 1;
    imageRootSigDesc.Desc_1_1.pStaticSamplers    = &staticSampler;
    imageRootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> imageSerialized;
    ComPtr<ID3DBlob> imageError;
    D3D12SerializeVersionedRootSignature(&imageRootSigDesc, &imageSerialized, &imageError);
    _device->CreateRootSignature(0, imageSerialized->GetBufferPointer(), imageSerialized->GetBufferSize(),
                                 IID_PPV_ARGS(&_imageRootSignature));

    // ─── Image pipeline state: Image.hlsl's VSMain/PSMain, matching ImageVertex's field layout ───
    const std::array<D3D12_INPUT_ELEMENT_DESC, 6> imageInputElements{{
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, Position)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"LOCAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, Local)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"HALFSIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, HalfSize)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"RADII", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, Radii)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, Uv)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TINTCOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(ImageVertex, TintColor)),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    }};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC imagePsoDesc{};
    imagePsoDesc.InputLayout.pInputElementDescs = imageInputElements.data();
    imagePsoDesc.InputLayout.NumElements        = static_cast<UINT>(imageInputElements.size());
    imagePsoDesc.pRootSignature = _imageRootSignature.Get();
    imagePsoDesc.VS = {Shaders::kImageVertexDxil, Shaders::kImageVertexDxilByteCount};
    imagePsoDesc.PS = {Shaders::kImagePixelDxil, Shaders::kImagePixelDxilByteCount};

    imagePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    imagePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    imagePsoDesc.BlendState.RenderTarget[0] = blendDesc;

    imagePsoDesc.DepthStencilState.DepthEnable   = FALSE;
    imagePsoDesc.DepthStencilState.StencilEnable = FALSE;
    imagePsoDesc.SampleMask                      = UINT_MAX;
    imagePsoDesc.PrimitiveTopologyType            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    imagePsoDesc.NumRenderTargets                = 1;
    imagePsoDesc.RTVFormats[0]                    = _colorFormat;
    imagePsoDesc.SampleDesc.Count                 = 1;

    _device->CreateGraphicsPipelineState(&imagePsoDesc, IID_PPV_ARGS(&_imagePipelineState));

    // ─── Premultiplied Image PSO: same root signature/shaders as _imagePipelineState, only the
    // blend factors differ -- needed for CompositeOpacityLayer()'s own already-premultiplied
    // captured render scaled by Opacity, mirroring NativeRendererVulkan's own
    // _premultipliedImagePipeline (Phase 35.5) ────────────────────────────────────────────────
    D3D12_RENDER_TARGET_BLEND_DESC premultipliedBlendDesc{};
    premultipliedBlendDesc.BlendEnable           = TRUE;
    premultipliedBlendDesc.SrcBlend              = D3D12_BLEND_ONE;
    premultipliedBlendDesc.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    premultipliedBlendDesc.BlendOp               = D3D12_BLEND_OP_ADD;
    premultipliedBlendDesc.SrcBlendAlpha         = D3D12_BLEND_ONE;
    premultipliedBlendDesc.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    premultipliedBlendDesc.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    premultipliedBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC premultipliedPsoDesc = imagePsoDesc;
    premultipliedPsoDesc.BlendState.RenderTarget[0] = premultipliedBlendDesc;
    _device->CreateGraphicsPipelineState(&premultipliedPsoDesc, IID_PPV_ARGS(&_premultipliedImagePipelineState));

    // ─── Blend root signature: two-SRV descriptor table (t0/t1) + static sampler (s0) + one root
    // constant (Mode) -- no vertex input at all, Blend.hlsl generates its own fixed fullscreen
    // quad from SV_VertexID ─────────────────────────────────────────────────────────────────────
    D3D12_DESCRIPTOR_RANGE1 blendSrvRange{};
    blendSrvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    blendSrvRange.NumDescriptors     = 2;
    blendSrvRange.BaseShaderRegister = 0;

    std::array<D3D12_ROOT_PARAMETER1, 2> blendRootParams{};
    blendRootParams[0].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    blendRootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    blendRootParams[0].DescriptorTable.pDescriptorRanges    = &blendSrvRange;
    blendRootParams[0].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;
    blendRootParams[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    blendRootParams[1].Constants.ShaderRegister = 0;
    blendRootParams[1].Constants.Num32BitValues = 1; // Mode
    blendRootParams[1].ShaderVisibility           = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC blendStaticSampler{};
    blendStaticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    blendStaticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    blendStaticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    blendStaticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    blendStaticSampler.ShaderRegister   = 0;
    blendStaticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC blendRootSigDesc{};
    blendRootSigDesc.Version                     = D3D_ROOT_SIGNATURE_VERSION_1_1;
    blendRootSigDesc.Desc_1_1.NumParameters      = static_cast<UINT>(blendRootParams.size());
    blendRootSigDesc.Desc_1_1.pParameters        = blendRootParams.data();
    blendRootSigDesc.Desc_1_1.NumStaticSamplers = 1;
    blendRootSigDesc.Desc_1_1.pStaticSamplers    = &blendStaticSampler;
    // No D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT -- this pipeline has none.

    ComPtr<ID3DBlob> blendSerialized;
    ComPtr<ID3DBlob> blendError;
    D3D12SerializeVersionedRootSignature(&blendRootSigDesc, &blendSerialized, &blendError);
    _device->CreateRootSignature(0, blendSerialized->GetBufferPointer(), blendSerialized->GetBufferSize(),
                                 IID_PPV_ARGS(&_blendRootSignature));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC blendPsoDesc{};
    blendPsoDesc.pRootSignature = _blendRootSignature.Get();
    blendPsoDesc.VS = {Shaders::kBlendVertexDxil, Shaders::kBlendVertexDxilByteCount};
    blendPsoDesc.PS = {Shaders::kBlendPixelDxil, Shaders::kBlendPixelDxilByteCount};

    blendPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    blendPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // blendEnable=false -- the shader itself computes the full Porter-Duff-composited result;
    // fixed-function blending on top of that output would double-composite it (matches
    // NativeRendererVulkan's own _blendPipeline identical reasoning).
    D3D12_RENDER_TARGET_BLEND_DESC blendReplaceDesc{};
    blendReplaceDesc.BlendEnable           = FALSE;
    blendReplaceDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendPsoDesc.BlendState.RenderTarget[0] = blendReplaceDesc;

    blendPsoDesc.DepthStencilState.DepthEnable   = FALSE;
    blendPsoDesc.DepthStencilState.StencilEnable = FALSE;
    blendPsoDesc.SampleMask                      = UINT_MAX;
    blendPsoDesc.PrimitiveTopologyType            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    blendPsoDesc.NumRenderTargets                = 1;
    blendPsoDesc.RTVFormats[0]                    = _colorFormat;
    blendPsoDesc.SampleDesc.Count                 = 1;

    _device->CreateGraphicsPipelineState(&blendPsoDesc, IID_PPV_ARGS(&_blendPipelineState));

    _srvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ─── Command allocator/list, fence -- matches ViewportFramebufferDX12's own identical pattern ─
    _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
    _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                IID_PPV_ARGS(&_commandList));

    _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // ─── PerFrame constant buffer: vec2 ViewportSize ───────────────────────────────────────────
    _perFrameCb = CreateUploadBuffer(_device, AlignUp(sizeof(float) * 2, kConstantBufferAlignment),
                                     &_perFrameCbMapped);

    // ─── Phase 35.12: BatchKind::Shadow's own small, dedicated quad buffers ───────────────────
    // Sized for the larger of RectVertex/ImageVertex (4 vertices) -- reused sequentially (never
    // concurrently) by the silhouette draw (RectVertex) and the composite draw (ImageVertex),
    // mirroring NativeRendererVulkan's own _shadowQuadVertexBuffer/_shadowQuadIndexBuffer.
    const std::size_t shadowQuadVertexBytes = 4 * std::max(sizeof(RectVertex), sizeof(ImageVertex));
    _shadowQuadVertexBuffer = CreateUploadBuffer(_device, shadowQuadVertexBytes, &_shadowQuadVertexMapped);
    _shadowQuadIndexBuffer  = CreateUploadBuffer(_device, 6 * sizeof(std::uint32_t), &_shadowQuadIndexMapped);

    _initialized = true;
}

void NativeRendererDX12::EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes) {
    if (vertexBytes > _vertexBufferCapacityBytes) {
        _vertexBuffer = CreateUploadBuffer(_device, vertexBytes, &_vertexBufferMapped);
        _vertexBufferCapacityBytes = vertexBytes;
    }
    if (indexBytes > _indexBufferCapacityBytes) {
        _indexBuffer = CreateUploadBuffer(_device, indexBytes, &_indexBufferMapped);
        _indexBufferCapacityBytes = indexBytes;
    }
}

void NativeRendererDX12::EnsureImageDescriptorCapacity(std::size_t neededSlots) {
    if (neededSlots <= _srvHeapCapacitySlots) { return; }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = static_cast<UINT>(neededSlots);
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    _device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_srvHeap));

    _srvHeapCapacitySlots = neededSlots;
}

void NativeRendererDX12::EnsureShadowSilhouetteTarget(std::uint32_t width, std::uint32_t height) {
    if (_shadowSilhouetteWidth == width && _shadowSilhouetteHeight == height && _shadowSilhouetteResource) {
        return;
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    // Both roles this resource ever plays: the silhouette's own render target, and BlurPassDX12's
    // UAV source -- see this class's own file comment on why, unlike Vulkan's single GENERAL
    // layout, D3D12 still needs a real transition between the two (just not two separate resources).
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // Created directly in UNORDERED_ACCESS -- its "at rest" state between RenderShadowBatch() calls
    // (mirrors BlurPassDX12's own identical choice for its ping-pong targets, Phase 35.11).
    _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                     nullptr, IID_PPV_ARGS(&_shadowSilhouetteResource));

    if (!_shadowRtvHeap) {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        _device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_shadowRtvHeap));
        _shadowRtv = _shadowRtvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    _device->CreateRenderTargetView(_shadowSilhouetteResource.Get(), &rtvDesc, _shadowRtv);

    _shadowSilhouetteWidth  = width;
    _shadowSilhouetteHeight = height;
}

void NativeRendererDX12::EnsureLayerTarget(std::size_t depth, std::uint32_t width, std::uint32_t height) {
    if (_layerTargets.size() <= depth) { _layerTargets.resize(depth + 1); }

    LayerTarget& target = _layerTargets[depth];
    if (target.Resource && target.Width == width && target.Height == height) { return; }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // "At rest" state is PIXEL_SHADER_RESOURCE (see this class's own file comment) -- created
    // directly there so PushLayer()'s own PIXEL_SHADER_RESOURCE -> RENDER_TARGET transition is
    // always the correct one to record, whether this target is freshly created or reused.
    _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                     IID_PPV_ARGS(&target.Resource));

    if (!target.RtvHeap) {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        _device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&target.RtvHeap));
        target.Rtv = target.RtvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    _device->CreateRenderTargetView(target.Resource.Get(), &rtvDesc, target.Rtv);

    target.Width  = width;
    target.Height = height;
}

void NativeRendererDX12::EnsureBackdropTarget(std::uint32_t width, std::uint32_t height) {
    if (_backdropResource && _backdropWidth == width && _backdropHeight == height) { return; }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    // No special resource flags -- a plain texture is always valid as a copy destination and SRV
    // source (see this class's own file comment).

    _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                     IID_PPV_ARGS(&_backdropResource));

    _backdropWidth  = width;
    _backdropHeight = height;
}

void NativeRendererDX12::EnsureBackdropBlurCopyTarget(std::uint32_t width, std::uint32_t height) {
    if (_backdropBlurCopyResource && _backdropBlurCopyWidth == width && _backdropBlurCopyHeight == height) {
        return;
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // "At rest" state is UNORDERED_ACCESS (see this class's own file comment) -- created directly
    // there, mirroring BlurPassDX12's own ping-pong targets (Phase 35.11), since this resource is
    // never rendered into, only copied into and read/written via _blurPass's own UAV.
    _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                     IID_PPV_ARGS(&_backdropBlurCopyResource));

    _backdropBlurCopyWidth  = width;
    _backdropBlurCopyHeight = height;
}

void NativeRendererDX12::RenderBackdropBlurBatch(const Batch& batch) {
    const auto& blurs = std::get<std::vector<BackdropBlurVertex>>(batch.Vertices);
    if (blurs.empty()) { return; }
    const BackdropBlurVertex& blur = blurs.front();
    if (blur.Size.x <= 0.0f || blur.Size.y <= 0.0f) { return; }

    if (_backdropBlurCountThisFrame >= _maxBackdropBlurPerFrame) {
        return; // matches NativeRendererVulkan's/NativeRendererGL3's own identical rate-limit
    }
    ++_backdropBlurCountThisFrame;

    // Pad the copied region by the blur radius on every side (clamped to the target's own bounds)
    // so _blurPass's kernel has real surrounding content to read, the same reasoning as
    // RenderShadowBatch()'s silhouette padding -- then crop the composite back down to exactly
    // blur.Position/blur.Size, since (unlike a shadow, which is expected to bleed past its shape)
    // backdrop blur is documented as replacing exactly the requested rect, nothing more.
    const float pad    = std::max(blur.BlurRadius, 0.0f);
    const float left   = std::max(blur.Position.x - pad, 0.0f);
    const float top    = std::max(blur.Position.y - pad, 0.0f);
    const float right  = std::min(blur.Position.x + blur.Size.x + pad, static_cast<float>(_targetWidth));
    const float bottom = std::min(blur.Position.y + blur.Size.y + pad, static_cast<float>(_targetHeight));
    const float copyWidthF  = right - left;
    const float copyHeightF = bottom - top;
    if (copyWidthF <= 0.0f || copyHeightF <= 0.0f) { return; }

    const auto copyWidth  = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(copyWidthF))));
    const auto copyHeight = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(copyHeightF))));

    // Flush everything recorded so far -- the copy below needs this Render() call's own prior
    // batches' content actually on the GPU already (not merely recorded), and _blurPass.Apply() is
    // its own separate submission, exactly like RenderShadowBatch()'s identical first step.
    EndAndSubmitMainCommandList();

    EnsureBackdropBlurCopyTarget(copyWidth, copyHeight);

    // Copy [left,top]-[right,bottom] (already in this renderer's own top-down pixel space -- no
    // GL-style window-coordinate flip needed, matching NativeRendererVulkan's own identical finding)
    // from _targetResource into _backdropBlurCopyResource.
    {
        _commandAllocator->Reset();
        _commandList->Reset(_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toCopySrc{};
        toCopySrc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopySrc.Transition.pResource   = _targetResource;
        toCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toCopySrc.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopySrc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toCopySrc);

        D3D12_RESOURCE_BARRIER toCopyDst{};
        toCopyDst.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopyDst.Transition.pResource   = _backdropBlurCopyResource.Get();
        toCopyDst.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toCopyDst.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        toCopyDst.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toCopyDst);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource        = _targetResource;
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = _backdropBlurCopyResource.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;
        const D3D12_BOX srcBox{static_cast<UINT>(left), static_cast<UINT>(top), 0, static_cast<UINT>(left) + copyWidth,
                              static_cast<UINT>(top) + copyHeight, 1};
        _commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

        D3D12_RESOURCE_BARRIER backToRt{};
        backToRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backToRt.Transition.pResource   = _targetResource;
        backToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        backToRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        backToRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &backToRt);

        D3D12_RESOURCE_BARRIER backToUav{};
        backToUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backToUav.Transition.pResource   = _backdropBlurCopyResource.Get();
        backToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        backToUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        backToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &backToUav);

        _commandList->Close();
        ID3D12CommandList* lists[] = {_commandList.Get()};
        _directQueue->ExecuteCommandLists(1, lists);
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    const BlurResult blurred =
        _blurPass.Apply(BlurResult{_backdropBlurCopyResource.Get()}, copyWidth, copyHeight, blur.BlurRadius);

    // Transition the borrowed blurred resource UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE for our own
    // composite SRV, mirroring RenderShadowBatch()'s own identical transition for the same reason
    // (BlurPassDX12 expects its own owned targets back in UNORDERED_ACCESS by its next Apply() call).
    {
        _commandAllocator->Reset();
        _commandList->Reset(_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toSrv{};
        toSrv.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource   = blurred.Resource;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toSrv.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toSrv);

        _commandList->Close();
        ID3D12CommandList* lists[] = {_commandList.Get()};
        _directQueue->ExecuteCommandLists(1, lists);
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;
    _device->CreateShaderResourceView(blurred.Resource, &srvDesc, _compositeSrvCpuBase);

    // ─── Composite: resume the main command list, restore its own PerFrame CB, draw the cropped
    // sub-rectangle of the padded, blurred copy that corresponds to blur.Position/blur.Size ────────
    BeginMainCommandList();

    struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameCb mainPerFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameCbMapped, &mainPerFrame, sizeof(mainPerFrame));

    const Widgets::Vec2 uvMin{(blur.Position.x - left) / static_cast<float>(copyWidth),
                              (blur.Position.y - top) / static_cast<float>(copyHeight)};
    const Widgets::Vec2 uvMax{(blur.Position.x + blur.Size.x - left) / static_cast<float>(copyWidth),
                              (blur.Position.y + blur.Size.y - top) / static_cast<float>(copyHeight)};
    const std::vector<ImageVertex> vertices =
        BuildImageQuadVertices(blur.Position, blur.Size, blur.TintColor, uvMin, uvMax, blur.Radii);
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    std::memcpy(_shadowQuadVertexMapped, vertices.data(), vertices.size() * sizeof(ImageVertex));
    std::memcpy(_shadowQuadIndexMapped, indices.data(), indices.size() * sizeof(std::uint32_t));

    // Standard (non-premultiplied) blend, not _premultipliedImagePipelineState -- matches
    // RenderShadowBatch()'s own identical choice (this content isn't scaled by any additional
    // factor the way an opacity layer's captured render is).
    _commandList->SetPipelineState(_imagePipelineState.Get());
    _commandList->SetGraphicsRootSignature(_imageRootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(1, _compositeSrvGpuBase);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = _shadowQuadVertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes    = static_cast<UINT>(vertices.size() * sizeof(ImageVertex));
    vbView.StrideInBytes  = sizeof(ImageVertex);
    _commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = _shadowQuadIndexBuffer->GetGPUVirtualAddress();
    ibView.SizeInBytes    = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    ibView.Format         = DXGI_FORMAT_R32_UINT;
    _commandList->IASetIndexBuffer(&ibView);

    _commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

    // Restore BlurPassDX12's own "always UNORDERED_ACCESS at rest" invariant for its next Apply()
    // call, matching RenderShadowBatch()'s own identical restoration.
    D3D12_RESOURCE_BARRIER backToUav{};
    backToUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    backToUav.Transition.pResource   = blurred.Resource;
    backToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    backToUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    backToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &backToUav);
}

void NativeRendererDX12::HandleLayerMarker(const Batch& batch) {
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

void NativeRendererDX12::PushLayer(LayerOp op, float opacity, Rendering::BlendMode mode) {
    const std::size_t depth = _layerStack.size();

    LayerFrame frame;
    frame.Op      = op;
    frame.Opacity = opacity;
    frame.Mode    = mode;
    if (_layerStack.empty()) {
        frame.ParentResource = _targetResource;
        frame.ParentRtv      = _targetRtv;
    } else {
        const LayerTarget& enclosing = _layerTargets[_layerStack.back().TargetIndex];
        frame.ParentResource = enclosing.Resource.Get();
        frame.ParentRtv      = enclosing.Rtv;
    }
    frame.TargetIndex = depth;
    _layerStack.push_back(frame);

    EnsureLayerTarget(depth, _targetWidth, _targetHeight);
    const LayerTarget& target = _layerTargets[depth];

    D3D12_RESOURCE_BARRIER toRt{};
    toRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRt.Transition.pResource   = target.Resource.Get();
    toRt.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    toRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &toRt);

    _commandList->OMSetRenderTargets(1, &target.Rtv, FALSE, nullptr);
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    _commandList->ClearRenderTargetView(target.Rtv, clearColor, 0, nullptr);

    // Every layer, and the real target, is always _targetWidth x _targetHeight -- no separate
    // "current viewport" state to restore, matching NativeRendererVulkan's own identical note.
    const D3D12_VIEWPORT viewport{
        0.0f, 0.0f, static_cast<float>(_targetWidth), static_cast<float>(_targetHeight), 0.0f, 1.0f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(_targetWidth), static_cast<LONG>(_targetHeight)};
    _commandList->RSSetViewports(1, &viewport);
    _commandList->RSSetScissorRects(1, &scissor);
}

void NativeRendererDX12::CompositeOpacityLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;
    _device->CreateShaderResourceView(target.Resource.Get(), &srvDesc, _compositeSrvCpuBase);

    // TintColor = (Opacity,Opacity,Opacity,Opacity) scales every channel of the already-
    // premultiplied source texture uniformly by Opacity -- matches
    // NativeRendererVulkan::CompositeOpacityLayer()'s identical reasoning.
    const std::vector<ImageVertex> vertices = BuildImageQuadVertices(
        {0.0f, 0.0f}, {static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)},
        {frame.Opacity, frame.Opacity, frame.Opacity, frame.Opacity});
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    std::memcpy(_shadowQuadVertexMapped, vertices.data(), vertices.size() * sizeof(ImageVertex));
    std::memcpy(_shadowQuadIndexMapped, indices.data(), indices.size() * sizeof(std::uint32_t));

    _commandList->SetPipelineState(_premultipliedImagePipelineState.Get());
    _commandList->SetGraphicsRootSignature(_imageRootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(1, _compositeSrvGpuBase);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = _shadowQuadVertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes    = static_cast<UINT>(vertices.size() * sizeof(ImageVertex));
    vbView.StrideInBytes  = sizeof(ImageVertex);
    _commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = _shadowQuadIndexBuffer->GetGPUVirtualAddress();
    ibView.SizeInBytes    = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    ibView.Format         = DXGI_FORMAT_R32_UINT;
    _commandList->IASetIndexBuffer(&ibView);

    _commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void NativeRendererDX12::CopyBackdropForBlend(const LayerFrame& frame) {
    EnsureBackdropTarget(_targetWidth, _targetHeight);

    // The parent (real target or an enclosing layer) is always in RENDER_TARGET while active --
    // unlike Vulkan, which only needs a conditional transition for its own depth-0-vs-nested
    // layout distinction (this class's own file comment), D3D12 always needs one here.
    D3D12_RESOURCE_BARRIER parentToCopySrc{};
    parentToCopySrc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    parentToCopySrc.Transition.pResource   = frame.ParentResource;
    parentToCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    parentToCopySrc.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    parentToCopySrc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &parentToCopySrc);

    D3D12_RESOURCE_BARRIER backdropToCopyDst{};
    backdropToCopyDst.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    backdropToCopyDst.Transition.pResource   = _backdropResource.Get();
    backdropToCopyDst.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    backdropToCopyDst.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    backdropToCopyDst.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &backdropToCopyDst);

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource        = frame.ParentResource;
    srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource        = _backdropResource.Get();
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;
    _commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    D3D12_RESOURCE_BARRIER parentBackToRt{};
    parentBackToRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    parentBackToRt.Transition.pResource   = frame.ParentResource;
    parentBackToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    parentBackToRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    parentBackToRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &parentBackToRt);

    D3D12_RESOURCE_BARRIER backdropToSrv{};
    backdropToSrv.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    backdropToSrv.Transition.pResource   = _backdropResource.Get();
    backdropToSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    backdropToSrv.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    backdropToSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &backdropToSrv);
}

void NativeRendererDX12::CompositeBlendLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE sourceSlot = _compositeSrvCpuBase;
    sourceSlot.ptr += static_cast<SIZE_T>(1) * _srvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE backdropSlot = _compositeSrvCpuBase;
    backdropSlot.ptr += static_cast<SIZE_T>(2) * _srvDescriptorSize;
    _device->CreateShaderResourceView(target.Resource.Get(), &srvDesc, sourceSlot);
    _device->CreateShaderResourceView(_backdropResource.Get(), &srvDesc, backdropSlot);

    D3D12_GPU_DESCRIPTOR_HANDLE tableBase = _compositeSrvGpuBase;
    tableBase.ptr += static_cast<UINT64>(1) * _srvDescriptorSize; // covers slots +1 (source) and +2 (backdrop)

    const auto mode = static_cast<std::int32_t>(frame.Mode);
    _commandList->SetPipelineState(_blendPipelineState.Get());
    _commandList->SetGraphicsRootSignature(_blendRootSignature.Get());
    _commandList->SetGraphicsRootDescriptorTable(0, tableBase);
    _commandList->SetGraphicsRoot32BitConstants(1, 1, &mode, 0);
    _commandList->IASetVertexBuffers(0, 0, nullptr);
    _commandList->IASetIndexBuffer(nullptr);
    _commandList->DrawInstanced(6, 1, 0, 0); // no vertex buffer -- Blend.hlsl generates the quad from SV_VertexID
}

void NativeRendererDX12::PopLayer() {
    // A PopLayer with no matching Push{Opacity,Blend}Layer is a malformed Rendering::CommandBuffer
    // -- a caller bug, not a runtime condition this internal renderer recovers from.
    IMF_ASSERT(!_layerStack.empty());

    const LayerFrame frame = _layerStack.back();
    _layerStack.pop_back();

    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    if (frame.Op == LayerOp::PushBlend) {
        // Unlike Vulkan (whose active-rendering-instance restriction forces this to run before
        // resuming the parent), D3D12 has no such restriction -- CopyTextureRegion() needs no
        // "no instance active" precondition, so this can run at any point relative to
        // OMSetRenderTargets(); kept here, before resuming the parent, for structural parity with
        // NativeRendererVulkan::PopLayer()'s own ordering.
        CopyBackdropForBlend(frame);
    }

    // The popped layer's own content is read via SRV by the composite draw below -- transition it
    // out of RENDER_TARGET first.
    D3D12_RESOURCE_BARRIER toSrv{};
    toSrv.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toSrv.Transition.pResource   = target.Resource.Get();
    toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toSrv.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &toSrv);

    _commandList->OMSetRenderTargets(1, &frame.ParentRtv, FALSE, nullptr);

    const D3D12_VIEWPORT viewport{
        0.0f, 0.0f, static_cast<float>(_targetWidth), static_cast<float>(_targetHeight), 0.0f, 1.0f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(_targetWidth), static_cast<LONG>(_targetHeight)};
    _commandList->RSSetViewports(1, &viewport);
    _commandList->RSSetScissorRects(1, &scissor);
    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (frame.Op == LayerOp::PushOpacity) {
        CompositeOpacityLayer(frame);
    } else {
        CompositeBlendLayer(frame);
    }

    // target.Resource is already back in PIXEL_SHADER_RESOURCE (transitioned above, before the
    // composite draw) -- its own "at rest" state for whatever PushLayer() reuses this depth next,
    // no further transition needed.
}

void NativeRendererDX12::BeginMainCommandList() {
    _commandAllocator->Reset();
    _commandList->Reset(_commandAllocator.Get(), nullptr);

    if (_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = {_srvHeap.Get()};
        _commandList->SetDescriptorHeaps(1, heaps);
    }

    _commandList->OMSetRenderTargets(1, &_targetRtv, FALSE, nullptr);

    const D3D12_VIEWPORT viewport{0.0f,
                                 0.0f,
                                 static_cast<float>(_targetWidth),
                                 static_cast<float>(_targetHeight),
                                 0.0f,
                                 1.0f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(_targetWidth), static_cast<LONG>(_targetHeight)};
    _commandList->RSSetViewports(1, &viewport);
    _commandList->RSSetScissorRects(1, &scissor);
    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void NativeRendererDX12::EndAndSubmitMainCommandList() {
    _commandList->Close();
    ID3D12CommandList* lists[] = {_commandList.Get()};
    _directQueue->ExecuteCommandLists(1, lists);

    // Block until this call's GPU work completes before returning -- see this class's own file
    // comment on this deliberate, documented stopgap.
    ++_fenceValue;
    _directQueue->Signal(_fence.Get(), _fenceValue);
    if (_fence->GetCompletedValue() < _fenceValue) {
        _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
}

void NativeRendererDX12::RenderShadowBatch(const Batch& batch) {
    const auto& shadows = std::get<std::vector<ShadowVertex>>(batch.Vertices);
    if (shadows.empty()) { return; }
    const ShadowVertex& shadow = shadows.front();

    // Spread grows the silhouette outward on all sides before blurring -- matches
    // Rendering::DrawShadow::Spread's documented meaning, mirroring NativeRendererVulkan's own
    // identical formula.
    const float spreadWidth  = shadow.Size.x + 2.0f * shadow.Spread;
    const float spreadHeight = shadow.Size.y + 2.0f * shadow.Spread;
    if (spreadWidth <= 0.0f || spreadHeight <= 0.0f) { return; }

    // Pad the offscreen silhouette by the blur radius on every side so _blurPass's kernel has real
    // surrounding content to read at the silhouette's own edges, instead of clamped-edge repeats.
    const float pad = std::max(shadow.BlurRadius, 0.0f);
    const auto texWidth  = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(spreadWidth + 2.0f * pad))));
    const auto texHeight = static_cast<std::uint32_t>(std::max(1, static_cast<int>(std::ceil(spreadHeight + 2.0f * pad))));

    // Flush everything recorded so far -- _blurPass.Apply() below is its own fully self-contained,
    // synchronously-awaited submission; it cannot be recorded into the not-yet-submitted main
    // command list (see this class's own file comment).
    EndAndSubmitMainCommandList();

    EnsureShadowSilhouetteTarget(texWidth, texHeight);

    // ─── Silhouette: an opaque-white rounded rect, in the silhouette's own small coordinate space ───
    {
        struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
        const PerFrameCb perFrame{static_cast<float>(texWidth), static_cast<float>(texHeight)};
        std::memcpy(_perFrameCbMapped, &perFrame, sizeof(perFrame));

        const std::vector<RectVertex> silhouetteVertices =
            BuildRectQuadVertices({static_cast<float>(pad), static_cast<float>(pad)}, {spreadWidth, spreadHeight},
                                 shadow.Radii, {1.0f, 1.0f, 1.0f, 1.0f});
        constexpr std::array<std::uint32_t, 6> silhouetteIndices{0, 1, 2, 0, 2, 3};
        std::memcpy(_shadowQuadVertexMapped, silhouetteVertices.data(), silhouetteVertices.size() * sizeof(RectVertex));
        std::memcpy(_shadowQuadIndexMapped, silhouetteIndices.data(), silhouetteIndices.size() * sizeof(std::uint32_t));

        _commandAllocator->Reset();
        _commandList->Reset(_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toRt{};
        toRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toRt.Transition.pResource   = _shadowSilhouetteResource.Get();
        toRt.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toRt);

        _commandList->OMSetRenderTargets(1, &_shadowRtv, FALSE, nullptr);
        const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        _commandList->ClearRenderTargetView(_shadowRtv, clearColor, 0, nullptr);

        const D3D12_VIEWPORT viewport{
            0.0f, 0.0f, static_cast<float>(texWidth), static_cast<float>(texHeight), 0.0f, 1.0f};
        const D3D12_RECT scissor{0, 0, static_cast<LONG>(texWidth), static_cast<LONG>(texHeight)};
        _commandList->RSSetViewports(1, &viewport);
        _commandList->RSSetScissorRects(1, &scissor);
        _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        _commandList->SetPipelineState(_rectPipelineState.Get());
        _commandList->SetGraphicsRootSignature(_rootSignature.Get());
        _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());

        D3D12_VERTEX_BUFFER_VIEW vbView{};
        vbView.BufferLocation = _shadowQuadVertexBuffer->GetGPUVirtualAddress();
        vbView.SizeInBytes    = static_cast<UINT>(silhouetteVertices.size() * sizeof(RectVertex));
        vbView.StrideInBytes  = sizeof(RectVertex);
        _commandList->IASetVertexBuffers(0, 1, &vbView);

        D3D12_INDEX_BUFFER_VIEW ibView{};
        ibView.BufferLocation = _shadowQuadIndexBuffer->GetGPUVirtualAddress();
        ibView.SizeInBytes    = static_cast<UINT>(silhouetteIndices.size() * sizeof(std::uint32_t));
        ibView.Format         = DXGI_FORMAT_R32_UINT;
        _commandList->IASetIndexBuffer(&ibView);

        _commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

        D3D12_RESOURCE_BARRIER toUav{};
        toUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toUav.Transition.pResource   = _shadowSilhouetteResource.Get();
        toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toUav);

        _commandList->Close();
        ID3D12CommandList* lists[] = {_commandList.Get()};
        _directQueue->ExecuteCommandLists(1, lists);
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    const BlurResult blurred =
        _blurPass.Apply(BlurResult{_shadowSilhouetteResource.Get()}, texWidth, texHeight, shadow.BlurRadius);

    // Transition the borrowed blurred resource UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE for our
    // own composite SRV -- BlurPassDX12 owns this resource and expects it back in
    // UNORDERED_ACCESS by its own next Apply() call, so it must be transitioned back afterward too
    // (see this class's own file comment on why Vulkan's GENERAL layout needed neither transition).
    {
        _commandAllocator->Reset();
        _commandList->Reset(_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toSrv{};
        toSrv.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource   = blurred.Resource;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toSrv.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _commandList->ResourceBarrier(1, &toSrv);

        _commandList->Close();
        ID3D12CommandList* lists[] = {_commandList.Get()};
        _directQueue->ExecuteCommandLists(1, lists);
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;
    _device->CreateShaderResourceView(blurred.Resource, &srvDesc, _compositeSrvCpuBase);

    // ─── Composite: resume the main command list, restore its own PerFrame CB, draw ───────────
    BeginMainCommandList();

    struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameCb mainPerFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameCbMapped, &mainPerFrame, sizeof(mainPerFrame));

    // Top-left of the padded silhouette texture, in the shape's own coordinate space, plus the
    // shadow's drop offset -- matches NativeRendererVulkan::RenderShadowBatch()'s identical
    // placement formula.
    const Widgets::Vec2 compositePosition{
        shadow.Position.x - shadow.Spread - pad + shadow.Offset.x,
        shadow.Position.y - shadow.Spread - pad + shadow.Offset.y,
    };
    const Widgets::Vec2 compositeSize{static_cast<float>(texWidth), static_cast<float>(texHeight)};
    const std::vector<ImageVertex> compositeVertices =
        BuildImageQuadVertices(compositePosition, compositeSize, shadow.ShadowColor);
    constexpr std::array<std::uint32_t, 6> compositeIndices{0, 1, 2, 0, 2, 3};
    std::memcpy(_shadowQuadVertexMapped, compositeVertices.data(), compositeVertices.size() * sizeof(ImageVertex));
    std::memcpy(_shadowQuadIndexMapped, compositeIndices.data(), compositeIndices.size() * sizeof(std::uint32_t));

    _commandList->SetPipelineState(_imagePipelineState.Get());
    _commandList->SetGraphicsRootSignature(_imageRootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(1, _compositeSrvGpuBase);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = _shadowQuadVertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes    = static_cast<UINT>(compositeVertices.size() * sizeof(ImageVertex));
    vbView.StrideInBytes  = sizeof(ImageVertex);
    _commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = _shadowQuadIndexBuffer->GetGPUVirtualAddress();
    ibView.SizeInBytes    = static_cast<UINT>(compositeIndices.size() * sizeof(std::uint32_t));
    ibView.Format         = DXGI_FORMAT_R32_UINT;
    _commandList->IASetIndexBuffer(&ibView);

    _commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

    // Restore BlurPassDX12's own "always UNORDERED_ACCESS at rest" invariant for its next Apply()
    // call -- see this method's own comment above on why this transition-back is necessary here,
    // unlike Vulkan's zero-transition GENERAL layout.
    D3D12_RESOURCE_BARRIER backToUav{};
    backToUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    backToUav.Transition.pResource   = blurred.Resource;
    backToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    backToUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    backToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    _commandList->ResourceBarrier(1, &backToUav);
}

void NativeRendererDX12::RenderRectBatch(const Batch& batch, std::size_t& vertexByteOffset,
                                         std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    // Rebinds unconditionally, regardless of what the previous batch (if any) bound -- matches
    // NativeRendererVulkan::RenderRectBatch()'s own identical convention, and correctly handles
    // Rect/Image batches interleaving within one Render() call now that both pipelines exist.
    _commandList->SetPipelineState(_rectPipelineState.Get());
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());

    const std::size_t vertexBytes = vertices.size() * sizeof(RectVertex);
    const std::size_t indexBytes  = batch.Indices.size() * sizeof(std::uint32_t);

    std::memcpy(static_cast<std::byte*>(_vertexBufferMapped) + vertexByteOffset, vertices.data(), vertexBytes);
    std::memcpy(static_cast<std::byte*>(_indexBufferMapped) + indexByteOffset, batch.Indices.data(), indexBytes);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress() + vertexByteOffset;
    vbView.SizeInBytes    = static_cast<UINT>(vertexBytes);
    vbView.StrideInBytes  = sizeof(RectVertex);
    _commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = _indexBuffer->GetGPUVirtualAddress() + indexByteOffset;
    ibView.SizeInBytes    = static_cast<UINT>(indexBytes);
    ibView.Format         = DXGI_FORMAT_R32_UINT;
    _commandList->IASetIndexBuffer(&ibView);

    _commandList->DrawIndexedInstanced(static_cast<UINT>(batch.Indices.size()), 1, 0, 0, 0);

    vertexByteOffset += vertexBytes;
    indexByteOffset += indexBytes;
}

void NativeRendererDX12::RenderImageBatch(D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle, const Batch& batch,
                                          std::size_t& vertexByteOffset, std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<ImageVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    // srvGpuHandle was already written (both the SRV itself, and which resource it points at) by
    // Render(), before command-list recording began -- see this class's own file comment on why
    // the write can't happen here, per-batch, the way RenderRectBatch() rebinds its own root
    // signature/PSO/CBV unconditionally (neither has any per-batch texture to write).
    _commandList->SetPipelineState(_imagePipelineState.Get());
    _commandList->SetGraphicsRootSignature(_imageRootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

    const std::size_t vertexBytes = vertices.size() * sizeof(ImageVertex);
    const std::size_t indexBytes  = batch.Indices.size() * sizeof(std::uint32_t);

    std::memcpy(static_cast<std::byte*>(_vertexBufferMapped) + vertexByteOffset, vertices.data(), vertexBytes);
    std::memcpy(static_cast<std::byte*>(_indexBufferMapped) + indexByteOffset, batch.Indices.data(), indexBytes);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress() + vertexByteOffset;
    vbView.SizeInBytes    = static_cast<UINT>(vertexBytes);
    vbView.StrideInBytes  = sizeof(ImageVertex);
    _commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = _indexBuffer->GetGPUVirtualAddress() + indexByteOffset;
    ibView.SizeInBytes    = static_cast<UINT>(indexBytes);
    ibView.Format         = DXGI_FORMAT_R32_UINT;
    _commandList->IASetIndexBuffer(&ibView);

    _commandList->DrawIndexedInstanced(static_cast<UINT>(batch.Indices.size()), 1, 0, 0, 0);

    vertexByteOffset += vertexBytes;
    indexByteOffset += indexBytes;
}

void NativeRendererDX12::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    // A missing SetTarget() call is a caller bug, not a runtime condition to recover from --
    // matches NativeRendererVulkan's own equivalent contract.
    IMF_ASSERT(_targetResource != nullptr);

    // "Per frame" == "per Render() call" -- matches NativeRendererVulkan's/NativeRendererGL3's own
    // identical reset point for their own rate limit.
    _backdropBlurCountThisFrame = 0;

    _batchBuilder.Build(buffer);
    const std::vector<Batch>& batches = _batchBuilder.Batches();

    std::size_t totalVertexBytes = 0;
    std::size_t totalIndexBytes  = 0;
    std::size_t imageBatchCount  = 0;
    std::size_t shadowBatchCount = 0;
    std::size_t layerBatchCount  = 0;
    std::size_t backdropBlurBatchCount = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) {
            totalVertexBytes += std::get<std::vector<RectVertex>>(batch.Vertices).size() * sizeof(RectVertex);
            totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
        } else if (batch.Kind == BatchKind::Image) {
            totalVertexBytes += std::get<std::vector<ImageVertex>>(batch.Vertices).size() * sizeof(ImageVertex);
            totalIndexBytes += batch.Indices.size() * sizeof(std::uint32_t);
            ++imageBatchCount;
        } else if (batch.Kind == BatchKind::Shadow) {
            ++shadowBatchCount; // uses its own dedicated buffers, not totalVertexBytes/totalIndexBytes
        } else if (batch.Kind == BatchKind::Layer) {
            ++layerBatchCount; // push/pop markers only -- no vertex/index data of their own either
        } else if (batch.Kind == BatchKind::BackdropBlur) {
            ++backdropBlurBatchCount; // uses its own dedicated buffers too
        }
        // BatchKind::Text is out of this sub-phase's scope, matching NativeRendererVulkan's own
        // identical Phase 35.6 starting point.
    }
    if (totalVertexBytes == 0 && shadowBatchCount == 0 && layerBatchCount == 0 && backdropBlurBatchCount == 0) {
        return;
    }

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameCb perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameCbMapped, &perFrame, sizeof(perFrame));

    // 3 extra, reserved slots (beyond one per Image batch) for every renderer-internal composite
    // draw this call might need: slot +0 shared by RenderShadowBatch()/CompositeOpacityLayer()
    // (one texture each), slots +1/+2 by CompositeBlendLayer() (two textures) -- see this class's
    // own file comment on why this shares _srvHeap rather than using a second heap (D3D12 only
    // allows one CBV_SRV_UAV heap bound via SetDescriptorHeaps() at a time per command list).
    // Reserved whenever any of Shadow/Layer/BackdropBlur batches exist, regardless of which specific
    // ops are used this frame -- a small, constant amount of heap-slot overhead, not worth scanning
    // LayerVertex ops during this counting pass just to avoid. BackdropBlur reuses slot +0 too (a
    // single-texture composite, same footprint as Shadow's/CompositeOpacityLayer's own).
    const bool needsCompositeSlots =
        shadowBatchCount > 0 || layerBatchCount > 0 || backdropBlurBatchCount > 0;
    const std::size_t compositeSrvSlot = imageBatchCount; // valid only when needsCompositeSlots
    const std::size_t neededSrvSlots   = imageBatchCount + (needsCompositeSlots ? 3 : 0);

    // Write every Image batch's SRV into _srvHeap entirely before command-list recording begins --
    // see RenderImageBatch()'s own comment, and NativeRendererVulkan::Render()'s identical Phase
    // 35.2 reasoning, for why this can't happen per-batch inside the recording loop below. The
    // composite slots (if reserved) are written later, by RenderShadowBatch()/CompositeOpacityLayer()/
    // CompositeBlendLayer() themselves, once whatever they each depend on has actually completed --
    // see those methods' own comments for why that's safe.
    if (neededSrvSlots > 0) {
        EnsureImageDescriptorCapacity(neededSrvSlots);

        const D3D12_CPU_DESCRIPTOR_HANDLE srvHeapCpuStart = _srvHeap->GetCPUDescriptorHandleForHeapStart();
        std::size_t nextSrvSlot = 0;
        for (const Batch& batch : batches) {
            if (batch.Kind != BatchKind::Image) { continue; }

            // batch.Texture's value is a raw ID3D12Resource* -- see this class's own file comment
            // on why (the D3D12 analogue of NativeRendererVulkan's raw-VkImageView convention).
            auto* resource = reinterpret_cast<ID3D12Resource*>(static_cast<std::uintptr_t>(batch.Texture.Value()));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format                  = DXGI_FORMAT_UNKNOWN; // inherit the resource's own format
            srvDesc.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels     = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeapCpuStart;
            cpuHandle.ptr += static_cast<SIZE_T>(nextSrvSlot) * _srvDescriptorSize;
            _device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

            ++nextSrvSlot;
        }
    }

    _compositeSrvCpuBase = {};
    _compositeSrvGpuBase = {};
    if (needsCompositeSlots) {
        _compositeSrvCpuBase = _srvHeap->GetCPUDescriptorHandleForHeapStart();
        _compositeSrvCpuBase.ptr += static_cast<SIZE_T>(compositeSrvSlot) * _srvDescriptorSize;
        _compositeSrvGpuBase = _srvHeap->GetGPUDescriptorHandleForHeapStart();
        _compositeSrvGpuBase.ptr += static_cast<UINT64>(compositeSrvSlot) * _srvDescriptorSize;
    }

    BeginMainCommandList();

    const D3D12_GPU_DESCRIPTOR_HANDLE srvHeapGpuStart =
        imageBatchCount > 0 ? _srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};

    std::size_t vertexByteOffset = 0;
    std::size_t indexByteOffset  = 0;
    std::size_t imageSrvSlot     = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) {
            RenderRectBatch(batch, vertexByteOffset, indexByteOffset);
        } else if (batch.Kind == BatchKind::Image) {
            D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvHeapGpuStart;
            srvGpuHandle.ptr += static_cast<UINT64>(imageSrvSlot++) * _srvDescriptorSize;
            RenderImageBatch(srvGpuHandle, batch, vertexByteOffset, indexByteOffset);
        } else if (batch.Kind == BatchKind::Shadow) {
            // Ends and re-begins the main command list internally -- see this method's own comment
            // on why (BlurPassDX12::Apply() is its own separate submission).
            RenderShadowBatch(batch);
        } else if (batch.Kind == BatchKind::Layer) {
            HandleLayerMarker(batch);
        } else if (batch.Kind == BatchKind::BackdropBlur) {
            // Ends and re-begins the main command list internally, same reason as RenderShadowBatch().
            RenderBackdropBlurBatch(batch);
        }
    }

    EndAndSubmitMainCommandList();
}

void NativeRendererDX12::Shutdown() {
    if (!_initialized) { return; }

    // Render() already waits synchronously for its own submission before returning, so nothing
    // should be in flight here -- this is cheap insurance for Shutdown() being callable at any
    // time per IRenderer's own "safe to call multiple times" contract.
    if (_fence && _fenceEvent) {
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    if (_vertexBuffer) { _vertexBuffer->Unmap(0, nullptr); _vertexBuffer.Reset(); }
    _vertexBufferMapped        = nullptr;
    _vertexBufferCapacityBytes = 0;

    if (_indexBuffer) { _indexBuffer->Unmap(0, nullptr); _indexBuffer.Reset(); }
    _indexBufferMapped        = nullptr;
    _indexBufferCapacityBytes = 0;

    if (_perFrameCb) { _perFrameCb->Unmap(0, nullptr); _perFrameCb.Reset(); }
    _perFrameCbMapped = nullptr;

    if (_shadowQuadVertexBuffer) { _shadowQuadVertexBuffer->Unmap(0, nullptr); _shadowQuadVertexBuffer.Reset(); }
    _shadowQuadVertexMapped = nullptr;
    if (_shadowQuadIndexBuffer) { _shadowQuadIndexBuffer->Unmap(0, nullptr); _shadowQuadIndexBuffer.Reset(); }
    _shadowQuadIndexMapped = nullptr;

    _shadowSilhouetteResource.Reset();
    _shadowRtvHeap.Reset();
    _shadowRtv              = {};
    _shadowSilhouetteWidth  = 0;
    _shadowSilhouetteHeight = 0;
    _blurPass.Shutdown();

    _layerStack.clear();
    _layerTargets.clear();
    _backdropResource.Reset();
    _backdropWidth  = 0;
    _backdropHeight = 0;
    _compositeSrvCpuBase = {};
    _compositeSrvGpuBase = {};

    _backdropBlurCopyResource.Reset();
    _backdropBlurCopyWidth      = 0;
    _backdropBlurCopyHeight     = 0;
    _backdropBlurCountThisFrame = 0;

    _srvHeap.Reset();
    _srvHeapCapacitySlots = 0;

    _commandList.Reset();
    _commandAllocator.Reset();
    _blendPipelineState.Reset();
    _blendRootSignature.Reset();
    _premultipliedImagePipelineState.Reset();
    _imagePipelineState.Reset();
    _imageRootSignature.Reset();
    _rectPipelineState.Reset();
    _rootSignature.Reset();

    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
    _fence.Reset();
    _fenceValue = 0;

    _initialized = false;
}

} // namespace ImFrame::Internal
