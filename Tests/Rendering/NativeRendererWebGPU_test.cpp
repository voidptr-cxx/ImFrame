/**
 * @file     NativeRendererWebGPU_test.cpp
 * @brief    Real-pixel tests for NativeRendererWebGPU's DrawRect rendering (Phase 35.8)
 *
 * @internal
 * Mirrors `NativeRendererDX12_test.cpp`'s own pattern (a hand-created offscreen render target, no
 * `Application`/`Viewport` machinery) but for WebGPU: a self-contained `ScratchTarget` allocates a
 * `WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc` texture + its own view, clears it
 * up front via a real render pass (`WGPULoadOp_Clear` — simpler than Vulkan/DX12's own separate
 * clear-command-buffer step), then reads it back via a one-shot command encoder copying into a
 * `WGPUBufferUsage_MapRead` buffer, synchronously mapped via `wgpuInstanceWaitAny()` — the same
 * pattern `DawnWebGPUBackend::ReadPixels()` itself already uses for its own headless readback.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-14
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/DawnWebGPU/DawnWebGPUBackend.hpp"
#include "Backends/DawnWebGPU/NativeRendererWebGPU.hpp"

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
constexpr WGPUTextureFormat kColorFormat = WGPUTextureFormat_RGBA8Unorm;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "NativeRendererWebGPU_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
    return {static_cast<int>(pixels[off + 0]), static_cast<int>(pixels[off + 1]), static_cast<int>(pixels[off + 2]),
            static_cast<int>(pixels[off + 3])};
}

/// A small, self-contained offscreen render target — see this file's own header comment for why a
/// hand-rolled target is used instead of the backend's real swap chain (matches
/// `NativeRendererDX12_test.cpp`'s identical reasoning for its own `ScratchTarget`).
class ScratchTarget {
public:
    ScratchTarget(WGPUDevice device, WGPUQueue queue, WGPUInstance instance, uint32_t width, uint32_t height)
        : _device(device), _queue(queue), _instance(instance), _width(width), _height(height) {
        WGPUTextureDescriptor texDesc{};
        texDesc.usage         = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        texDesc.dimension     = WGPUTextureDimension_2D;
        texDesc.size          = WGPUExtent3D{width, height, 1};
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

        ClearToTransparent();
    }

    ~ScratchTarget() {
        if (_view) { wgpuTextureViewRelease(_view); }
        if (_texture) { wgpuTextureRelease(_texture); }
    }

    ScratchTarget(const ScratchTarget&)            = delete;
    ScratchTarget& operator=(const ScratchTarget&) = delete;

    [[nodiscard]] WGPUTextureView View() const noexcept { return _view; }

    /// Returns tightly-packed-row RGBA8 pixels — internally de-strides WebGPU's own
    /// 256-byte-row-pitch-aligned copy destination (same alignment requirement
    /// `NativeRendererDX12_test.cpp`'s own `ScratchTarget::ReadPixels()` de-strides), so callers
    /// can index with a plain `width * 4` stride via `Sample()`.
    [[nodiscard]] std::vector<std::byte> ReadPixels() const {
        constexpr std::size_t kRowPitchAlignment = 256;
        const std::size_t bytesPerRow =
            (static_cast<std::size_t>(_width) * 4 + kRowPitchAlignment - 1) & ~(kRowPitchAlignment - 1);
        const std::size_t bufferSize = bytesPerRow * static_cast<std::size_t>(_height);

        WGPUBufferDescriptor readbackDesc{};
        readbackDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        readbackDesc.size  = bufferSize;
        WGPUBuffer readback = wgpuDeviceCreateBuffer(_device, &readbackDesc);

        WGPUCommandEncoderDescriptor encDesc{};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

        WGPUTexelCopyTextureInfo src{};
        src.texture = _texture;
        src.aspect   = WGPUTextureAspect_All;

        WGPUTexelCopyBufferInfo dst{};
        dst.buffer               = readback;
        dst.layout.bytesPerRow   = static_cast<uint32_t>(bytesPerRow);
        dst.layout.rowsPerImage = _height;

        const WGPUExtent3D copySize{_width, _height, 1};
        wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &copySize);

        WGPUCommandBufferDescriptor cbDesc{};
        WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuCommandEncoderRelease(encoder);
        wgpuQueueSubmit(_queue, 1, &cmdBuf);
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
        wgpuInstanceWaitAny(_instance, 1, &waitInfo, UINT64_MAX);

        std::vector<std::byte> tightPixels;
        if (mapDone.Ok) {
            const void* mapped = wgpuBufferGetConstMappedRange(readback, 0, bufferSize);
            if (mapped) {
                const auto* src8 = static_cast<const std::byte*>(mapped);
                const std::size_t tightRowBytes = static_cast<std::size_t>(_width) * 4;
                tightPixels.resize(tightRowBytes * static_cast<std::size_t>(_height));
                for (uint32_t y = 0; y < _height; ++y) {
                    std::memcpy(tightPixels.data() + static_cast<std::size_t>(y) * tightRowBytes,
                                src8 + static_cast<std::size_t>(y) * bytesPerRow, tightRowBytes);
                }
            }
            wgpuBufferUnmap(readback);
        }

        wgpuBufferRelease(readback);
        return tightPixels;
    }

private:
    void ClearToTransparent() {
        WGPUCommandEncoderDescriptor encDesc{};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

        WGPURenderPassColorAttachment colorAttachment{};
        colorAttachment.view       = _view;
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.loadOp      = WGPULoadOp_Clear;
        colorAttachment.storeOp     = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};

        WGPURenderPassDescriptor passDesc{};
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments      = &colorAttachment;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);

        WGPUCommandBufferDescriptor cbDesc{};
        WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuCommandEncoderRelease(encoder);
        wgpuQueueSubmit(_queue, 1, &cmdBuf);
        wgpuCommandBufferRelease(cmdBuf);
        // No fence/wait needed here -- WebGPU's sequential submit model guarantees this clear
        // completes before the next submit on this queue (the test's own Render() call), matching
        // ViewportWebGPU.cpp's/NativeRendererWebGPU.cpp's own identical finding.
    }

    WGPUDevice   _device   = nullptr;
    WGPUQueue    _queue    = nullptr;
    WGPUInstance _instance = nullptr;
    uint32_t     _width;
    uint32_t     _height;
    WGPUTexture     _texture = nullptr;
    WGPUTextureView _view    = nullptr;
};

} // namespace

TEST_CASE("NativeRendererWebGPU draws a filled DrawRect at the recorded position", "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {static_cast<float>(WIDTH) / 2.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel left  = Sample(pixels, WIDTH / 4, HEIGHT / 2, WIDTH);
        const Pixel right = Sample(pixels, WIDTH * 3 / 4, HEIGHT / 2, WIDTH);

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

TEST_CASE("NativeRendererWebGPU renders an off-center rect: asymmetric placement stays correct "
          "(catches a Y-axis-convention mismatch immediately, per Phase 35.2's own lesson)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        // A rect only in the TOP-left quadrant -- deliberately asymmetric in Y, so a Y-flip bug
        // (like Phase 35.2's own Vulkan one) would show up as content in the wrong half instead of
        // passing by coincidence.
        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 20.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels      = target.ReadPixels();
        const Pixel insideRect = Sample(pixels, 20, 20, WIDTH);  // inside the rect's own footprint
        const Pixel belowRect  = Sample(pixels, 20, 100, WIDTH); // well below it -- untouched

        REQUIRE(insideRect.r > 200);
        REQUIRE(insideRect.a > 200);
        REQUIRE(belowRect.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU renders rounded corners: the extreme corner pixel stays outside the fill",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .Radii    = CornerRadii::All(24.0f),
            .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels      = target.ReadPixels();
        const Pixel corner     = Sample(pixels, 1, 1, WIDTH);
        const Pixel middleEdge = Sample(pixels, WIDTH / 2, 1, WIDTH);

        REQUIRE(corner.a < 100);
        REQUIRE(middleEdge.a > 200);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
