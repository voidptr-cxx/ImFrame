/**
 * @file     NativeRendererGL3_test.cpp
 * @brief    Real-pixel tests for NativeRendererGL3's DrawRect (Phase 32.4) and DrawImage (Phase 32.9) rendering
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
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "GLFWOpenGL3Backend.hpp"
#include "NativeRendererGL3.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
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

/// A small, solid opaque-blue GL texture — known content to assert against after compositing.
/// `NativeRendererGL3` treats `DrawImage::Texture`'s value as a raw GL texture name (see
/// `NativeRendererGL3.hpp`'s own file comment), so this real texture's GLuint is what gets pushed.
class ScratchTexture {
public:
    ScratchTexture() {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);

        constexpr int kSize = 8;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = 0; pixels[i + 1] = 0; pixels[i + 2] = 255; pixels[i + 3] = 255;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    ~ScratchTexture() { glDeleteTextures(1, &_id); }

    ScratchTexture(const ScratchTexture&) = delete;
    ScratchTexture& operator=(const ScratchTexture&) = delete;

    [[nodiscard]] TextureId Id() const { return TextureId(static_cast<std::uint64_t>(_id)); }

private:
    unsigned int _id = 0;
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

TEST_CASE("NativeRendererGL3 draws a DrawImage at the recorded position, sampling the bound "
          "GL texture and applying TintColor",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        ScratchTexture texture;

        CommandBuffer buffer;
        buffer.Push(DrawImage{
            .Position  = {0.0f, 0.0f},
            .Size      = {64.0f, 64.0f},
            .Texture   = texture.Id(),
            .TintColor = {0.5f, 1.0f, 1.0f, 1.0f}, // multiplies the texture's solid blue
        });

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel inside  = Sample(pixels, 32, 32, WIDTH);
        const Pixel outside = Sample(pixels, WIDTH - 4, HEIGHT - 4, WIDTH);

        REQUIRE(inside.b > 200);
        REQUIRE(inside.r < 150);  // TintColor.r == 0.5 darkens the source texture's zero red further
        REQUIRE(inside.a > 200);
        REQUIRE(outside.a == 0); // untouched — still the FBO's transparent clear colour

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 renders a DrawImage with rounded corners: the extreme corner "
          "pixel stays outside the mask",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        ScratchTexture texture;

        CommandBuffer buffer;
        buffer.Push(DrawImage{
            .Position = {0.0f, 0.0f},
            .Size     = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .Texture  = texture.Id(),
            .Radii    = CornerRadii::All(24.0f),
        });

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel corner     = Sample(pixels, 1, 1, WIDTH);          // just inside the rounded-away corner
        const Pixel middleEdge = Sample(pixels, WIDTH / 2, 1, WIDTH);  // flat top edge, outside any radius

        REQUIRE(corner.a < 100);
        REQUIRE(middleEdge.a > 200);
        REQUIRE(middleEdge.b > 200); // still sampling the texture's blue, not just an opaque mask

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 renders a DrawShadow behind and offset from the shape it shadows "
          "(Phase 34.4)",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        // Shadow first (drawn behind), then an opaque white rect at the same (unshifted) position
        // and size on top -- per PHASE_34_PROPOSAL.md's DrawShadow section ("composite it behind
        // the shape at the specified offset").
        buffer.Push(DrawShadow{
            .Position = {40.0f, 40.0f},
            .Size = {48.0f, 48.0f},
            .BlurRadius = 8.0f,
            .Offset = {10.0f, 10.0f},
            .ShadowColor = {0.0f, 0.0f, 0.0f, 1.0f},
        });
        buffer.Push(DrawRect{
            .Position = {40.0f, 40.0f}, .Size = {48.0f, 48.0f}, .FillColor = {1.0f, 1.0f, 1.0f, 1.0f}});

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
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

TEST_CASE("NativeRendererGL3 composites a PushOpacityLayer at the recorded opacity (Phase 34.5)", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(PushOpacityLayer{.Opacity = 0.5f});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel inside  = Sample(pixels, 40, 40, WIDTH);
        const Pixel outside = Sample(pixels, 5, 5, WIDTH);

        // Correct premultiplied-alpha compositing: an opaque red rect at Opacity=0.5 ends up with
        // both its alpha AND its stored (premultiplied) red channel scaled to roughly half --
        // matches PHASE_34_PROPOSAL.md's own testing spec ("PushOpacityLayer(0.5) halves pixel
        // alpha values").
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

TEST_CASE("NativeRendererGL3 composites a PushBlendLayer using the Multiply formula (Phase 34.5)", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        // Opaque light-gray backdrop filling the whole viewport, then a Multiply layer with an
        // opaque mid-gray rect over part of it.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Multiply});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel overlap      = Sample(pixels, 40, 40, WIDTH); // inside the blended rect
        const Pixel backdropOnly = Sample(pixels, 5, 5, WIDTH);  // outside it -- backdrop untouched

        // Multiply(0.8, 0.5) = 0.4 -> ~102/255. Backdrop-only area stays 0.8 -> ~204/255 (the
        // layer's own texture is transparent there, so the union-alpha formula falls back to the
        // unchanged backdrop).
        REQUIRE(overlap.r > 90);
        REQUIRE(overlap.r < 115);
        REQUIRE(backdropOnly.r > 190);
        REQUIRE(backdropOnly.r < 215);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 composites a PushBlendLayer using the Screen formula, differently "
          "from Multiply (Phase 34.5)",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT)},
            .FillColor = {0.8f, 0.8f, 0.8f, 1.0f}});
        buffer.Push(PushBlendLayer{.Mode = BlendMode::Screen});
        buffer.Push(DrawRect{
            .Position = {20.0f, 20.0f}, .Size = {40.0f, 40.0f}, .FillColor = {0.5f, 0.5f, 0.5f, 1.0f}});
        buffer.Push(PopLayer{});

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel overlap = Sample(pixels, 40, 40, WIDTH);

        // Screen(0.8, 0.5) = 0.8 + 0.5 - 0.4 = 0.9 -> ~229/255, distinctly brighter than Multiply's
        // ~102/255 for the exact same backdrop/source pair -- proves uMode really switches the
        // formula rather than one mode being hardcoded regardless of the uniform.
        REQUIRE(overlap.r > 215);
        REQUIRE(overlap.r < 240);

        renderer.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("NativeRendererGL3 renders a DrawBackdropBlur: blends across a colour seam within its "
          "own rect, leaves everything outside untouched (Phase 34.6)",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        // A hard horizontal colour seam at y=64: red above, blue below. Deliberately asymmetric
        // in Y (not a uniform fill) so a Y-orientation bug (like Phase 34.5's) would show up as a
        // wrong-side blend instead of passing by coincidence.
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawRect{
            .Position = {0.0f, static_cast<float>(HEIGHT) / 2.0f},
            .Size = {static_cast<float>(WIDTH), static_cast<float>(HEIGHT) / 2.0f},
            .FillColor = {0.0f, 0.0f, 1.0f, 1.0f}});
        buffer.Push(DrawBackdropBlur{.Position = {40.0f, 44.0f}, .Size = {48.0f, 40.0f}, .BlurRadius = 10.0f});

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        // Exactly at the seam, well inside the blur rect (x in [40,88], y in [44,84]) -- a real
        // blur straddling red-above/blue-below should show a roughly even mix of both.
        const Pixel atSeam = Sample(pixels, 64, 64, WIDTH);
        // Just outside the blur rect's own top edge (y=40 < 44) but inside its padded copy region
        // (padding = BlurRadius = 10, so the copy reaches up to y=34) -- proves the composite was
        // cropped to the requested Size, not left showing the padding's own blurred bleed.
        const Pixel justAboveRect = Sample(pixels, 64, 40, WIDTH);
        // Far from the blur rect and the seam entirely -- untouched original colours.
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

TEST_CASE("NativeRendererGL3 rate-limits DrawBackdropBlur at MaxBackdropBlurPerFrame, degrading "
          "gracefully past the limit (Phase 34.6)",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

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

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        // y=59 is 5px above the seam (y=64), inside every region's own Y range [54,74] but nowhere
        // near the geometric seam itself -- a processed (blurred) region bleeds some blue this far
        // into the red band; a skipped region leaves this pixel exactly the original pure red.
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

TEST_CASE("NativeRendererGL3 renders interleaved Rect and Image batches, each with the "
          "correct kind's geometry and texture",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        ScratchTexture texture;

        CommandBuffer buffer;
        buffer.Push(DrawRect{
            .Position = {0.0f, 0.0f}, .Size = {32.0f, 32.0f}, .FillColor = {1.0f, 0.0f, 0.0f, 1.0f}});
        buffer.Push(DrawImage{
            .Position = {96.0f, 96.0f}, .Size = {32.0f, 32.0f}, .Texture = texture.Id()});

        BatchBuilder verifyBatching;
        verifyBatching.Build(buffer);
        REQUIRE(verifyBatching.Batches().size() == 2);
        REQUIRE(verifyBatching.Batches()[0].Kind == BatchKind::Rect);
        REQUIRE(verifyBatching.Batches()[1].Kind == BatchKind::Image);

        NativeRendererGL3 renderer;
        renderer.Render(buffer);

        auto pixels = fb.ReadPixels();
        const Pixel rectPixel  = Sample(pixels, 16, 16, WIDTH);
        const Pixel imagePixel = Sample(pixels, 112, 112, WIDTH);

        REQUIRE(rectPixel.r > 200);
        REQUIRE(rectPixel.b < 50);
        REQUIRE(imagePixel.b > 200);
        REQUIRE(imagePixel.r < 50);

        renderer.Shutdown();
    }

    backend.Shutdown();
}
