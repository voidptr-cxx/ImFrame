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

/// Builds one axis-aligned quad's `ImageVertex`es — `RenderShadowBatch()`'s own composite pass
/// reuses the existing Image pipeline to draw a renderer-produced offscreen texture (a blurred
/// shadow), tinted, as a plain rectangle. Mirrors `NativeRendererVulkan::BuildImageQuadVertices()`'s
/// role, including its **unflipped** UV table -- not because this backend's own vertex shader
/// matches Vulkan's (it doesn't; `Image.hlsl`'s `VSMain` negates Y like GL, Phase 35.9), but
/// because a D3D12 texture's row 0 is its top row (the same top-down memory convention Vulkan's
/// own images use), independent of whichever way a vertex shader happens to negate Y for
/// clip-space purposes -- the two questions (screen-position handedness vs. texture-row order)
/// are unrelated, and this backend's own DX12 texture-upload convention (already exercised
/// correctly by every `BatchKind::Image` test, Phase 35.9) is top-down like Vulkan's, not
/// bottom-up like GL's. A plain, unflipped mapping is therefore the correct choice here too.
std::vector<ImageVertex> BuildImageQuadVertices(Widgets::Vec2 position, Widgets::Vec2 size, Widgets::Vec4 tintColor) {
    const Widgets::Vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    const Widgets::Vec2 halfSize{size.x * 0.5f, size.y * 0.5f};
    const Widgets::Vec2 corners[4] = {
        {position.x, position.y},
        {position.x + size.x, position.y},
        {position.x + size.x, position.y + size.y},
        {position.x, position.y + size.y},
    };
    constexpr Widgets::Vec2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

    std::vector<ImageVertex> vertices;
    vertices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        vertices.push_back(ImageVertex{
            .Position = corners[i],
            .Local = {corners[i].x - center.x, corners[i].y - center.y},
            .HalfSize = halfSize,
            .Radii = {},
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

void NativeRendererDX12::RenderShadowBatch(const Batch& batch, D3D12_CPU_DESCRIPTOR_HANDLE compositeSrvCpuHandle,
                                           D3D12_GPU_DESCRIPTOR_HANDLE compositeSrvGpuHandle) {
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
    _device->CreateShaderResourceView(blurred.Resource, &srvDesc, compositeSrvCpuHandle);

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
    _commandList->SetGraphicsRootDescriptorTable(1, compositeSrvGpuHandle);

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

    _batchBuilder.Build(buffer);
    const std::vector<Batch>& batches = _batchBuilder.Batches();

    std::size_t totalVertexBytes = 0;
    std::size_t totalIndexBytes  = 0;
    std::size_t imageBatchCount  = 0;
    std::size_t shadowBatchCount = 0;
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
        }
        // Every other BatchKind (Text/Layer/BackdropBlur) is out of this sub-phase's scope, matching
        // NativeRendererVulkan's own identical Phase 35.4 starting point.
    }
    if (totalVertexBytes == 0 && shadowBatchCount == 0) { return; }

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameCb perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameCbMapped, &perFrame, sizeof(perFrame));

    // One extra, reserved slot (beyond one per Image batch) for RenderShadowBatch()'s own composite
    // draw, if any Shadow batch exists this call -- see this class's own file comment on why this
    // shares _srvHeap rather than using a second heap (D3D12 only allows one CBV_SRV_UAV heap bound
    // via SetDescriptorHeaps() at a time per command list).
    const std::size_t compositeSrvSlot  = imageBatchCount; // valid only when shadowBatchCount > 0
    const std::size_t neededSrvSlots    = imageBatchCount + (shadowBatchCount > 0 ? 1 : 0);

    // Write every Image batch's SRV into _srvHeap entirely before command-list recording begins --
    // see RenderImageBatch()'s own comment, and NativeRendererVulkan::Render()'s identical Phase
    // 35.2 reasoning, for why this can't happen per-batch inside the recording loop below. The
    // composite slot (if reserved) is written later, by RenderShadowBatch() itself, once the blur
    // it depends on has actually completed -- see that method's own comment for why that's safe.
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

    D3D12_CPU_DESCRIPTOR_HANDLE compositeSrvCpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE compositeSrvGpuHandle{};
    if (shadowBatchCount > 0) {
        compositeSrvCpuHandle = _srvHeap->GetCPUDescriptorHandleForHeapStart();
        compositeSrvCpuHandle.ptr += static_cast<SIZE_T>(compositeSrvSlot) * _srvDescriptorSize;
        compositeSrvGpuHandle = _srvHeap->GetGPUDescriptorHandleForHeapStart();
        compositeSrvGpuHandle.ptr += static_cast<UINT64>(compositeSrvSlot) * _srvDescriptorSize;
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
            RenderShadowBatch(batch, compositeSrvCpuHandle, compositeSrvGpuHandle);
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

    _srvHeap.Reset();
    _srvHeapCapacitySlots = 0;

    _commandList.Reset();
    _commandAllocator.Reset();
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
