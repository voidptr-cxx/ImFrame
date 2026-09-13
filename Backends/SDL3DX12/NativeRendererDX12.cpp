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

#include "Shaders/SDFRect.Pixel.hpp"
#include "Shaders/SDFRect.Vertex.hpp"

#include <array>
#include <cstddef>
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

} // namespace

NativeRendererDX12::NativeRendererDX12(ID3D12Device4* device, ID3D12CommandQueue* directQueue,
                                       DXGI_FORMAT colorFormat)
    : _device(device), _directQueue(directQueue), _colorFormat(colorFormat) {}

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

    // ─── Command allocator/list, fence -- matches ViewportFramebufferDX12's own identical pattern ─
    _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
    _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                IID_PPV_ARGS(&_commandList));

    _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // ─── PerFrame constant buffer: vec2 ViewportSize ───────────────────────────────────────────
    _perFrameCb = CreateUploadBuffer(_device, AlignUp(sizeof(float) * 2, kConstantBufferAlignment),
                                     &_perFrameCbMapped);

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

void NativeRendererDX12::RenderRectBatch(const Batch& batch, std::size_t& vertexByteOffset,
                                         std::size_t& indexByteOffset) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

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

void NativeRendererDX12::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    // A missing SetTarget() call is a caller bug, not a runtime condition to recover from --
    // matches NativeRendererVulkan's own equivalent contract.
    IMF_ASSERT(_targetResource != nullptr);

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
        // scope, matching NativeRendererVulkan's own identical Phase 35.1 starting point.
    }
    if (totalVertexBytes == 0) { return; }

    EnsureVertexIndexCapacity(totalVertexBytes, totalIndexBytes);

    struct PerFrameCb { float ViewportSizeX; float ViewportSizeY; };
    const PerFrameCb perFrame{static_cast<float>(_targetWidth), static_cast<float>(_targetHeight)};
    std::memcpy(_perFrameCbMapped, &perFrame, sizeof(perFrame));

    _commandAllocator->Reset();
    _commandList->Reset(_commandAllocator.Get(), _rectPipelineState.Get());

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

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(0, _perFrameCb->GetGPUVirtualAddress());
    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::size_t vertexByteOffset = 0;
    std::size_t indexByteOffset  = 0;
    for (const Batch& batch : batches) {
        if (batch.Kind == BatchKind::Rect) { RenderRectBatch(batch, vertexByteOffset, indexByteOffset); }
    }

    _commandList->Close();
    ID3D12CommandList* lists[] = {_commandList.Get()};
    _directQueue->ExecuteCommandLists(1, lists);

    // Block until this Render() call's GPU work completes before returning -- a deliberate,
    // documented stopgap (see this class's own .hpp comment) so the streaming vertex/index/
    // constant buffers are always safe to overwrite again on the very next call.
    ++_fenceValue;
    _directQueue->Signal(_fence.Get(), _fenceValue);
    if (_fence->GetCompletedValue() < _fenceValue) {
        _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
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

    _commandList.Reset();
    _commandAllocator.Reset();
    _rectPipelineState.Reset();
    _rootSignature.Reset();

    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
    _fence.Reset();
    _fenceValue = 0;

    _initialized = false;
}

} // namespace ImFrame::Internal
