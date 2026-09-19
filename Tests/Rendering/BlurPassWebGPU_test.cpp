/**
 * @file     BlurPassWebGPU_test.cpp
 * @brief    Real-pixel tests for `BlurPassWebGPU`'s compute-shader Gaussian blur (Phase 35.15)
 *
 * @internal
 * Mirrors `BlurPassVulkan_test.cpp`'s/`BlurPassDX12_test.cpp`'s own test scenarios (no-op below
 * radius 0, an impulse pixel spreading into its neighbours, ping-pong target reuse at a stable
 * size) against the WebGPU compute-shader implementation instead. `ReadPixels()` is a free
 * function taking a `BlurResult` directly (not a method on a scratch-texture helper) since a
 * `BlurResult` already carries its own `WGPUTexture` (see `BlurPassWebGPU.hpp`'s own file comment
 * on why `Texture` is needed alongside `View`) — no separate "read back someone else's resource"
 * indirection is needed the way `BlurPassDX12_test.cpp`'s own `ScratchStorageResource::ReadPixels()`
 * needs for a raw `ID3D12Resource*`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-19
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/DawnWebGPU/BlurPassWebGPU.hpp"
#include "Backends/DawnWebGPU/DawnWebGPUBackend.hpp"

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
constexpr WGPUTextureFormat kColorFormat = WGPUTextureFormat_RGBA8Unorm;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "BlurPassWebGPU_test";
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

/// A storage-binding-capable `WGPUTextureFormat_RGBA8Unorm` texture usable as
/// `BlurPassWebGPU::Apply()`'s source. Upload via `wgpuQueueWriteTexture()` (no staging buffer
/// needed, matching `NativeRendererWebGPU_test.cpp`'s own `ScratchTexture` finding).
class ScratchStorageTexture {
public:
    ScratchStorageTexture(WGPUDevice device, WGPUQueue queue, int width, int height)
        : _device(device), _queue(queue), _width(width), _height(height) {
        WGPUTextureDescriptor texDesc{};
        texDesc.usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding |
                       WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
        texDesc.dimension     = WGPUTextureDimension_2D;
        texDesc.size          = WGPUExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        texDesc.format        = kColorFormat;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount   = 1;
        _texture               = wgpuDeviceCreateTexture(_device, &texDesc);

        WGPUTextureViewDescriptor viewDesc{};
        viewDesc.format          = kColorFormat;
        viewDesc.dimension       = WGPUTextureViewDimension_2D;
        viewDesc.mipLevelCount   = 1;
        viewDesc.arrayLayerCount = 1;
        _view                     = wgpuTextureCreateView(_texture, &viewDesc);
    }

    ~ScratchStorageTexture() {
        if (_view) { wgpuTextureViewRelease(_view); }
        if (_texture) { wgpuTextureRelease(_texture); }
    }

    ScratchStorageTexture(const ScratchStorageTexture&)            = delete;
    ScratchStorageTexture& operator=(const ScratchStorageTexture&) = delete;

    [[nodiscard]] BlurResult Id() const { return {_texture, _view}; }

    /// Transparent black everywhere except one fully-opaque white pixel at the exact center --
    /// known content to assert the blur spreads it.
    void UploadImpulse() {
        std::vector<std::byte> pixels(static_cast<std::size_t>(_width) * _height * 4, std::byte{0});
        const std::size_t centerOff = (static_cast<std::size_t>(_height / 2) * _width + _width / 2) * 4;
        pixels[centerOff + 0] = std::byte{255};
        pixels[centerOff + 1] = std::byte{255};
        pixels[centerOff + 2] = std::byte{255};
        pixels[centerOff + 3] = std::byte{255};

        WGPUTexelCopyTextureInfo dst{};
        dst.texture = _texture;
        dst.aspect   = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout dataLayout{};
        dataLayout.bytesPerRow   = static_cast<uint32_t>(_width) * 4;
        dataLayout.rowsPerImage = static_cast<uint32_t>(_height);

        const WGPUExtent3D writeSize{static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), 1};
        wgpuQueueWriteTexture(_queue, &dst, pixels.data(), pixels.size(), &dataLayout, &writeSize);
        // No fence/wait needed -- WebGPU's sequential submit model guarantees this write completes
        // before the next submit on this queue (whichever test calls Apply() next).
    }

private:
    WGPUDevice _device = nullptr;
    WGPUQueue  _queue  = nullptr;
    int        _width;
    int        _height;
    WGPUTexture     _texture = nullptr;
    WGPUTextureView _view    = nullptr;
};

/// Reads back any `BlurResult` (this test's own source, or `BlurPassWebGPU::Apply()`'s returned
/// result) via `wgpuCommandEncoderCopyTextureToBuffer()` + `wgpuInstanceWaitAny()`, matching
/// `NativeRendererWebGPU_test.cpp`'s own `ScratchTarget::ReadPixels()` pattern -- de-strides
/// WebGPU's own 256-byte-row-pitch-aligned copy destination.
std::vector<std::byte> ReadPixels(WGPUDevice device, WGPUQueue queue, WGPUInstance instance, BlurResult result,
                                  int width, int height) {
    constexpr std::size_t kRowPitchAlignment = 256;
    const std::size_t bytesPerRow =
        (static_cast<std::size_t>(width) * 4 + kRowPitchAlignment - 1) & ~(kRowPitchAlignment - 1);
    const std::size_t bufferSize = bytesPerRow * static_cast<std::size_t>(height);

    WGPUBufferDescriptor readbackDesc{};
    readbackDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    readbackDesc.size  = bufferSize;
    WGPUBuffer readback = wgpuDeviceCreateBuffer(device, &readbackDesc);

    WGPUCommandEncoderDescriptor encDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encDesc);

    WGPUTexelCopyTextureInfo src{};
    src.texture = result.Texture;
    src.aspect   = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dst{};
    dst.buffer               = readback;
    dst.layout.bytesPerRow   = static_cast<uint32_t>(bytesPerRow);
    dst.layout.rowsPerImage = static_cast<uint32_t>(height);

    const WGPUExtent3D copySize{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &copySize);

    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);

    struct MapDoneFlag { bool Done = false; bool Ok = false; };
    MapDoneFlag mapDone{};
    WGPUBufferMapCallbackInfo mapCb{};
    mapCb.mode     = WGPUCallbackMode_WaitAnyOnly;
    mapCb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        auto* flag = static_cast<MapDoneFlag*>(userdata1);
        flag->Done = true;
        flag->Ok   = (status == WGPUMapAsyncStatus_Success);
    };
    mapCb.userdata1 = &mapDone;

    WGPUFuture future = wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, bufferSize, mapCb);
    WGPUFutureWaitInfo waitInfo{};
    waitInfo.future = future;
    wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);

    std::vector<std::byte> tightPixels;
    if (mapDone.Ok) {
        const void* mapped = wgpuBufferGetConstMappedRange(readback, 0, bufferSize);
        if (mapped) {
            const auto* src8 = static_cast<const std::byte*>(mapped);
            const std::size_t tightRowBytes = static_cast<std::size_t>(width) * 4;
            tightPixels.resize(tightRowBytes * static_cast<std::size_t>(height));
            for (int y = 0; y < height; ++y) {
                std::memcpy(tightPixels.data() + static_cast<std::size_t>(y) * tightRowBytes,
                            src8 + static_cast<std::size_t>(y) * bytesPerRow, tightRowBytes);
            }
        }
        wgpuBufferUnmap(readback);
    }

    wgpuBufferRelease(readback);
    return tightPixels;
}

} // namespace

TEST_CASE("BlurPassWebGPU with radius <= 0 returns the source unchanged", "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageTexture source(handles.Device, handles.Queue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassWebGPU blur(handles.Device, handles.Queue);

        const BlurResult noOp1 = blur.Apply(source.Id(), WIDTH, HEIGHT, 0.0f);
        const BlurResult noOp2 = blur.Apply(source.Id(), WIDTH, HEIGHT, -5.0f);
        REQUIRE(noOp1.View == source.Id().View);
        REQUIRE(noOp2.View == source.Id().View);

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassWebGPU spreads a single opaque pixel into its neighbours", "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageTexture source(handles.Device, handles.Queue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassWebGPU blur(handles.Device, handles.Queue);

        const BlurResult blurred = blur.Apply(source.Id(), WIDTH, HEIGHT, 6.0f);
        REQUIRE(blurred.View != source.Id().View);

        auto pixels = ReadPixels(handles.Device, handles.Queue, handles.Instance, blurred, WIDTH, HEIGHT);
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

TEST_CASE("BlurPassWebGPU reuses ping-pong targets across repeated calls at the same size", "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchStorageTexture source(handles.Device, handles.Queue, WIDTH, HEIGHT);
        source.UploadImpulse();

        BlurPassWebGPU blur(handles.Device, handles.Queue);

        const BlurResult first  = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        const BlurResult second = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        REQUIRE(first.View == second.View); // same target view reused, not reallocated

        blur.Shutdown();
    }

    backend.Shutdown();
}
