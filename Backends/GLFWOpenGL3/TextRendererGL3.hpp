/**
 * @file     TextRendererGL3.hpp
 * @brief    Concrete `ITextRenderer`/`ITextLayoutProvider` adapting FontRegistry+TextShaper+GlyphAtlas to real GL3 draw calls
 *
 * @internal
 * Compiled only when both `IMF_BUILD_NATIVE_RENDERER` and `IMF_BUILD_TEXT_MSDF` are enabled (see
 * `Backends/GLFWOpenGL3/CMakeLists.txt`) — `NativeRendererGL3` itself stays free of this file's
 * `FontRegistry`/`TextShaper`/`GlyphAtlas` dependency, only ever calling through the abstract
 * `ITextRenderer`/`ITextLayoutProvider` interfaces (`NativeRendererGL3.hpp`). A consumer wanting
 * real MSDF text support constructs one of these and calls
 * `NativeRendererGL3::AttachTextRenderer(&textRenderer)`.
 *
 * Owns the one thing `GlyphAtlas`'s `TextureAtlas` (Phase 32.1) still lacks: a real GPU texture.
 * `TextureAtlas` is CPU-only by design (see its own file comment) — `SyncAtlasTexture()` mirrors
 * its RGBA8 pixel buffer into an actual `GL_TEXTURE_2D`, re-uploading the whole buffer whenever
 * `TextureAtlas::Generation()` (Phase 33.7) has advanced since the last upload. A full re-upload,
 * not a partial/dirty-rect one — simplest correct thing; see `.claude/DECISIONS.md`, Phase 33.7.
 *
 * Its shader is a hand-written `#version 330 core` port of `Shaders/MSDFText.glsl` (Phase 33.5),
 * for the exact same reason `NativeRendererGL3.hpp`'s own Rect/Image shaders are hand-written
 * ports rather than reusing the `#version 450` generated header — see that file's own comment.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-26
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "NativeRendererGL3.hpp"

#include "Rendering/Text/FontRegistry.hpp"
#include "Rendering/Text/GlyphAtlas.hpp"
#include "Rendering/Text/TextShaper.hpp"

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"

#include <cstdint>
#include <vector>

namespace ImFrame::Internal {

/**
 * @class    TextRendererGL3
 * @brief    Real GL3 MSDF text rendering — the concrete `ITextRenderer` for `NativeRendererGL3`
 *
 * @internal
 * Must be constructed and used with an OpenGL 3.3+ core-profile context current on the calling
 * thread, matching every other GL-touching type in this codebase. GL resources are created lazily
 * on first use (`EnsureInitialized()`), so construction never requires a context to already exist
 * — same convention as `NativeRendererGL3`/`FontRegistry`.
 *
 * Non-copyable, non-moveable — every `Rendering::FontId` a caller holds implicitly refers to this
 * instance's own `FontRegistry`, and every GL resource is tied to whichever context was current
 * when it was created (matches `FontRegistry`'s own non-copy/non-move reasoning).
 *
 * @since    3.0.0
 */
class TextRendererGL3 final : public ITextRenderer, public ITextLayoutProvider {
public:
    TextRendererGL3() = default;
    ~TextRendererGL3() override;

    TextRendererGL3(const TextRendererGL3&)            = delete;
    TextRendererGL3& operator=(const TextRendererGL3&) = delete;
    TextRendererGL3(TextRendererGL3&&)                 = delete;
    TextRendererGL3& operator=(TextRendererGL3&&)      = delete;

    /**
     * @brief    Loads a font file, returning an opaque handle usable as `Rendering::DrawText::Font`.
     * @param[in]  path        Path to a `.ttf`/`.otf` file.
     * @param[in]  sizePixels  Initial pixel size hint — see `FontRegistry::Load()`. `DrawText`
     *                         commands may later request a different `FontSize`; `LayoutText()`
     *                         rescales this font's shaped metrics to match.
     * @return   A `Rendering::FontId`, or `Error::FileNotFound`/`Error::FontLoadFailed`.
     * @throws   Nothing.
     */
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels);

    /// Releases all GL resources. Safe to call multiple times, including before any render call.
    void Shutdown();

    // --- ITextLayoutProvider ---
    [[nodiscard]] Rendering::TextureId LayoutText(const Rendering::DrawText& cmd,
                                                   std::vector<GlyphQuad>& outQuads) override;

    // --- ITextRenderer ---
    [[nodiscard]] ITextLayoutProvider* LayoutProvider() noexcept override { return this; }
    void RenderTextBatch(const Batch& batch) override;

private:
    void EnsureInitialized();
    void SyncAtlasTexture();

    FontRegistry _registry;
    TextShaper   _shaper;
    GlyphAtlas   _glyphAtlas;

    bool _initialized = false;

    unsigned int _program         = 0; ///< GLuint linked shader program — GL3 port of Shaders/MSDFText.glsl.
    int          _viewportSizeLoc = -1;
    int          _atlasLoc        = -1;
    int          _pxRangeLoc      = -1; ///< Uniform for GlyphAtlas::kPxRange (msdfgen's generation-time distance range).

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ebo = 0;

    unsigned int  _atlasTexture       = 0; ///< GL mirror of _glyphAtlas.Atlas()'s CPU pixel buffer.
    std::uint64_t _uploadedGeneration = 0; ///< Last TextureAtlas::Generation() mirrored to _atlasTexture.
};

} // namespace ImFrame::Internal
