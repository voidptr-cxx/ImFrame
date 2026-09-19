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
 * Phase 35.9 adds `ScratchTexture` and two `DrawImage` tests, mirroring
 * `NativeRendererVulkan_test.cpp`'s own Phase 35.2 `ScratchTexture`/tests: a small
 * `D3D12_HEAP_TYPE_DEFAULT` texture uploaded via a `D3D12_HEAP_TYPE_UPLOAD` staging buffer, then
 * transitioned to `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` — the state
 * `NativeRendererDX12::RenderImageBatch()`'s SRV expects, matching
 * `NativeRendererVulkan`'s own `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` contract.
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

/// `NativeRendererDX12` treats `DrawImage::Texture`'s value as a raw `ID3D12Resource*` (see
/// `NativeRendererDX12.hpp`'s own file comment), so this real texture resource's pointer is what
/// gets pushed. Uploads via a staging buffer, then transitions to
/// `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` -- the state `RenderImageBatch()`'s SRV expects.
class ScratchTexture {
public:
    ScratchTexture(ID3D12Device4* device, ID3D12CommandQueue* queue, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::uint8_t a)
        : _device(device), _queue(queue) {
        constexpr int kSize = 8;
        std::vector<std::byte> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = std::byte{r};
            pixels[i + 1] = std::byte{g};
            pixels[i + 2] = std::byte{b};
            pixels[i + 3] = std::byte{a};
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width            = kSize;
        texDesc.Height           = kSize;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels        = 1;
        texDesc.Format           = kColorFormat;
        texDesc.SampleDesc.Count = 1;

        _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                         nullptr, IID_PPV_ARGS(&_resource));

        UploadAndTransition(pixels, kSize);
    }

    ScratchTexture(const ScratchTexture&)            = delete;
    ScratchTexture& operator=(const ScratchTexture&) = delete;

    [[nodiscard]] TextureId Id() const {
        return TextureId(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(_resource.Get())));
    }

private:
    void UploadAndTransition(const std::vector<std::byte>& pixels, int size) {
        constexpr std::size_t kRowPitchAlignment = 256;
        const std::size_t rowPitch = (static_cast<std::size_t>(size) * 4 + kRowPitchAlignment - 1) &
                                      ~(kRowPitchAlignment - 1);
        const std::size_t uploadSize = rowPitch * static_cast<std::size_t>(size);

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
        for (int y = 0; y < size; ++y) {
            std::memcpy(static_cast<std::byte*>(mapped) + static_cast<std::size_t>(y) * rowPitch,
                       pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(size) * 4,
                       static_cast<std::size_t>(size) * 4);
        }
        uploadBuf->Unmap(0, nullptr);

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    cmdAlloc;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
        _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
        _device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                    IID_PPV_ARGS(&cmdList));
        cmdList->Reset(cmdAlloc.Get(), nullptr);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = _resource.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource                          = uploadBuf.Get();
        srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Footprint.Format   = kColorFormat;
        srcLoc.PlacedFootprint.Footprint.Width    = static_cast<UINT>(size);
        srcLoc.PlacedFootprint.Footprint.Height   = static_cast<UINT>(size);
        srcLoc.PlacedFootprint.Footprint.Depth    = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_RESOURCE_BARRIER toShaderResource{};
        toShaderResource.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toShaderResource.Transition.pResource   = _resource.Get();
        toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        toShaderResource.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &toShaderResource);

        cmdList->Close();
        ID3D12CommandList* lists[] = {cmdList.Get()};
        _queue->ExecuteCommandLists(1, lists);

        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        _queue->Signal(fence.Get(), 1);
        if (fence->GetCompletedValue() < 1) {
            fence->SetEventOnCompletion(1, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        CloseHandle(fenceEvent);
    }

    ID3D12Device4*      _device = nullptr;
    ID3D12CommandQueue* _queue  = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
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

TEST_CASE("NativeRendererDX12 draws a DrawImage at the recorded position, sampling the bound "
          "ID3D12Resource and applying TintColor (Phase 35.9)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);
        ScratchTexture texture(handles.Device, handles.DirectQueue, 0, 0, 255, 255);

        CommandBuffer buffer;
        buffer.Push(DrawImage{
            .Position  = {0.0f, 0.0f},
            .Size      = {64.0f, 64.0f},
            .Texture   = texture.Id(),
            .TintColor = {0.5f, 1.0f, 1.0f, 1.0f}});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel inside  = Sample(pixels, 32, 32, WIDTH, rowPitch);
        const Pixel outside = Sample(pixels, WIDTH - 4, HEIGHT - 4, WIDTH, rowPitch);

        REQUIRE(inside.b > 200);
        REQUIRE(inside.r < 150); // TintColor.r == 0.5 darkens the source texture's zero red further
        REQUIRE(inside.a > 200);
        REQUIRE(outside.a == 0); // untouched -- still the target's transparent clear colour

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 renders a Rect and two differently-textured Image batches "
          "correctly in the same frame (Phase 35.9)",
          "[dx12]") {
    // Directly validates the per-batch SRV-heap scheme (see NativeRendererDX12.hpp's own file
    // comment): a single reused SRV slot would make every draw in this frame sample whichever
    // texture was written *last*, once all these commands actually execute on the GPU (recording
    // all of them happens before any of them run) -- mirrors
    // NativeRendererVulkan_test.cpp's own identical Phase 35.2 test.
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);
        ScratchTexture redTexture(handles.Device, handles.DirectQueue, 255, 0, 0, 255);
        ScratchTexture greenTexture(handles.Device, handles.DirectQueue, 0, 255, 0, 255);

        CommandBuffer buffer;
        buffer.Push(DrawRect{.Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}, .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawImage{.Position = {48.0f, 0.0f}, .Size = {32.0f, 32.0f}, .Texture = redTexture.Id()});
        buffer.Push(DrawImage{.Position = {96.0f, 96.0f}, .Size = {32.0f, 32.0f}, .Texture = greenTexture.Id()});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel rect  = Sample(pixels, 16, 16, WIDTH, rowPitch);
        const Pixel red   = Sample(pixels, 64, 16, WIDTH, rowPitch);
        const Pixel green = Sample(pixels, 112, 112, WIDTH, rowPitch);

        REQUIRE(rect.b > 200);
        REQUIRE(rect.a > 200);
        REQUIRE(red.r > 200);
        REQUIRE(red.g < 50);
        REQUIRE(green.g > 200);
        REQUIRE(green.r < 50);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 renders a DrawShadow behind and offset from the shape it shadows "
          "(Phase 35.12)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // Shadow first (drawn behind), then an opaque white rect at the same (unshifted) position
        // and size on top -- per PHASE_34_PROPOSAL.md's DrawShadow section ("composite it behind
        // the shape at the specified offset"), the same scenario NativeRendererVulkan_test.cpp's
        // own Phase 35.4 test covers.
        buffer.Push(DrawShadow{
            .Position = {40.0f, 40.0f},
            .Size = {48.0f, 48.0f},
            .BlurRadius = 8.0f,
            .Offset = {10.0f, 10.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(DrawRect{
            .Position = {40.0f, 40.0f}, .Size = {48.0f, 48.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        // Deep inside the shadow's offset footprint (x,y in [50,98] before blur padding) but past
        // the white rect's own edge (rect ends at x=88, y=88) -- the shadow should be visible here,
        // not occluded.
        const Pixel shadowOnly = Sample(pixels, 94, 94, WIDTH, rowPitch);
        // Deep inside the rect's own footprint -- drawn after the shadow, so it occludes it.
        const Pixel rectOnTop = Sample(pixels, 60, 60, WIDTH, rowPitch);
        // Far from both the rect and the shadow's shifted+blurred footprint -- untouched.
        const Pixel untouched = Sample(pixels, 10, 10, WIDTH, rowPitch);

        REQUIRE(shadowOnly.a > 100);
        REQUIRE(shadowOnly.r < 50); // ShadowColor is opaque black
        REQUIRE(rectOnTop.r > 200);
        REQUIRE(rectOnTop.a > 200);
        REQUIRE(untouched.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 composites a PushOpacityLayer at the recorded opacity (Phase 35.13)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel inside  = Sample(pixels, 40, 40, WIDTH, rowPitch);
        const Pixel outside = Sample(pixels, 5, 5, WIDTH, rowPitch);

        // Correct premultiplied-alpha compositing: an opaque red rect at Opacity=0.5 ends up with
        // both its alpha AND its stored (premultiplied) red channel scaled to roughly half -- the
        // same scenario NativeRendererVulkan_test.cpp's own Phase 35.5 test covers.
        REQUIRE(inside.a > 100);
        REQUIRE(inside.a < 150);
        REQUIRE(inside.r > 100);
        REQUIRE(inside.r < 150);
        REQUIRE(inside.g < 20);
        REQUIRE(outside.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 composites a PushBlendLayer using the Multiply formula (Phase 35.13)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // Opaque light-gray backdrop filling the whole viewport, then a Multiply layer with an
        // opaque mid-gray rect over part of it -- the same scenario
        // NativeRendererVulkan_test.cpp's own Phase 35.5 test covers.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        const Pixel overlap      = Sample(pixels, 40, 40, WIDTH, rowPitch); // inside the blended rect
        const Pixel backdropOnly = Sample(pixels, 5, 5, WIDTH, rowPitch); // outside it -- backdrop untouched

        // Multiply(0.8, 0.5) = 0.4 -> ~102/255. Backdrop-only area stays 0.8 -> ~204/255.
        REQUIRE(overlap.r > 90);
        REQUIRE(overlap.r < 115);
        REQUIRE(backdropOnly.r > 190);
        REQUIRE(backdropOnly.r < 215);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 renders a DrawBackdropBlur: blends across a colour seam within its own "
          "rect, leaves everything outside untouched (Phase 35.14)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // A hard horizontal colour seam at y=64: red above, blue below. Deliberately asymmetric in
        // Y (not a uniform fill) so a Y-orientation bug would show up as a wrong-side blend instead
        // of passing by coincidence -- the same scenario NativeRendererVulkan_test.cpp's own Phase
        // 35.6 test covers.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawBackdropBlur{.Position = {40.0f, 44.0f}, .Size = {48.0f, 40.0f}, .BlurRadius = 10.0f});

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        // Exactly at the seam, well inside the blur rect (x in [40,88], y in [44,84]) -- a real
        // blur straddling red-above/blue-below should show a roughly even mix of both.
        const Pixel atSeam = Sample(pixels, 64, 64, WIDTH, rowPitch);
        // Just outside the blur rect's own top edge (y=40 < 44) but inside its padded copy region
        // (padding = BlurRadius = 10, so the copy reaches up to y=34) -- proves the composite was
        // cropped to the requested Size, not left showing the padding's own blurred bleed.
        const Pixel justAboveRect = Sample(pixels, 64, 40, WIDTH, rowPitch);
        // Far from the blur rect and the seam entirely -- untouched original colours.
        const Pixel untouchedRed  = Sample(pixels, 10, 10, WIDTH, rowPitch);
        const Pixel untouchedBlue = Sample(pixels, 10, 118, WIDTH, rowPitch);

        REQUIRE(atSeam.r > 80);
        REQUIRE(atSeam.r < 180);
        REQUIRE(atSeam.b > 80);
        REQUIRE(atSeam.b < 180);

        REQUIRE(justAboveRect.r > 200);
        REQUIRE(justAboveRect.b < 20);

        REQUIRE(untouchedRed.r > 200);
        REQUIRE(untouchedRed.b < 20);
        REQUIRE(untouchedBlue.b > 200);
        REQUIRE(untouchedBlue.r < 20);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererDX12 rate-limits DrawBackdropBlur at MaxBackdropBlurPerFrame, degrading "
          "gracefully past the limit (Phase 35.14)",
          "[dx12]") {
    SDL3DX12Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.DirectQueue, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        // Five non-overlapping backdrop-blur regions straddling the same seam -- default
        // MaxBackdropBlurPerFrame is 4, so the 5th should be skipped entirely.
        for (int i = 0; i < 5; ++i) {
            buffer.Push(DrawBackdropBlur{
                .Position = {10.0f + static_cast<float>(i) * 20.0f, 54.0f}, .Size = {16.0f, 20.0f}, .BlurRadius = 8.0f});
        }

        NativeRendererDX12 renderer(handles.Device, handles.DirectQueue, kColorFormat);
        renderer.SetTarget(target.Resource(), target.Rtv(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        auto [pixels, rowPitch] = target.ReadPixels();
        // y=59 is 5px above the seam (y=64), inside every region's own Y range [54,74] but nowhere
        // near the geometric seam itself -- a processed (blurred) region bleeds some blue this far
        // into the red band; a skipped region leaves this pixel exactly the original pure red.
        for (int i = 0; i < 4; ++i) {
            const int x = 10 + i * 20 + 8; // center-x of region i
            const Pixel processed = Sample(pixels, x, 59, WIDTH, rowPitch);
            REQUIRE(processed.b > 15);
        }
        const Pixel skipped = Sample(pixels, 10 + 4 * 20 + 8, 59, WIDTH, rowPitch);
        REQUIRE(skipped.b == 0);
        REQUIRE(skipped.r == 255);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
