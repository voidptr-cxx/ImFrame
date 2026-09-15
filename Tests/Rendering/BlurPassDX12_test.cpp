/**
 * @file     BlurPassDX12_test.cpp
 * @brief    Real-pixel tests for `BlurPassDX12`'s compute-shader Gaussian blur (Phase 35.11)
 *
 * @internal
 * Mirrors `BlurPassVulkan_test.cpp`'s own test scenarios (no-op below radius 0, an impulse pixel
 * spreading into its neighbours, ping-pong target reuse at a stable size) against the D3D12
 * compute-shader implementation instead. `ScratchStorageResource` plays the same combined
 * upload+readback role `BlurPassVulkan_test.cpp`'s own `ScratchStorageImage` does — `Apply()`'s
 * source and result are the same kind of resource (a `DXGI_FORMAT_R8G8B8A8_UNORM` UAV-capable
 * texture), so one class here serves both roles.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-15
 * @version  3.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3DX12/BlurPassDX12.hpp"
#include "Backends/SDL3DX12/SDL3DX12Backend.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

constexpr int WIDTH  = 32;
constexpr int HEIGHT = 32;
constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "BlurPassDX12_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {static_cast<int>(pixels[off + 0]), static_cast<int>(pixels[off + 1]), static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3])};
}

/// A `DXGI_FORMAT_R8G8B8A8_UNORM` UAV-capable texture usable both as `BlurPassDX12::Apply()`'s
/// source (already in `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`, per its own documented
/// precondition) and as a way to read back any `BlurResult` (including `Apply()`'s own return
/// value, which shares this same format/state contract) — see this file's own header comment.
class ScratchStorageResource {
public:
    ScratchStorageResource(ID3D12Device4* device, ID3D12CommandQueue* queue, int width, int height)
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
        desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&_resource));

        _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));
        _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                    IID_PPV_ARGS(&_cmdList));
        _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
        _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    ~ScratchStorageResource() { if (_fenceEvent) { CloseHandle(_fenceEvent); } }

    ScratchStorageResource(const ScratchStorageResource&)            = delete;
    ScratchStorageResource& operator=(const ScratchStorageResource&) = delete;

    [[nodiscard]] BlurResult Id() const { return {_resource.Get()}; }

    /// Transparent black everywhere except one fully-opaque white pixel at the exact center --
    /// known content to assert the blur spreads it. Leaves the resource in
    /// `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`, the state `BlurPassDX12::Apply()` requires of its
    /// source.
    void UploadImpulse() {
        std::vector<std::byte> pixels(static_cast<std::size_t>(_width) * _height * 4, std::byte{0});
        const std::size_t centerOff = (static_cast<std::size_t>(_height / 2) * _width + _width / 2) * 4;
        pixels[centerOff + 0] = std::byte{255};
        pixels[centerOff + 1] = std::byte{255};
        pixels[centerOff + 2] = std::byte{255};
        pixels[centerOff + 3] = std::byte{255};
        Upload(pixels);
    }

    void Upload(const std::vector<std::byte>& pixels) {
        constexpr std::size_t kRowPitchAlignment = 256;
        const std::size_t rowPitch = (static_cast<std::size_t>(_width) * 4 + kRowPitchAlignment - 1) &
                                     ~(kRowPitchAlignment - 1);
        const std::size_t uploadSize = rowPitch * static_cast<std::size_t>(_height);

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width            = static_cast<UINT64>(uploadSize);
        bufDesc.Height           = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels        = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuf;
        _device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf));

        void* mapped = nullptr;
        const D3D12_RANGE noRead{0, 0};
        uploadBuf->Map(0, &noRead, &mapped);
        for (int y = 0; y < _height; ++y) {
            std::memcpy(static_cast<std::byte*>(mapped) + static_cast<std::size_t>(y) * rowPitch,
                       pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) * 4,
                       static_cast<std::size_t>(_width) * 4);
        }
        uploadBuf->Unmap(0, nullptr);

        _cmdAlloc->Reset();
        _cmdList->Reset(_cmdAlloc.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toCopyDst{};
        toCopyDst.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopyDst.Transition.pResource   = _resource.Get();
        toCopyDst.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toCopyDst.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        toCopyDst.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &toCopyDst);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = _resource.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource                          = uploadBuf.Get();
        srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Footprint.Format   = kColorFormat;
        srcLoc.PlacedFootprint.Footprint.Width    = static_cast<UINT>(_width);
        srcLoc.PlacedFootprint.Footprint.Height   = static_cast<UINT>(_height);
        srcLoc.PlacedFootprint.Footprint.Depth    = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

        _cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        // Back to UNORDERED_ACCESS -- the state BlurPassDX12::Apply() requires of its source (see
        // that class's own .hpp comment).
        D3D12_RESOURCE_BARRIER toUav{};
        toUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toUav.Transition.pResource   = _resource.Get();
        toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        toUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &toUav);

        SubmitAndWait();
    }

    /// Reads back an arbitrary `BlurResult` (not necessarily this instance's own resource) --
    /// `BlurPassDX12::Apply()`'s return value shares the identical format/state contract this
    /// class's own resource does. Leaves `result`'s resource back in
    /// `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` in case it is `Apply()`-ed again (it is
    /// `BlurPassDX12`-owned, so this matters if the same `BlurPassDX12` is reused after a
    /// read-back).
    [[nodiscard]] std::vector<std::byte> ReadPixels(BlurResult result) {
        constexpr std::size_t kRowPitchAlignment = 256;
        const std::size_t rowPitch =
            (static_cast<std::size_t>(_width) * 4 + kRowPitchAlignment - 1) & ~(kRowPitchAlignment - 1);
        const std::size_t bufferSize = rowPitch * static_cast<std::size_t>(_height);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width            = static_cast<UINT64>(bufferSize);
        bufDesc.Height           = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels        = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                         nullptr, IID_PPV_ARGS(&readback));

        _cmdAlloc->Reset();
        _cmdList->Reset(_cmdAlloc.Get(), nullptr);

        D3D12_RESOURCE_BARRIER toCopySrc{};
        toCopySrc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopySrc.Transition.pResource   = result.Resource;
        toCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toCopySrc.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopySrc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &toCopySrc);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource        = result.Resource;
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

        D3D12_RESOURCE_BARRIER backToUav{};
        backToUav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backToUav.Transition.pResource   = result.Resource;
        backToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        backToUav.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        backToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _cmdList->ResourceBarrier(1, &backToUav);

        SubmitAndWait();

        void* mapped = nullptr;
        const D3D12_RANGE readRange{0, static_cast<SIZE_T>(bufferSize)};
        readback->Map(0, &readRange, &mapped);
        std::vector<std::byte> tightPixels(static_cast<std::size_t>(_width) * _height * 4);
        for (int y = 0; y < _height; ++y) {
            std::memcpy(tightPixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) * 4,
                       static_cast<const std::byte*>(mapped) + static_cast<std::size_t>(y) * rowPitch,
                       static_cast<std::size_t>(_width) * 4);
        }
        readback->Unmap(0, nullptr);

        return tightPixels;
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

    ID3D12Device4*      _device = nullptr;
    ID3D12CommandQueue* _queue  = nullptr;
    int                 _width;
    int                 _height;

    Microsoft::WRL::ComPtr<ID3D12Resource>            _resource;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList;
    Microsoft::WRL::ComPtr<ID3D12Fence>                _fence;
    std::uint64_t                                      _fenceValue = 0;
    HANDLE                                              _fenceEvent = nullptr;
};

} // namespace

TEST_CASE("BlurPassDX12 with radius <= 0 returns the source unchanged", "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageResource source(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassDX12 blur(handles.Device, handles.DirectQueue);

        const BlurResult noOp1 = blur.Apply(source.Id(), WIDTH, HEIGHT, 0.0f);
        const BlurResult noOp2 = blur.Apply(source.Id(), WIDTH, HEIGHT, -5.0f);
        REQUIRE(noOp1.Resource == source.Id().Resource);
        REQUIRE(noOp2.Resource == source.Id().Resource);

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassDX12 spreads a single opaque pixel into its neighbours", "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageResource source(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassDX12 blur(handles.Device, handles.DirectQueue);

        const BlurResult blurred = blur.Apply(source.Id(), WIDTH, HEIGHT, 6.0f);
        REQUIRE(blurred.Resource != source.Id().Resource);

        auto pixels = source.ReadPixels(blurred);
        const Pixel center    = Sample(pixels, WIDTH / 2, HEIGHT / 2, WIDTH);
        const Pixel neighbour = Sample(pixels, WIDTH / 2 + 3, HEIGHT / 2, WIDTH);
        const Pixel farAway   = Sample(pixels, 2, 2, WIDTH);

        // The center pixel's own energy spread out, so it's dimmer than the original impulse --
        // but a real Gaussian kernel still leaves it the single brightest point in the result.
        REQUIRE(center.a > 0);
        REQUIRE(center.a < 255);
        REQUIRE(center.a > neighbour.a);

        // A few pixels away, some of the impulse's energy has spread there -- it's no longer
        // exactly zero the way it was in the unblurred source.
        REQUIRE(neighbour.a > 0);

        // Far from the impulse (well outside a radius-6 kernel's reach), nothing spread there.
        REQUIRE(farAway.a == 0);

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassDX12 reuses ping-pong targets across repeated calls at the same size", "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageResource source(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassDX12 blur(handles.Device, handles.DirectQueue);

        const BlurResult first  = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        const BlurResult second = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        REQUIRE(first.Resource == second.Resource); // same target resource reused, not reallocated

        blur.Shutdown();
    }

    backend.Shutdown();
}
