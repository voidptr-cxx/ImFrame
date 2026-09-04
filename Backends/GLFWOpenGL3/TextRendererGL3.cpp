/**
 * @file     TextRendererGL3.cpp
 * @brief    `TextRendererGL3` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-26
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "TextRendererGL3.hpp"

#include <glad/glad.h>

#include <cstddef>
#include <vector>

namespace ImFrame::Internal {

namespace {

// Hand-written #version 330 core port of Shaders/MSDFText.glsl (Phase 33.5) — see this file's own
// header comment, and NativeRendererGL3.hpp's, for why the #version 450 generated source isn't
// reused directly. `uAtlas`/`uPxRange` are plain uniforms (not layout(binding=N), unavailable in
// 330 core without an extension), matching kImageVertexSource/kImageFragmentSource's own precedent.

const char* kTextVertexSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inTextColor;

uniform vec2 uViewportSize;

out vec2 vUv;
out vec4 vTextColor;

void main() {
    vec2 ndc = (inPosition / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vUv = inUv;
    vTextColor = inTextColor;
}
)GLSL";

const char* kTextFragmentSource = R"GLSL(
#version 330 core

in vec2 vUv;
in vec4 vTextColor;

uniform sampler2D uAtlas;
uniform float uPxRange;

out vec4 outColor;

float Median(vec3 msd) {
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

float ScreenPxRange() {
    vec2 unitRange = vec2(uPxRange) / vec2(textureSize(uAtlas, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(vUv);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(uAtlas, vUv).rgb;
    float signedDistance = Median(msd) - 0.5;
    float screenPxDistance = ScreenPxRange() * signedDistance;
    float opacity = smoothstep(-0.5, 0.5, screenPxDistance);

    outColor = vec4(vTextColor.rgb, vTextColor.a * opacity);
}
)GLSL";

// Duplicated from NativeRendererGL3.cpp's own anonymous-namespace helpers of the same name/shape
// (not shared via a header) — matches this codebase's already-accepted duplication of e.g.
// RoundedBoxSdf across SDFRect.glsl/Image.glsl/NativeRendererGL3.cpp. Kept in sync by hand.

[[nodiscard]] unsigned int CompileShaderStage(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    IMF_ASSERT(compiled != 0); // a compile failure here is a bug in the hardcoded source above, not
                               // a user-facing condition.
    return shader;
}

[[nodiscard]] unsigned int LinkProgram(const char* vertexSource, const char* fragmentSource) {
    const unsigned int vs = CompileShaderStage(GL_VERTEX_SHADER, vertexSource);
    const unsigned int fs = CompileShaderStage(GL_FRAGMENT_SHADER, fragmentSource);

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    IMF_ASSERT(linked != 0);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

} // namespace

TextRendererGL3::~TextRendererGL3() { Shutdown(); }

void TextRendererGL3::Shutdown() {
    if (_vbo != 0) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
    if (_ebo != 0) { glDeleteBuffers(1, &_ebo); _ebo = 0; }
    if (_vao != 0) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
    if (_program != 0) { glDeleteProgram(_program); _program = 0; }
    if (_atlasTexture != 0) { glDeleteTextures(1, &_atlasTexture); _atlasTexture = 0; }

    _initialized        = false;
    _uploadedGeneration = 0;
}

void TextRendererGL3::EnsureInitialized() {
    if (_initialized) { return; }

    _program         = LinkProgram(kTextVertexSource, kTextFragmentSource);
    _viewportSizeLoc = glGetUniformLocation(_program, "uViewportSize");
    _atlasLoc        = glGetUniformLocation(_program, "uAtlas");
    _pxRangeLoc      = glGetUniformLocation(_program, "uPxRange");

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

    constexpr auto stride = static_cast<GLsizei>(sizeof(TextVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(TextVertex, Position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(TextVertex, Uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(TextVertex, Color)));

    glBindVertexArray(0);

    _initialized = true;
}

void TextRendererGL3::SyncAtlasTexture() {
    const TextureAtlas& atlas      = _glyphAtlas.Atlas();
    const std::uint64_t generation = atlas.Generation();
    if (_atlasTexture != 0 && generation == _uploadedGeneration) { return; }

    if (_atlasTexture == 0) { glGenTextures(1, &_atlasTexture); }

    // TextureAtlas has no "give me every pixel" accessor beyond ReadRegion() -- a region spanning
    // its full current dimensions is exactly that, and satisfies ReadRegion()'s own bounds
    // assertion (X + Width <= Width(), Y + Height <= Height()) with equality.
    const std::vector<std::uint8_t> pixels =
        atlas.ReadRegion(AtlasRegion{.X = 0, .Y = 0, .Width = atlas.Width(), .Height = atlas.Height()});

    glBindTexture(GL_TEXTURE_2D, _atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(atlas.Width()),
                 static_cast<GLsizei>(atlas.Height()), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    _uploadedGeneration = generation;
}

Result<Rendering::FontId> TextRendererGL3::LoadFont(const Utility::Path& path, float sizePixels) {
    return _registry.Load(path, sizePixels);
}

Rendering::TextureId TextRendererGL3::LayoutText(const Rendering::DrawText& cmd, std::vector<GlyphQuad>& outQuads) {
    const FontFace* face = _registry.Get(cmd.Font);
    if (face == nullptr) { return Rendering::TextureId{}; }

    const ShapedText* shaped = _shaper.Shape(_registry, cmd.Font, cmd.Text);
    if (shaped == nullptr) { return Rendering::TextureId{}; }

    // TextShaper's Advance/Offset are in pixels at face->SizePixels() (the size FontRegistry::Load()
    // originally requested) -- rescale to whatever size this DrawText actually asked for. MSDF
    // itself needs no rescaling (GlyphAtlasEntry's SizeEm/BearingEm are already size-independent
    // em units); only the HarfBuzz-derived pen movement scales linearly with the requested size.
    const float fontSizePx = cmd.FontSize > 0.0f ? cmd.FontSize : face->SizePixels();
    const float scale      = fontSizePx / face->SizePixels();

    // DrawText::Position is the text's top-left corner, not the baseline -- matches
    // ImGuiCompatRenderer::Translate(DrawText)'s existing convention (it hands Position straight
    // to ImDrawList::AddText(), which is documented as top-left). FT_Face::size->metrics.ascender
    // is set by FT_Set_Pixel_Sizes() at face->SizePixels(); rescale it the same way as every other
    // shaped metric above.
    const float ascenderPx = static_cast<float>(face->Face()->size->metrics.ascender) / 64.0f * scale;
    Widgets::Vec2 pen{cmd.Position.x, cmd.Position.y + ascenderPx};
    for (const GlyphRun& run : shaped->Runs) {
        const GlyphAtlasEntry* entry = _glyphAtlas.GetOrCreate(_registry, cmd.Font, run.GlyphId);
        if (entry != nullptr && !entry->IsBlank) {
            const Widgets::Vec2 glyphPen{pen.x + run.Offset.x * scale, pen.y + run.Offset.y * scale};

            // GlyphAtlasEntry::BearingEm/SizeEm use msdfgen's Y-up convention (see GlyphAtlas.hpp's
            // own doc comment) -- negate Y to place the quad in this codebase's top-down screen space.
            outQuads.push_back(GlyphQuad{
                .Min =
                    {
                        glyphPen.x + entry->BearingEm.x * fontSizePx,
                        glyphPen.y - (entry->BearingEm.y + entry->SizeEm.y) * fontSizePx,
                    },
                .Max =
                    {
                        glyphPen.x + (entry->BearingEm.x + entry->SizeEm.x) * fontSizePx,
                        glyphPen.y - entry->BearingEm.y * fontSizePx,
                    },
                .UvMin = entry->UvMin,
                .UvMax = entry->UvMax,
            });
        }

        pen.x += run.Advance.x * scale;
        pen.y += run.Advance.y * scale;
    }

    // Stable for this instance's lifetime (TextureAtlas::TextureId() is stable across growth) --
    // every DrawText resolved by this provider shares the one glyph atlas, so BatchBuilder never
    // spuriously flushes on a texture change between two DrawText commands.
    return _glyphAtlas.Atlas().TextureId();
}

void TextRendererGL3::RenderTextBatch(const Batch& batch) {
    const auto& vertices = std::get<std::vector<TextVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    EnsureInitialized();
    SyncAtlasTexture();

    glBindVertexArray(_vao);
    glUseProgram(_program);

    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUniform2f(_viewportSizeLoc, static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));
    glUniform1f(_pxRangeLoc, static_cast<float>(GlyphAtlas::kPxRange));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _atlasTexture);
    glUniform1i(_atlasLoc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(TextVertex)), vertices.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.Indices.size() * sizeof(std::uint32_t)),
                 batch.Indices.data(), GL_STREAM_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.Indices.size()), GL_UNSIGNED_INT, nullptr);
}

} // namespace ImFrame::Internal
