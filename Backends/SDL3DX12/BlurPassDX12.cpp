/**
 * @file     BlurPassDX12.cpp
 * @brief    `BlurPassDX12` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-15
 * @version  3.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "BlurPassDX12.hpp"

#include "Shaders/GaussianBlur.Compute.hpp"

#include <algorithm>
#include <array>

using Microsoft::WRL::ComPtr;

namespace ImFrame::Internal {

namespace {

constexpr DXGI_FORMAT     kFormat        = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr std::uint32_t   kWorkgroupSize = 16;

[[nodiscard]] std::uint32_t DivRoundUp(std::uint32_t value, std::uint32_t divisor) {
    return (value + divisor - 1) / divisor;
}

/// Allocates one `DXGI_FORMAT_R8G8B8A8_UNORM` UAV-capable resource, created directly in (and never
/// leaving) `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` -- see `BlurPassDX12.hpp`'s own file comment
/// on why no Vulkan-`GENERAL`-style dual-role layout exists in D3D12, and why that doesn't matter
/// for these two ping-pong targets specifically (they are never rendered into).
ComPtr<ID3D12Resource> CreateUavTexture(ID3D12Device4* device, std::uint32_t width, std::uint32_t height) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = kFormat;
    desc.SampleDesc.Count = 1;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ComPtr<ID3D12Resource> resource;
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

void WriteUav(ID3D12Device4* device, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format             = kFormat;
    uavDesc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, handle);
}

} // namespace

BlurPassDX12::BlurPassDX12(ID3D12Device4* device, ID3D12CommandQueue* directQueue)
    : _device(device), _directQueue(directQueue) {}

BlurPassDX12::~BlurPassDX12() { Shutdown(); }

void BlurPassDX12::EnsureInitialized() {
    if (_initialized) { return; }

    // ─── Root signature: one UAV descriptor table (u0/u1) + root constants (Direction/Radius) ───
    D3D12_DESCRIPTOR_RANGE1 uavRange{};
    uavRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors     = 2;
    uavRange.BaseShaderRegister = 0;

    std::array<D3D12_ROOT_PARAMETER1, 2> rootParams{};
    rootParams[0].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges    = &uavRange;
    rootParams[0].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[1].ParameterType                 = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[1].Constants.ShaderRegister     = 0;
    rootParams[1].Constants.Num32BitValues     = 3; // Direction.xy + Radius
    rootParams[1].ShaderVisibility              = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Version                = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = static_cast<UINT>(rootParams.size());
    rootSigDesc.Desc_1_1.pParameters   = rootParams.data();

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&rootSigDesc, &serialized, &error);
    _device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                 IID_PPV_ARGS(&_rootSignature));

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.pRootSignature = _rootSignature.Get();
    pipelineDesc.CS = {Shaders::kGaussianBlurComputeDxil, Shaders::kGaussianBlurComputeDxilByteCount};
    _device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&_pipelineState));

    _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
    _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                IID_PPV_ARGS(&_commandList));
    _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    _uavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    _initialized = true;
}

void BlurPassDX12::EnsureTargets(std::uint32_t width, std::uint32_t height) {
    if (_targetWidth == width && _targetHeight == height && _imageA) { return; }

    _imageA = CreateUavTexture(_device, width, height);
    _imageB = CreateUavTexture(_device, width, height);
    _targetWidth  = width;
    _targetHeight = height;

    // 4 slots: [0]=this call's own source (rewritten per Apply(), below), [1]=_imageA (pass 1's
    // fixed dest), [2]=_imageA again (pass 2's fixed source), [3]=_imageB (pass 2's fixed dest) --
    // see this class's own .hpp comment for why slots 1-3 never change again at this size, only
    // slot 0 does.
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 4;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    _device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_uavHeap));

    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = _uavHeap->GetCPUDescriptorHandleForHeapStart();
    auto slotHandle = [&](std::size_t slot) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = heapStart;
        h.ptr += static_cast<SIZE_T>(slot) * _uavDescriptorSize;
        return h;
    };
    WriteUav(_device, _imageA.Get(), slotHandle(1));
    WriteUav(_device, _imageA.Get(), slotHandle(2));
    WriteUav(_device, _imageB.Get(), slotHandle(3));
}

void BlurPassDX12::RecordPass(D3D12_GPU_DESCRIPTOR_HANDLE tableBase, std::uint32_t width, std::uint32_t height,
                              float directionX, float directionY, float radius) {
    _commandList->SetPipelineState(_pipelineState.Get());
    _commandList->SetComputeRootDescriptorTable(0, tableBase);

    const std::array<float, 3> pc{directionX, directionY, radius};
    _commandList->SetComputeRoot32BitConstants(1, static_cast<UINT>(pc.size()), pc.data(), 0);

    _commandList->Dispatch(DivRoundUp(width, kWorkgroupSize), DivRoundUp(height, kWorkgroupSize), 1);
}

BlurResult BlurPassDX12::Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius) {
    if (radius <= 0.0f) { return source; }
    const float clampedRadius = std::min(radius, 64.0f);

    EnsureInitialized();
    EnsureTargets(width, height);

    // Slot 0 is the only piece of descriptor state that changes per call -- this call's own
    // source resource.
    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = _uavHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE       slot0     = heapStart;
    WriteUav(_device, source.Resource, slot0);

    _commandAllocator->Reset();
    _commandList->Reset(_commandAllocator.Get(), nullptr);

    ID3D12DescriptorHeap* heaps[] = {_uavHeap.Get()};
    _commandList->SetDescriptorHeaps(1, heaps);
    _commandList->SetComputeRootSignature(_rootSignature.Get());

    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = _uavHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE       pass1Table    = gpuHeapStart; // slots 0-1
    D3D12_GPU_DESCRIPTOR_HANDLE       pass2Table    = gpuHeapStart;
    pass2Table.ptr += static_cast<UINT64>(2) * _uavDescriptorSize; // slots 2-3

    RecordPass(pass1Table, width, height, 1.0f, 0.0f, clampedRadius); // horizontal -> _imageA

    // Pass 2 depends on pass 1's writes to _imageA -- Dispatch() calls give no implicit ordering
    // between them, unlike a render pass's own implicit ordering, so this barrier is not optional.
    D3D12_RESOURCE_BARRIER betweenPasses{};
    betweenPasses.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    betweenPasses.UAV.pResource = _imageA.Get();
    _commandList->ResourceBarrier(1, &betweenPasses);

    RecordPass(pass2Table, width, height, 0.0f, 1.0f, clampedRadius); // vertical -> _imageB

    _commandList->Close();
    ID3D12CommandList* lists[] = {_commandList.Get()};
    _directQueue->ExecuteCommandLists(1, lists);

    // Block until this Apply() call's GPU work completes before returning -- the same deliberate
    // stopgap NativeRendererDX12::Render() uses (see this class's own .hpp comment).
    ++_fenceValue;
    _directQueue->Signal(_fence.Get(), _fenceValue);
    if (_fence->GetCompletedValue() < _fenceValue) {
        _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }

    return BlurResult{_imageB.Get()};
}

void BlurPassDX12::Shutdown() {
    if (!_initialized) { return; }

    if (_fence && _fenceEvent) {
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    _imageA.Reset();
    _imageB.Reset();
    _targetWidth  = 0;
    _targetHeight = 0;

    _uavHeap.Reset();

    _commandList.Reset();
    _commandAllocator.Reset();
    _pipelineState.Reset();
    _rootSignature.Reset();

    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
    _fence.Reset();
    _fenceValue = 0;

    _initialized = false;
}

} // namespace ImFrame::Internal
