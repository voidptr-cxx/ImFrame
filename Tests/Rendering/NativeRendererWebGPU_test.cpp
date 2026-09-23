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

    [[nodiscard]] WGPUTexture Texture() const noexcept { return _texture; }
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

/// `NativeRendererWebGPU` treats `DrawImage::Texture`'s value as a raw `WGPUTextureView` handle
/// (see `NativeRendererWebGPU.hpp`'s own file comment), so this real texture view's handle is what
/// gets pushed. Uploads via `wgpuQueueWriteTexture()` -- no manual staging buffer, copy command, or
/// layout transition needed, unlike `NativeRendererVulkan_test.cpp`'s/`NativeRendererDX12_test.cpp`'s
/// own `ScratchTexture` helpers.
class ScratchTexture {
public:
    ScratchTexture(WGPUDevice device, WGPUQueue queue, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::uint8_t a)
        : _device(device) {
        constexpr uint32_t kSize = 8;
        std::vector<std::byte> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = std::byte{r};
            pixels[i + 1] = std::byte{g};
            pixels[i + 2] = std::byte{b};
            pixels[i + 3] = std::byte{a};
        }

        WGPUTextureDescriptor texDesc{};
        texDesc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        texDesc.dimension     = WGPUTextureDimension_2D;
        texDesc.size          = WGPUExtent3D{kSize, kSize, 1};
        texDesc.format        = kColorFormat;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount   = 1;
        _texture               = wgpuDeviceCreateTexture(_device, &texDesc);

        WGPUTexelCopyTextureInfo dst{};
        dst.texture = _texture;
        dst.aspect   = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout dataLayout{};
        dataLayout.bytesPerRow   = kSize * 4;
        dataLayout.rowsPerImage = kSize;

        const WGPUExtent3D writeSize{kSize, kSize, 1};
        wgpuQueueWriteTexture(queue, &dst, pixels.data(), pixels.size(), &dataLayout, &writeSize);

        WGPUTextureViewDescriptor viewDesc{};
        viewDesc.format          = kColorFormat;
        viewDesc.dimension       = WGPUTextureViewDimension_2D;
        viewDesc.mipLevelCount   = 1;
        viewDesc.arrayLayerCount = 1;
        _view                     = wgpuTextureCreateView(_texture, &viewDesc);
    }

    ~ScratchTexture() {
        if (_view) { wgpuTextureViewRelease(_view); }
        if (_texture) { wgpuTextureRelease(_texture); }
    }

    ScratchTexture(const ScratchTexture&)            = delete;
    ScratchTexture& operator=(const ScratchTexture&) = delete;

    [[nodiscard]] TextureId Id() const {
        return TextureId(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(_view)));
    }

private:
    WGPUDevice      _device  = nullptr;
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
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
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
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
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
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
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

TEST_CASE("NativeRendererWebGPU draws a DrawImage at the recorded position, sampling the bound "
          "WGPUTextureView and applying TintColor (Phase 35.10)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);
        ScratchTexture texture(handles.Device, handles.Queue, 0, 0, 255, 255);

        CommandBuffer buffer;
        buffer.Push(DrawImage{
            .Position  = {0.0f, 0.0f},
            .Size      = {64.0f, 64.0f},
            .Texture   = texture.Id(),
            .TintColor = {0.5f, 1.0f, 1.0f, 1.0f}});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels  = target.ReadPixels();
        const Pixel inside  = Sample(pixels, 32, 32, WIDTH);
        const Pixel outside = Sample(pixels, WIDTH - 4, HEIGHT - 4, WIDTH);

        REQUIRE(inside.b > 200);
        REQUIRE(inside.r < 150); // TintColor.r == 0.5 darkens the source texture's zero red further
        REQUIRE(inside.a > 200);
        REQUIRE(outside.a == 0); // untouched -- still the target's transparent clear colour

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU renders a Rect and two differently-textured Image batches "
          "correctly in the same frame (Phase 35.10)",
          "[webgpu]") {
    // Directly validates the per-batch bind group scheme (see NativeRendererWebGPU.hpp's own file
    // comment): unlike Vulkan/DX12, a WGPUBindGroup is immutable once created, so this test mainly
    // confirms the *simpler* WebGPU design still produces the correct per-batch texture binding,
    // mirroring NativeRendererVulkan_test.cpp's/NativeRendererDX12_test.cpp's own identical tests.
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);
        ScratchTexture redTexture(handles.Device, handles.Queue, 255, 0, 0, 255);
        ScratchTexture greenTexture(handles.Device, handles.Queue, 0, 255, 0, 255);

        CommandBuffer buffer;
        buffer.Push(DrawRect{.Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}, .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawImage{.Position = {48.0f, 0.0f}, .Size = {32.0f, 32.0f}, .Texture = redTexture.Id()});
        buffer.Push(DrawImage{.Position = {96.0f, 96.0f}, .Size = {32.0f, 32.0f}, .Texture = greenTexture.Id()});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel rect  = Sample(pixels, 16, 16, WIDTH);
        const Pixel red   = Sample(pixels, 64, 16, WIDTH);
        const Pixel green = Sample(pixels, 112, 112, WIDTH);

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

TEST_CASE("NativeRendererWebGPU renders a DrawShadow behind and offset from the shape it shadows "
          "(Phase 35.16)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // Shadow first (drawn behind), then an opaque white rect at the same (unshifted) position
        // and size on top -- the same scenario NativeRendererVulkan_test.cpp's/
        // NativeRendererDX12_test.cpp's own Phase 35.4/35.12 tests cover.
        buffer.Push(DrawShadow{
            .Position = {40.0f, 40.0f},
            .Size = {48.0f, 48.0f},
            .BlurRadius = 8.0f,
            .Offset = {10.0f, 10.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(DrawRect{
            .Position = {40.0f, 40.0f}, .Size = {48.0f, 48.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        // Deep inside the shadow's offset footprint (x,y in [50,98] before blur padding) but past
        // the white rect's own edge (rect ends at x=88, y=88) -- the shadow should be visible here,
        // not occluded.
        const Pixel shadowOnly = Sample(pixels, 94, 94, WIDTH);
        // Deep inside the rect's own footprint -- drawn after the shadow, so it occludes it.
        const Pixel rectOnTop = Sample(pixels, 60, 60, WIDTH);
        // Far from both the rect and the shadow's shifted+blurred footprint -- untouched.
        const Pixel untouched = Sample(pixels, 10, 10, WIDTH);

        REQUIRE(shadowOnly.a > 100);
        REQUIRE(shadowOnly.r < 50); // ShadowColor is opaque black
        REQUIRE(rectOnTop.r > 200);
        REQUIRE(rectOnTop.a > 200);
        REQUIRE(untouched.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU composites a PushOpacityLayer at the recorded opacity (Phase 35.17)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel inside  = Sample(pixels, 40, 40, WIDTH);
        const Pixel outside = Sample(pixels, 5, 5, WIDTH);

        // Correct premultiplied-alpha compositing: an opaque red rect at Opacity=0.5 ends up with
        // both its alpha AND its stored (premultiplied) red channel scaled to roughly half -- the
        // same scenario NativeRendererVulkan_test.cpp's/NativeRendererDX12_test.cpp's own tests cover.
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

TEST_CASE("NativeRendererWebGPU composites a PushBlendLayer using the Multiply formula (Phase 35.17)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // Opaque light-gray backdrop filling the whole viewport, then a Multiply layer with an
        // opaque mid-gray rect over part of it. The rect sits in the TOP-left quadrant only, so a
        // vertically-mirrored fullscreen-quad UV table (Blend.hlsl's own Phase 35.13 bug) would
        // sample empty layer rows here and leave the backdrop unblended.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel overlap      = Sample(pixels, 40, 40, WIDTH); // inside the blended rect
        const Pixel backdropOnly = Sample(pixels, 5, 5, WIDTH);   // outside it -- backdrop untouched
        const Pixel mirrored     = Sample(pixels, 40, 87, WIDTH); // the rect's own vertical mirror

        // Multiply(0.8, 0.5) = 0.4 -> ~102/255. Backdrop-only area stays 0.8 -> ~204/255.
        REQUIRE(overlap.r > 90);
        REQUIRE(overlap.r < 115);
        REQUIRE(backdropOnly.r > 190);
        REQUIRE(backdropOnly.r < 215);
        REQUIRE(mirrored.r > 190);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU keeps a Shadow composite and two sibling opacity layers independent "
          "within one Render() call (Phase 35.17)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // All three internal composite draws (shadow, layer 1, layer 2) share the same small quad
        // vertex/index buffers, rewritten before each draw -- if any two ended up in the same
        // not-yet-submitted command buffer, every one of them would read whichever data was
        // written last (see NativeRendererWebGPU.hpp's own Phase 35.17 file comment).
        buffer.Push(DrawShadow{
            .Position = {70.0f, 70.0f},
            .Size = {30.0f, 30.0f},
            .BlurRadius = 4.0f,
            .Offset = {6.0f, 6.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {10.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});
        buffer.Push(PushOpacityLayer{.Opacity = 1.0f});
        buffer.Push(DrawRect{
            .Position = {50.0f, 10.0f}, .Size = {30.0f, 30.0f}, .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel halfRed   = Sample(pixels, 25, 25, WIDTH);
        const Pixel fullBlue  = Sample(pixels, 65, 25, WIDTH);
        const Pixel shadow    = Sample(pixels, 91, 91, WIDTH);
        const Pixel untouched = Sample(pixels, 5, 120, WIDTH);

        REQUIRE(halfRed.a > 100); // layer 1 kept its OWN opacity, not layer 2's 1.0
        REQUIRE(halfRed.a < 150);
        REQUIRE(halfRed.r > 100);
        REQUIRE(halfRed.r < 150);
        REQUIRE(fullBlue.b > 200);
        REQUIRE(fullBlue.a > 200);
        REQUIRE(shadow.a > 100); // the shadow's own composite quad survived, in its own place
        REQUIRE(shadow.r < 50);
        REQUIRE(untouched.a == 0);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU renders a DrawBackdropBlur: blends across a colour seam within its own "
          "rect, leaves everything outside untouched (Phase 35.18)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // A hard horizontal colour seam at y=64: red above, blue below. Deliberately asymmetric in
        // Y so a Y-orientation bug would show up as a wrong-side blend instead of passing by
        // coincidence -- the same scenario NativeRendererVulkan_test.cpp's/
        // NativeRendererDX12_test.cpp's own Phase 35.6/35.14 tests cover.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawBackdropBlur{.Position = {40.0f, 44.0f}, .Size = {48.0f, 40.0f}, .BlurRadius = 10.0f});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        // Exactly at the seam, well inside the blur rect (x in [40,88], y in [44,84]).
        const Pixel atSeam = Sample(pixels, 64, 64, WIDTH);
        // Just above the blur rect's own top edge (y=40 < 44) but inside its padded copy region --
        // proves the composite was cropped to the requested Size.
        const Pixel justAboveRect = Sample(pixels, 64, 40, WIDTH);
        const Pixel untouchedRed  = Sample(pixels, 10, 10, WIDTH);
        const Pixel untouchedBlue = Sample(pixels, 10, 118, WIDTH);

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

TEST_CASE("NativeRendererWebGPU rate-limits DrawBackdropBlur at MaxBackdropBlurPerFrame, degrading "
          "gracefully past the limit (Phase 35.18)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

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

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        // y=59 is 5px above the seam, inside every region's own Y range [54,74]: a processed region
        // bleeds some blue this far into the red band; a skipped one leaves it pure red.
        for (int i = 0; i < 4; ++i) {
            const int x = 10 + i * 20 + 8; // center-x of region i
            const Pixel processed = Sample(pixels, x, 59, WIDTH);
            REQUIRE(processed.b > 15);
        }
        const Pixel skipped = Sample(pixels, 10 + 4 * 20 + 8, 59, WIDTH);
        REQUIRE(skipped.b == 0);
        REQUIRE(skipped.r == 255);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererWebGPU blurs a pushed layer's own content for a DrawBackdropBlur recorded "
          "inside that layer (Phase 35.18)",
          "[webgpu]") {
    DawnWebGPUBackend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        const auto handles = backend.GetRendererHandles();
        ScratchTarget target(handles.Device, handles.Queue, handles.Instance, WIDTH, HEIGHT);

        CommandBuffer buffer;
        // The real target stays fully transparent; the red/blue seam exists only inside the layer.
        // Copying from the real target instead (Vulkan's/DX12's behaviour) would blur nothing and
        // leave a hard seam here.
        buffer.Push(PushOpacityLayer{.Opacity = 1.0f});
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawBackdropBlur{.Position = {40.0f, 44.0f}, .Size = {48.0f, 40.0f}, .BlurRadius = 10.0f});
        buffer.Push(PopLayer{});

        NativeRendererWebGPU renderer(handles.Device, handles.Queue, kColorFormat);
        renderer.SetTarget(target.Texture(), target.View(), WIDTH, HEIGHT);
        renderer.Render(buffer);

        const auto pixels = target.ReadPixels();
        const Pixel atSeam = Sample(pixels, 64, 64, WIDTH);
        const Pixel outside = Sample(pixels, 10, 10, WIDTH);

        REQUIRE(atSeam.r > 80);
        REQUIRE(atSeam.r < 180);
        REQUIRE(atSeam.b > 80);
        REQUIRE(atSeam.b < 180);
        REQUIRE(atSeam.a > 200);
        REQUIRE(outside.r > 200);
        REQUIRE(outside.b < 20);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
