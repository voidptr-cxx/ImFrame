/**
 * @file     BlurPassGL3_test.cpp
 * @brief    Real-pixel tests for `BlurPassGL3`'s fragment-shader Gaussian blur (Phase 34.3)
 *
 * @internal
 * Same offscreen-FBO harness as `NativeRendererGL3_test.cpp` — see that file's header comment
 * for why a hand-created FBO is used instead of `GLFWOpenGL3Backend::BeginFrame()`/`EndFrame()`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-07
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "BlurPassGL3.hpp"
#include "GLFWOpenGL3Backend.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

constexpr int WIDTH  = 32;
constexpr int HEIGHT = 32;

WindowConfig OffscreenWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "BlurPassGL3_test";
    cfg.Width  = WIDTH;
    cfg.Height = HEIGHT;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<unsigned char>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {pixels[off + 0], pixels[off + 1], pixels[off + 2], pixels[off + 3]};
}

/// Reads back an arbitrary GL texture (not necessarily bound to the caller's own FBO) via a
/// throwaway FBO attachment -- `BlurPassGL3::Apply()` returns a texture it owns, not one already
/// bound to a caller-visible framebuffer.
std::vector<unsigned char> ReadTexture(unsigned int texture, int width, int height) {
    unsigned int fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glDeleteFramebuffers(1, &fbo);
    return pixels;
}

/// A small solid-white-on-transparent source texture: one fully-opaque pixel at the exact
/// center, fully transparent black everywhere else -- known content to assert blur spreads it.
class ImpulseTexture {
public:
    ImpulseTexture() {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);

        std::vector<unsigned char> pixels(static_cast<std::size_t>(WIDTH) * HEIGHT * 4, 0);
        const std::size_t centerOff = (static_cast<std::size_t>(HEIGHT / 2) * WIDTH + WIDTH / 2) * 4;
        pixels[centerOff + 0] = 255;
        pixels[centerOff + 1] = 255;
        pixels[centerOff + 2] = 255;
        pixels[centerOff + 3] = 255;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    ~ImpulseTexture() { glDeleteTextures(1, &_id); }

    ImpulseTexture(const ImpulseTexture&) = delete;
    ImpulseTexture& operator=(const ImpulseTexture&) = delete;

    [[nodiscard]] unsigned int Id() const { return _id; }

private:
    unsigned int _id = 0;
};

} // namespace

TEST_CASE("BlurPassGL3 with radius <= 0 returns the source texture unchanged", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ImpulseTexture source;
        BlurPassGL3 blur;

        REQUIRE(blur.Apply(source.Id(), WIDTH, HEIGHT, 0.0f) == source.Id());
        REQUIRE(blur.Apply(source.Id(), WIDTH, HEIGHT, -5.0f) == source.Id());

        blur.Shutdown();
    }

    backend.Shutdown();
}

TEST_CASE("BlurPassGL3 spreads a single opaque pixel into its neighbours", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ImpulseTexture source;
        BlurPassGL3 blur;

        const unsigned int blurred = blur.Apply(source.Id(), WIDTH, HEIGHT, 6.0f);
        REQUIRE(blurred != source.Id());

        auto pixels = ReadTexture(blurred, WIDTH, HEIGHT);
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

TEST_CASE("BlurPassGL3 reuses ping-pong targets across repeated calls at the same size", "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(OffscreenWindowConfig()).has_value());

    {
        ImpulseTexture source;
        BlurPassGL3 blur;

        const unsigned int first  = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        const unsigned int second = blur.Apply(source.Id(), WIDTH, HEIGHT, 4.0f);
        REQUIRE(first == second); // same target texture reused, not reallocated

        blur.Shutdown();
    }

    backend.Shutdown();
}
