/**
 * @file     TextRendererGL3_test.cpp
 * @brief    Real-pixel tests for TextRendererGL3's MSDF text rendering (Phase 33.7)
 *
 * @internal
 * Uses the same hand-created offscreen-FBO harness as `NativeRendererGL3_test.cpp` (see that
 * file's own header comment for why `GLFWOpenGL3Backend::EndFrame()`'s lifecycle can't be reused
 * for a direct-GL renderer). `Icons::Fa::House` is used as the rendered glyph, not a hand-typed
 * Private-Use-Area codepoint — see `.claude/DECISIONS.md`, Phase 33.4, for why the latter is
 * unreliable (falls back to `.notdef`, not a real icon, for most codepoints in the declared range).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-26
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "GLFWOpenGL3Backend.hpp"
#include "NativeRendererGL3.hpp"
#include "TextRendererGL3.hpp"

#include "ImFrame/Icons/Icons.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Rendering;

namespace {

constexpr int WIDTH  = 128;
constexpr int HEIGHT = 128;

const Utility::Path kRealFont("Assets/Fonts/fa-solid-900.ttf");

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "TextRendererGL3_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<unsigned char>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {pixels[off + 0], pixels[off + 1], pixels[off + 2], pixels[off + 3]};
}

/// See NativeRendererGL3_test.cpp's identical class for the full rationale.
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

    ScratchFramebuffer(const ScratchFramebuffer&)            = delete;
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

TEST_CASE("TextRendererGL3::LoadFont returns FileNotFound for a missing path", "[unit]") {
    TextRendererGL3 textRenderer;
    const auto      result = textRenderer.LoadFont(Utility::Path("Assets/Fonts/does-not-exist.ttf"), 16.0f);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Error::FileNotFound);
}

TEST_CASE("TextRendererGL3::LayoutText returns an invalid texture and no quads for an unknown FontId", "[unit]") {
    TextRendererGL3 textRenderer;

    std::vector<ITextLayoutProvider::GlyphQuad> quads;
    const TextureId texture = textRenderer.LayoutText(DrawText{.Text = "hi", .Font = FontId{}}, quads);

    REQUIRE_FALSE(texture.IsValid());
    REQUIRE(quads.empty());
}

TEST_CASE("TextRendererGL3 renders real MSDF glyph pixels through NativeRendererGL3's "
          "BatchKind::Text path, end to end",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        TextRendererGL3 textRenderer;
        const auto      font = textRenderer.LoadFont(kRealFont, 48.0f);
        REQUIRE(font.has_value());

        ScratchFramebuffer fb(WIDTH, HEIGHT);
        fb.BindAndClear();

        CommandBuffer buffer;
        buffer.Push(DrawText{
            .Position = {16.0f, 16.0f},
            .Text     = Icons::Fa::House,
            .Font     = *font,
            .FontSize = 64.0f,
            .Color    = {1.0f, 1.0f, 1.0f, 1.0f},
        });

        NativeRendererGL3 renderer;
        renderer.AttachTextRenderer(&textRenderer);
        renderer.Render(buffer);

        const std::vector<unsigned char> pixels = fb.ReadPixels();

        // Don't assume an exact glyph-center coordinate (that depends on this specific icon's
        // bearing/advance) -- scan for the brightest pixel across the canvas instead, and confirm
        // it is both opaque and bright, proving the MSDF pipeline actually drew real coverage.
        int   brightestAlpha = 0;
        Pixel brightest{};
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                const Pixel p = Sample(pixels, x, y, WIDTH);
                if (p.a > brightestAlpha) {
                    brightestAlpha = p.a;
                    brightest      = p;
                }
            }
        }

        UNSCOPED_INFO("brightest pixel: r=" << brightest.r << " g=" << brightest.g << " b=" << brightest.b
                                             << " a=" << brightest.a);
        REQUIRE(brightest.a > 200);  // real, mostly-opaque coverage was drawn somewhere
        REQUIRE(brightest.r > 200);  // Color was white -- covered pixels should be near-white, not tinted
        REQUIRE(brightest.g > 200);
        REQUIRE(brightest.b > 200);

        const Pixel corner = Sample(pixels, 2, 2, WIDTH);
        REQUIRE(corner.a == 0); // far outside the glyph -- still fully transparent background

        renderer.Shutdown();
        textRenderer.Shutdown();
    }

    backend.Shutdown();
}
