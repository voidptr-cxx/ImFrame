/**
 * @file     ReconcilerImage_test.cpp
 * @brief    Proves ImageWidget's non-interactive Paint() emits a real DrawImage ImGuiCompatRenderer
 *           composites correctly (Phase 32.8)
 *
 * @internal
 * Drives `Internal::Reconciler::Show()` — the real production call site — with
 * an `ImageWidget` that has no `OnClick`, through a real `GLFWOpenGL3Backend`
 * and the default `ImGuiCompatRenderer`. Before Phase 32.8, `ImageElement::
 * Paint()` called `ImGui::Image()` directly and never touched the
 * `CommandBuffer` at all; `ImGuiCompatRenderer`'s `DrawImage` -> `AddImage`
 * translation (built in Phase 31) had no producer anywhere in the tree and
 * was consequently never exercised by a real pixel assertion. This test
 * closes both gaps at once: a real GL texture with known solid-colour content
 * is bound, an `ImageWidget` (no `OnClick`) is shown, and the real
 * framebuffer is read back after a full `BeginFrame()`/`EndFrame()` cycle —
 * proving the `DrawImage` command was actually pushed, actually translated,
 * and actually rasterized at the right screen position.
 *
 * `ImGuiCompatRenderer` only ever queues onto `ImGui::GetWindowDrawList()`
 * (it never draws immediately, unlike `NativeRendererGL3`), so there is no
 * Phase 32.5-style deferred-vs-immediate compositing hazard here — the
 * `AddImage()` call lands in the same draw list, after the window's own
 * queued background, and rasterizes correctly in one pass at `EndFrame()`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-18
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "GLFWOpenGL3Backend.hpp"
#include "Tree/Reconciler.hpp"

#include "ImFrame/Widgets/Image.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Widgets;

namespace {

WindowConfig TestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "ReconcilerImage_test";
    cfg.Width  = 128;
    cfg.Height = 128;
    return cfg;
}

struct Pixel { int r, g, b, a; };

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {
        static_cast<int>(pixels[off + 0]),
        static_cast<int>(pixels[off + 1]),
        static_cast<int>(pixels[off + 2]),
        static_cast<int>(pixels[off + 3]),
    };
}

/// A small, solid opaque-red GL texture — known content to assert against after compositing.
class ScratchTexture {
public:
    ScratchTexture() {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);

        constexpr int kSize = 8;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i + 0] = 255; pixels[i + 1] = 0; pixels[i + 2] = 0; pixels[i + 3] = 255;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    ~ScratchTexture() { glDeleteTextures(1, &_id); }

    ScratchTexture(const ScratchTexture&) = delete;
    ScratchTexture& operator=(const ScratchTexture&) = delete;

    /// Same reinterpret-through-void* convention `ImageWidget`/`TextureId` already use.
    [[nodiscard]] void* Handle() const { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(_id)); }

private:
    unsigned int _id = 0;
};

} // namespace

TEST_CASE("Reconciler + ImageWidget (no OnClick): Paint() emits a DrawImage command "
          "ImGuiCompatRenderer composites at the correct screen position",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    ScratchTexture texture;

    Reconciler reconciler; // default ImGuiCompatRenderer — no SetRenderer() call needed

    const Tree::Widget root = ImageWidget(texture.Handle(), Vec2{64.0f, 64.0f});
    reconciler.Show(root);

    backend.EndFrame();
    auto pixels = backend.ReadPixels();

    // The root window's content region starts a few pixels in from (0,0) (default ImGui window
    // padding) — (20, 20) is safely inside a 64x64 image anchored at the content region's origin.
    const Pixel inside = Sample(pixels, 20, 20, extent.Width);
    REQUIRE(inside.r > 200);
    REQUIRE(inside.g < 50);
    REQUIRE(inside.b < 50);
    REQUIRE(inside.a > 200);

    // Well outside the 64x64 image, still inside the window — should be the clear colour, not red.
    const Pixel outside = Sample(pixels, extent.Width - 5, extent.Height - 5, extent.Width);
    const bool outsideLooksRed = (outside.r > 200) && (outside.g < 50) && (outside.b < 50);
    REQUIRE_FALSE(outsideLooksRed);

    backend.Shutdown();
}
