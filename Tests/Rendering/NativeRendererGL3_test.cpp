/**
 * @file     NativeRendererGL3_test.cpp
 * @brief    Real-pixel tests for NativeRendererGL3's DrawRect rendering (Phase 32.4)
 *
 * @internal
 * Renders into a small offscreen FBO created directly by this test, not via
 * `GLFWOpenGL3Backend::BeginFrame()`/`EndFrame()` — `EndFrame()` calls
 * `glViewport()`/`glClear()` on the *main window* framebuffer internally,
 * after which it reads pixels back, so a direct `NativeRendererGL3::Render()`
 * call issued before `EndFrame()` (matching `ImGuiCompatRenderer_test.cpp`'s
 * pattern) would draw real pixels that `EndFrame()`'s own `glClear()` then
 * immediately erases before readback. `NativeRendererGL3` draws real GL
 * geometry immediately, unlike `ImGuiCompatRenderer`, which only records
 * into ImGui's draw list for later replay — the two need different test
 * harnesses for exactly that reason. A hand-created FBO sidesteps the
 * ordering conflict entirely and needs no `Application`/`Viewport` machinery.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "GLFWOpenGL3Backend.hpp"
#include "NativeRendererGL3.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Rendering;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "NativeRendererGL3_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<unsigned char>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {pixels[off + 0], pixels[off + 1], pixels[off + 2], pixels[off + 3]};
}

/// A small, self-contained offscreen FBO — see this file's header comment for why the main
/// window's own BeginFrame()/EndFrame() lifecycle can't be reused for a direct-GL renderer.
class ScratchFramebuffer {
public:
    ScratchFramebuffer(int width, int height) : _width(width), _height(height) {
        glGenFramebuffers(1, &_fbo);
        glGenTextures(1, &_colorTex);

        glBindTexture(GL_TEXTURE_2D, _colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);
    }

    ~ScratchFramebuffer() {
        glDeleteTextures(1, &_colorTex);
        glDeleteFramebuffers(1, &_fbo);
    }

    ScratchFramebuffer(const ScratchFramebuffer&) = delete;
    ScratchFramebuffer& operator=(const ScratchFramebuffer&) = delete;

    void BindAndClear() {
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, _width, _height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    [[nodiscard]] std::vector<unsigned char> ReadPixels() const {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(_width) * _height * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        // OpenGL's readback origin is bottom-left; flip rows so index 0 is top-left, matching
        // every other backend's ReadPixels() convention in this codebase.
        std::vector<unsigned char> flipped(pixels.size());
        const std::size_t rowBytes = static_cast<std::size_t>(_width) * 4;
        for (int y = 0; y < _height; ++y) {
            std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(rowBytes * (_height - 1 - y)), rowBytes,
                        flipped.begin() + static_cast<std::ptrdiff_t>(rowBytes * y));
        }
        return flipped;
    }

private:
    unsigned int _fbo      = 0;
    unsigned int _colorTex = 0;
    int          _width;
    int          _height;
};

} // namespace

TEST_CASE("NativeRendererGL3 draws a filled DrawRect at the recorded position", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(DrawRect{
            .Position = {static_cast<float>(WIDTH) / 2.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH) / 2.0f, static_cast<float>(HEIGHT)},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f},
        });

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
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

TEST_CASE("NativeRendererGL3 draws a DrawRect stroke without filling the interior", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position    = {10.0f, 10.0f},
            .Size        = {static_cast<float>(WIDTH) - 20.0f, static_cast<float>(HEIGHT) - 20.0f},
            .StrokeColor = {0.0f, 1.0f, 0.0f, 1.0f},
            .StrokeWidth = 4.0f,
        });

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel edge   = Sample(pixels, 11, HEIGHT / 2, WIDTH);
        const Pixel center = Sample(pixels, WIDTH / 2, HEIGHT / 2, WIDTH);

        REQUIRE(edge.g > edge.r);
        REQUIRE(edge.g > 150);
        REQUIRE(center.a == 0); // interior wasn't filled — still fully transparent

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 renders rounded corners: the extreme corner pixel stays outside the fill", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .Radii    = CornerRadii::All(24.0f),
            .FillColor = {1.0f, 1.0f, 1.0f, 1.0f},
        });

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel corner = Sample(pixels, 1, 1, WIDTH);   // just inside the rounded-away corner
        const Pixel middleEdge = Sample(pixels, WIDTH / 2, 1, WIDTH); // top-middle, outside any corner radius

        REQUIRE(corner.a < 100);      // rounded corner leaves this pixel (mostly) uncovered
        REQUIRE(middleEdge.a > 200);  // the flat top edge between corners is still fully filled

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 renders multiple batches (separated by a clip boundary) correctly", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}, .FillColor = {1.0f, 1.0f, 0.0f, 1.0f}});
        buffer.Push(PushClipRect{.Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)}});
        buffer.Push(DrawRect{
            .Position = {96.0f, 96.0f}, .Size = {32.0f, 32.0f}, .FillColor = {0.0f, 1.0f, 1.0f, 1.0f}});
        buffer.Push(PopClipRect{});

        BatchBuilder verifyBatching;
        verifyBatching.Build(buffer);
        // Rect1 opens a batch; PushClipRect flushes it; Rect2 opens a second batch; PopClipRect
        // flushes that one too (nothing is open afterward, so no further EndOfBuffer flush fires).
        REQUIRE(verifyBatching.Batches().size() == 2);

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel topLeft     = Sample(pixels, 16, 16, WIDTH);
        const Pixel bottomRight = Sample(pixels, 112, 112, WIDTH);

        REQUIRE(topLeft.r > 200);
        REQUIRE(topLeft.g > 200);
        REQUIRE(bottomRight.g > 200);
        REQUIRE(bottomRight.b > 200);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
