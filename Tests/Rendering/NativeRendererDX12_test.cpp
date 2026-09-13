/**
 * @file     NativeRendererDX12_test.cpp
 * @brief    Real-pixel tests for NativeRendererDX12's DrawRect rendering (Phase 35.7)
 *
 * @internal
 * Mirrors `NativeRendererVulkan_test.cpp`'s own pattern (a hand-created offscreen render target,
 * no `Application`/`Viewport` machinery) but for D3D12: a self-contained `ScratchTarget` allocates
 * a `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET` `ID3D12Resource` + its own small RTV descriptor
 * heap, clears it up front, then reads it back via a one-shot command list copying into a
 * `D3D12_HEAP_TYPE_READBACK` buffer.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3DX12/NativeRendererDX12.hpp"
#include "Backends/SDL3DX12/SDL3DX12Backend.hpp"

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
constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "NativeRendererDX12_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width, std::size_t rowPitch) {
    const std::size_t off = static_cast<std::size_t>(y) * rowPitch + static_cast<std::size_t>(x) * 4;
    (void)width;
    return {static_cast<int>(pixels[off + 0]), static_cast<int>(pixels[off + 1]), static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3])};
}

/// A small, self-contained offscreen render target — see this file's own header comment for why
/// a hand-rolled target is used instead of the backend's real swap chain (matches
/// `NativeRendererVulkan_test.cpp`'s identical reasoning for its own `ScratchImage`).
class ScratchTarget {
public:
    ScratchTarget(ID3D12Device4* device, ID3D12CommandQueue* queue, int width, int height)
        : _device(device), _queue(queue), _width(width), _height(height) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width            = static_cast<UINT64>(width);
        desc.Height           = static_cast<UINT>(height);
        desc.DepthOrArraySize = 1;
        desc.MipLevels        = 1;
        desc.Format           = kColorFormat;
        desc.SampleDesc.Count = 1;
        desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal{};
        clearVal.Format = kColorFormat;

        _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                         D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal,
                                         IID_PPV_ARGS(&_resource));

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        _device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_rtvHeap));
        _rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format        = kColorFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        _device->CreateRenderTargetView(_resource.Get(), &rtvDesc, _rtvHandle);

        _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));
        _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                    IID_PPV_ARGS(&_cmdList));
        _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
        _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        ClearToTransparent();
    }

    ~ScratchTarget() { if (_fenceEvent) { CloseHandle(_fenceEvent); } }

    ScratchTarget(const ScratchTarget&) = delete;
    ScratchTarget& operator=(const ScratchTarget&) = delete;

    [[nodiscard]] ID3D12Resource* Resource() const noexcept { return _resource.Get(); }
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const noexcept { return _rtvHandle; }

    /// Returns tightly-packed-row pixels and the source buffer's own row pitch (D3D12 readback
    /// rows are 256-byte-pitch-aligned, not necessarily `width * 4`) — callers index via
    /// `Sample()`'s own `rowPitch` parameter rather than assuming `width * 4`.
    // Not const -- reuses this instance's own persistent _cmdAlloc/_cmdList/_fence across calls
    // (unlike NativeRendererVulkan_test.cpp's own ScratchImage::ReadPixels(), which allocates a
    // fresh one-shot command buffer per call and can stay const).
    [[nodiscard]] std::pair<std::vector<std::byte>, std::size_t> ReadPixels() {
        constexpr std::size_t kRowPitchAlignment = 256;
        const std::size_t rowPitch =
            (static_cast<std::size_t>(_width) * 4 + kRowPitchAlignment - 1) & ~(kRowPitchAlignment - 1);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width            = static_cast<UINT64>(rowPitch * static_cast<std::size_t>(_height));
        bufDesc.Height           = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels        = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));

        _cmdAlloc->Reset();
        _cmdList->Reset(_cmdAlloc.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toCopySrc{};
        toCopySrc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopySrc.Transition.pResource   = _resource.Get();
        toCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toCopySrc.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopySrc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &toCopySrc);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource        = _resource.Get();
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource                          = readback.Get();
        dstLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLoc.PlacedFootprint.Footprint.Format   = kColorFormat;
        dstLoc.PlacedFootprint.Footprint.Width    = static_cast<UINT>(_width);
        dstLoc.PlacedFootprint.Footprint.Height   = static_cast<UINT>(_height);
        dstLoc.PlacedFootprint.Footprint.Depth    = 1;
        dstLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

        _cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_RESOURCE_BARRIER backToRt{};
        backToRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backToRt.Transition.pResource   = _resource.Get();
        backToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        backToRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        backToRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &backToRt);

        SubmitAndWait();

        void* mapped = nullptr;
        const D3D12_RANGE readRange{0, static_cast<SIZE_T>(rowPitch * static_cast<std::size_t>(_height))};
        readback->Map(0, &readRange, &mapped);
        std::vector<std::byte> result(rowPitch * static_cast<std::size_t>(_height));
        std::memcpy(result.data(), mapped, result.size());
        readback->Unmap(0, nullptr);

        return {result, rowPitch};
    }

private:
    void SubmitAndWait() {
        _cmdList->Close();
        ID3D12CommandList* lists[] = {_cmdList.Get()};
        _queue->ExecuteCommandLists(1, lists);
        ++_fenceValue;
        _queue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    void ClearToTransparent() {
        _cmdAlloc->Reset();
        _cmdList->Reset(_cmdAlloc.Get(), nullptr);
        const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        _cmdList->ClearRenderTargetView(_rtvHandle, clearColor, 0, nullptr);
        SubmitAndWait();
    }

    ID3D12Device4*      _device = nullptr;
    ID3D12CommandQueue* _queue  = nullptr;
    int                 _width;
    int                 _height;

    Microsoft::WRL::ComPtr<ID3D12Resource>            _resource;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      _rtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                       _rtvHandle{};
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList;
    Microsoft::WRL::ComPtr<ID3D12Fence>                _fence;
    std::uint64_t                                      _fenceValue = 0;
    HANDLE                                              _fenceEvent = nullptr;
};

} // namespace

TEST_CASE("NativeRendererDX12 draws a filled DrawRect at the recorded position", "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {static_cast<float>(WIDTH) / 2.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel left  = Sample(pixels, WIDTH / 4, HEIGHT / 2, WIDTH, rowPitch);
        const Pixel right = Sample(pixels, WIDTH * 3 / 4, HEIGHT / 2, WIDTH, rowPitch);

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

TEST_CASE("NativeRendererDX12 renders an off-center rect: asymmetric placement stays correct "
          "(catches a Y-axis-convention mismatch immediately, per Phase 35.2's own lesson)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        // A rect only in the TOP-left quadrant -- deliberately asymmetric in Y, so a Y-flip bug
        // (like Phase 35.2's own Vulkan one) would show up as content in the wrong half instead of
        // passing by coincidence.
        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 20.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel insideRect = Sample(pixels, 20, 20, WIDTH, rowPitch);   // inside the rect's own footprint
        const Pixel belowRect  = Sample(pixels, 20, 100, WIDTH, rowPitch); // well below it -- untouched

        REQUIRE(insideRect.r > 200);
        REQUIRE(insideRect.a > 200);
        REQUIRE(belowRect.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 renders rounded corners: the extreme corner pixel stays outside the fill",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .Radii    = CornerRadii::All(24.0f),
            .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel corner     = Sample(pixels, 1, 1, WIDTH, rowPitch);
        const Pixel middleEdge = Sample(pixels, WIDTH / 2, 1, WIDTH, rowPitch);

        REQUIRE(corner.a < 100);
        REQUIRE(middleEdge.a > 200);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
