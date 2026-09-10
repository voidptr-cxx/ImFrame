/**
 * @file     NativeRendererGL3.cpp
 * @brief    `NativeRendererGL3` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "NativeRendererGL3.hpp"

#include "ImFrame/Core/Error.hpp"

#include <glad/glad.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

namespace ImFrame::Internal {

namespace {

// Hand-written #version 330 core equivalent of Shaders/SDFRect.glsl's rounded-box SDF logic — see
// this file's own header comment for why the Phase 32.2 #version 450 source isn't reused directly.

const char* kRectVertexSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inLocal;
layout (location = 2) in vec2 inHalfSize;
layout (location = 3) in vec4 inRadii;
layout (location = 4) in vec4 inFillColor;
layout (location = 5) in vec4 inStrokeColor;
layout (location = 6) in float inStrokeWidth;

uniform vec2 uViewportSize;

out vec2 vLocal;
out vec2 vHalfSize;
out vec4 vRadii;
out vec4 vFillColor;
out vec4 vStrokeColor;
out float vStrokeWidth;

void main() {
    vec2 ndc = (inPosition / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vLocal = inLocal;
    vHalfSize = inHalfSize;
    vRadii = inRadii;
    vFillColor = inFillColor;
    vStrokeColor = inStrokeColor;
    vStrokeWidth = inStrokeWidth;
}
)GLSL";

const char* kRectFragmentSource = R"GLSL(
#version 330 core

in vec2 vLocal;
in vec2 vHalfSize;
in vec4 vRadii;
in vec4 vFillColor;
in vec4 vStrokeColor;
in float vStrokeWidth;

out vec4 outColor;

float RoundedBoxSdf(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r.x;
}

void main() {
    vec4 r = vec4(vRadii.y, vRadii.z, vRadii.x, vRadii.w);
    float dist = RoundedBoxSdf(vLocal, vHalfSize, r);

    // fwidth-derived AA band (Phase 34.2) -- matches Shaders/SDFRect.glsl's identical fix
    // (Phase 34.1): a hardcoded +-1.0 band is only correct at exact 1:1 pixel scale.
    float aa = max(fwidth(dist) * 0.5, 1e-4);
    float fillAlpha = 1.0 - smoothstep(-aa, aa, dist);
    vec4 color = vFillColor * fillAlpha;

    if (vStrokeWidth > 0.0) {
        float strokeDist = abs(dist) - vStrokeWidth * 0.5;
        float strokeAa = max(fwidth(strokeDist) * 0.5, 1e-4);
        float strokeAlpha = 1.0 - smoothstep(-strokeAa, strokeAa, strokeDist);
        color = mix(color, vStrokeColor, strokeAlpha * vStrokeColor.a);
    }

    outColor = color;
}
)GLSL";

// Hand-written #version 330 core equivalent of Shaders/Image.glsl (Phase 32.9) — same deviation
// reasoning as the Rect shaders above. `uTexture` uses a plain `uniform sampler2D` (not
// `layout(binding=1)`, unavailable in 330 core without an extension) — bound to texture unit 0
// explicitly via glUniform1i() at draw time, matching uViewportSize's glGetUniformLocation() cache.

const char* kImageVertexSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inLocal;
layout (location = 2) in vec2 inHalfSize;
layout (location = 3) in vec4 inRadii;
layout (location = 4) in vec2 inUv;
layout (location = 5) in vec4 inTintColor;

uniform vec2 uViewportSize;

out vec2 vLocal;
out vec2 vHalfSize;
out vec4 vRadii;
out vec2 vUv;
out vec4 vTintColor;

void main() {
    vec2 ndc = (inPosition / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vLocal = inLocal;
    vHalfSize = inHalfSize;
    vRadii = inRadii;
    vUv = inUv;
    vTintColor = inTintColor;
}
)GLSL";

const char* kImageFragmentSource = R"GLSL(
#version 330 core

in vec2 vLocal;
in vec2 vHalfSize;
in vec4 vRadii;
in vec2 vUv;
in vec4 vTintColor;

uniform sampler2D uTexture;

out vec4 outColor;

float RoundedBoxSdf(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r.x;
}

void main() {
    vec4 r = vec4(vRadii.y, vRadii.z, vRadii.x, vRadii.w);
    float dist = RoundedBoxSdf(vLocal, vHalfSize, r);

    // fwidth-derived AA band (Phase 34.2) -- see kRectFragmentSource's identical fix above.
    float aa = max(fwidth(dist) * 0.5, 1e-4);
    float mask = 1.0 - smoothstep(-aa, aa, dist);
    outColor = texture(uTexture, vUv) * vTintColor * mask;
}
)GLSL";

[[nodiscard]] unsigned int CompileShaderStage(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    IMF_ASSERT(compiled != 0); // a compile failure here is a bug in the hardcoded source above, not
                               // a user-facing condition — see this file's DOC_STANDARDS error-handling note.
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

/// Heap-owned snapshot handed to `ImDrawList::AddCallback()` as userdata for `RenderMode::
/// DeferredReplay` — owns its own copy of the batches so a subsequent `Render()` call within the
/// same frame (rebuilding `_batchBuilder`'s internal storage) cannot invalidate a callback that
/// hasn't fired yet. Freed by `NativeRendererGL3::ExecuteDeferredDraw()` after it runs.
struct PendingDraw {
    NativeRendererGL3*  Self;
    std::vector<Batch>  Batches;
};

/// Builds one axis-aligned quad's `RectVertex`es (Phase 34.4's shadow silhouette pass reuses the
/// existing Rect shader/pipeline via `RenderRectBatch()` rather than a third hand-written shader).
std::vector<RectVertex> BuildRectQuadVertices(Widgets::Vec2 position, Widgets::Vec2 size,
                                               Rendering::CornerRadii radii, Widgets::Vec4 fillColor) {
    const Widgets::Vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    const Widgets::Vec2 halfSize{size.x * 0.5f, size.y * 0.5f};
    const Widgets::Vec2 corners[4] = {
        {position.x, position.y},
        {position.x + size.x, position.y},
        {position.x + size.x, position.y + size.y},
        {position.x, position.y + size.y},
    };

    std::vector<RectVertex> vertices;
    vertices.reserve(4);
    for (const Widgets::Vec2& corner : corners) {
        vertices.push_back(RectVertex{
            .Position = corner,
            .Local = {corner.x - center.x, corner.y - center.y},
            .HalfSize = halfSize,
            .Radii = radii,
            .FillColor = fillColor,
            .StrokeColor = {},
            .StrokeWidth = 0.0f,
        });
    }
    return vertices;
}

/// Builds one axis-aligned quad's `ImageVertex`es, unrounded (`Radii` all zero) — Phase 34.4's
/// shadow composite pass and Phase 34.5's opacity-layer composite pass both reuse the existing
/// Image shader/pipeline via `RenderImageBatch()` to draw a renderer-produced offscreen texture
/// (a blurred shadow, or a popped layer), tinted, as a plain rectangle.
///
/// @internal
/// The UV table is deliberately V-flipped relative to `AppendImage()`'s own (`BatchBuilder.cpp`,
/// for real user `DrawImage` commands sampling an externally-uploaded texture) -- and this isn't
/// optional/stylistic, it's required specifically because the *source* texture here was itself
/// rendered by this same renderer's Rect/Image vertex shader, which negates Y
/// (`gl_Position = vec4(ndc.x, -ndc.y, 0, 1)`): pixel-space Y=0 (this codebase's "top") ends up
/// written to the texture's OWN raw bottom row (GL's native bottom-up row order), and pixel-space
/// Y=height ends up at the texture's raw top row -- the *opposite* of GLSL's default `texture()`
/// sampling convention, where UV=(0,0) reads the raw bottom row directly. A naive (non-flipped)
/// UV table here would sample the wrong row for anything drawn at pixel-space Y=0 vs Y=height,
/// silently reading content from the *mirrored* position instead (or nothing, if the source
/// content isn't itself vertically symmetric). This was caught empirically: Phase 34.4's own
/// shadow-composite test never exposed it because its source content (a full, edge-to-edge,
/// unrounded white silhouette) looks identical whether flipped or not; Phase 34.5's opacity-layer
/// composite test (an off-center rect) does, and failed until this V-flip was added. See
/// `.claude/DECISIONS.md`'s Phase 34.5 entry for the full derivation.
std::vector<ImageVertex> BuildImageQuadVertices(Widgets::Vec2 position, Widgets::Vec2 size, Widgets::Vec4 tintColor) {
    const Widgets::Vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    const Widgets::Vec2 halfSize{size.x * 0.5f, size.y * 0.5f};
    const Widgets::Vec2 corners[4] = {
        {position.x, position.y},
        {position.x + size.x, position.y},
        {position.x + size.x, position.y + size.y},
        {position.x, position.y + size.y},
    };
    const Widgets::Vec2 uvs[4] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};

    std::vector<ImageVertex> vertices;
    vertices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        vertices.push_back(ImageVertex{
            .Position = corners[i],
            .Local = {corners[i].x - center.x, corners[i].y - center.y},
            .HalfSize = halfSize,
            .Radii = Rendering::CornerRadii{},
            .Uv = uvs[i],
            .TintColor = tintColor,
        });
    }
    return vertices;
}

const char* kBlendVertexSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inUv;

out vec2 vUv;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    vUv = inUv;
}
)GLSL";

// One program, a uMode uniform selects the formula -- see NativeRendererGL3::CompositeBlendLayer()'s
// own doc comment for why real blend modes need both textures (this shader can't be expressed as
// fixed-function glBlendFunc factors). Mode indices match Rendering::BlendMode's declaration order
// exactly (CommandBuffer.hpp's own comment on that enum says not to reorder it). Both textures are
// this renderer's usual premultiplied-by-coverage convention; each blend formula operates on
// unpremultiplied ("straight") colors per the standard CSS/PDF compositing model, so the shader
// unpremultiplies both inputs before blending and recomposites the (correctly premultiplied) result
// with the standard "simple/non-isolated" Porter-Duff formula (W3C Compositing and Blending Level 1).
const char* kBlendFragmentSource = R"GLSL(
#version 330 core

in vec2 vUv;
out vec4 outColor;

uniform sampler2D uSourceTexture;
uniform sampler2D uBackdropTexture;
uniform int       uMode;

vec3 Multiply(vec3 cb, vec3 cs) { return cb * cs; }
vec3 Screen(vec3 cb, vec3 cs) { return cb + cs - cb * cs; }

vec3 Overlay(vec3 cb, vec3 cs) {
    vec3 lo = 2.0 * cb * cs;
    vec3 hi = vec3(1.0) - 2.0 * (vec3(1.0) - cb) * (vec3(1.0) - cs);
    return mix(lo, hi, step(0.5, cb)); // branches on the BACKDROP's own brightness
}

vec3 HardLight(vec3 cb, vec3 cs) {
    vec3 lo = 2.0 * cb * cs;
    vec3 hi = vec3(1.0) - 2.0 * (vec3(1.0) - cb) * (vec3(1.0) - cs);
    return mix(lo, hi, step(0.5, cs)); // branches on the SOURCE's own brightness
}

vec3 ColorDodge(vec3 cb, vec3 cs) {
    return min(vec3(1.0), cb / max(vec3(1.0) - cs, vec3(1e-4)));
}

vec3 ColorBurn(vec3 cb, vec3 cs) {
    return vec3(1.0) - min(vec3(1.0), (vec3(1.0) - cb) / max(cs, vec3(1e-4)));
}

vec3 SoftLightD(vec3 x) {
    vec3 poly = ((16.0 * x - 12.0) * x + 4.0) * x;
    vec3 sq = sqrt(x);
    return mix(poly, sq, step(0.25, x));
}

vec3 SoftLight(vec3 cb, vec3 cs) {
    vec3 dark = cb - (vec3(1.0) - 2.0 * cs) * cb * (vec3(1.0) - cb);
    vec3 light = cb + (2.0 * cs - vec3(1.0)) * (SoftLightD(cb) - cb);
    return mix(dark, light, step(0.5, cs));
}

vec3 Blend(vec3 cb, vec3 cs, int mode) {
    if (mode == 1) return Multiply(cb, cs);
    if (mode == 2) return Screen(cb, cs);
    if (mode == 3) return Overlay(cb, cs);
    if (mode == 4) return min(cb, cs);              // Darken
    if (mode == 5) return max(cb, cs);               // Lighten
    if (mode == 6) return ColorDodge(cb, cs);
    if (mode == 7) return ColorBurn(cb, cs);
    if (mode == 8) return HardLight(cb, cs);
    if (mode == 9) return SoftLight(cb, cs);
    if (mode == 10) return abs(cb - cs);             // Difference
    if (mode == 11) return cb + cs - 2.0 * cb * cs;  // Exclusion
    return cs;                                       // Normal (mode == 0)
}

void main() {
    vec4 src = texture(uSourceTexture, vUv);
    vec4 backdrop = texture(uBackdropTexture, vUv);

    float srcAlpha = src.a;
    float backdropAlpha = backdrop.a;
    vec3 cs = srcAlpha > 0.0 ? src.rgb / srcAlpha : vec3(0.0);
    vec3 cb = backdropAlpha > 0.0 ? backdrop.rgb / backdropAlpha : vec3(0.0);

    vec3 blended = Blend(cb, cs, uMode);

    // W3C Compositing and Blending Level 1, "simple alpha compositing" formula, premultiplied output.
    vec3 resultRgb = (1.0 - backdropAlpha) * srcAlpha * cs
                    + backdropAlpha * srcAlpha * blended
                    + (1.0 - srcAlpha) * backdropAlpha * cb;
    float resultAlpha = srcAlpha + backdropAlpha - srcAlpha * backdropAlpha;

    outColor = vec4(resultRgb, resultAlpha);
}
)GLSL";

/// NDC-space fullscreen quad, UV in [0,1] — same shape as `BlurPassGL3`'s own private fullscreen
/// quad (Phase 34.3); not shared across the two classes, matching this codebase's established
/// per-owner GL-resource convention (each class that draws a fullscreen quad owns its own VAO/VBO).
constexpr std::array<float, 24> kBlendFullscreenQuad = {
    // clang-format off
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
    // clang-format on
};

} // namespace

NativeRendererGL3::NativeRendererGL3(RenderMode mode) : _mode(mode) {}

NativeRendererGL3::~NativeRendererGL3() { Shutdown(); }

void NativeRendererGL3::Shutdown() {
    if (_vbo != 0) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
    if (_ebo != 0) { glDeleteBuffers(1, &_ebo); _ebo = 0; }
    if (_vao != 0) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
    if (_rectProgram != 0) { glDeleteProgram(_rectProgram); _rectProgram = 0; }

    if (_imageVbo != 0) { glDeleteBuffers(1, &_imageVbo); _imageVbo = 0; }
    if (_imageEbo != 0) { glDeleteBuffers(1, &_imageEbo); _imageEbo = 0; }
    if (_imageVao != 0) { glDeleteVertexArrays(1, &_imageVao); _imageVao = 0; }
    if (_imageProgram != 0) { glDeleteProgram(_imageProgram); _imageProgram = 0; }

    if (_shadowSilhouetteFbo != 0) { glDeleteFramebuffers(1, &_shadowSilhouetteFbo); _shadowSilhouetteFbo = 0; }
    if (_shadowSilhouetteTex != 0) { glDeleteTextures(1, &_shadowSilhouetteTex); _shadowSilhouetteTex = 0; }
    _shadowSilhouetteWidth = 0;
    _shadowSilhouetteHeight = 0;
    _blurPass.Shutdown();

    for (LayerTarget& target : _layerTargets) {
        if (target.Fbo != 0) { glDeleteFramebuffers(1, &target.Fbo); }
        if (target.Tex != 0) { glDeleteTextures(1, &target.Tex); }
    }
    _layerTargets.clear();
    _layerStack.clear();

    if (_blendVbo != 0) { glDeleteBuffers(1, &_blendVbo); _blendVbo = 0; }
    if (_blendVao != 0) { glDeleteVertexArrays(1, &_blendVao); _blendVao = 0; }
    if (_blendProgram != 0) { glDeleteProgram(_blendProgram); _blendProgram = 0; }
    if (_backdropTex != 0) { glDeleteTextures(1, &_backdropTex); _backdropTex = 0; }
    _backdropWidth = 0;
    _backdropHeight = 0;

    _initialized = false;
}

void NativeRendererGL3::EnsureInitialized() {
    if (_initialized) { return; }

    _rectProgram = LinkProgram(kRectVertexSource, kRectFragmentSource);
    _rectViewportSizeLoc = glGetUniformLocation(_rectProgram, "uViewportSize");

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

    constexpr auto stride = static_cast<GLsizei>(sizeof(RectVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(RectVertex, Position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(RectVertex, Local)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(RectVertex, HalfSize)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(RectVertex, Radii)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(RectVertex, FillColor)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(RectVertex, StrokeColor)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void*>(offsetof(RectVertex, StrokeWidth)));

    glBindVertexArray(0);

    _imageProgram = LinkProgram(kImageVertexSource, kImageFragmentSource);
    _imageViewportSizeLoc = glGetUniformLocation(_imageProgram, "uViewportSize");
    _imageTextureLoc      = glGetUniformLocation(_imageProgram, "uTexture");

    glGenVertexArrays(1, &_imageVao);
    glGenBuffers(1, &_imageVbo);
    glGenBuffers(1, &_imageEbo);

    glBindVertexArray(_imageVao);
    glBindBuffer(GL_ARRAY_BUFFER, _imageVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _imageEbo);

    constexpr auto imageStride = static_cast<GLsizei>(sizeof(ImageVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, Position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, Local)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, HalfSize)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, Radii)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, Uv)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, imageStride,
                           reinterpret_cast<void*>(offsetof(ImageVertex, TintColor)));

    glBindVertexArray(0);

    _initialized = true;
}

void NativeRendererGL3::RenderRectBatch(const Batch& batch) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    glBindVertexArray(_vao);
    glUseProgram(_rectProgram);

    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUniform2f(_rectViewportSizeLoc, static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(RectVertex)), vertices.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.Indices.size() * sizeof(std::uint32_t)),
                 batch.Indices.data(), GL_STREAM_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.Indices.size()), GL_UNSIGNED_INT, nullptr);
}

void NativeRendererGL3::RenderImageBatch(const Batch& batch) {
    const auto& vertices = std::get<std::vector<ImageVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    glBindVertexArray(_imageVao);
    glUseProgram(_imageProgram);

    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUniform2f(_imageViewportSizeLoc, static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));

    // batch.Texture's value is a raw GL texture name, not a registry index — see this file's
    // header comment.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(batch.Texture.Value()));
    glUniform1i(_imageTextureLoc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, _imageVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(ImageVertex)), vertices.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _imageEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.Indices.size() * sizeof(std::uint32_t)),
                 batch.Indices.data(), GL_STREAM_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.Indices.size()), GL_UNSIGNED_INT, nullptr);
}

void NativeRendererGL3::EnsureShadowSilhouetteTarget(int width, int height) {
    if (_shadowSilhouetteFbo != 0 && _shadowSilhouetteWidth == width && _shadowSilhouetteHeight == height) {
        return;
    }

    if (_shadowSilhouetteFbo != 0) { glDeleteFramebuffers(1, &_shadowSilhouetteFbo); }
    if (_shadowSilhouetteTex != 0) { glDeleteTextures(1, &_shadowSilhouetteTex); }

    glGenTextures(1, &_shadowSilhouetteTex);
    glBindTexture(GL_TEXTURE_2D, _shadowSilhouetteTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &_shadowSilhouetteFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _shadowSilhouetteFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _shadowSilhouetteTex, 0);
    IMF_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    _shadowSilhouetteWidth  = width;
    _shadowSilhouetteHeight = height;
}

void NativeRendererGL3::RenderShadowBatch(const Batch& batch) {
    const auto& shadows = std::get<std::vector<ShadowVertex>>(batch.Vertices);
    if (shadows.empty()) { return; }
    const ShadowVertex& shadow = shadows.front();

    // Spread grows the silhouette outward on all sides before blurring -- matches
    // Rendering::DrawShadow::Spread's documented meaning (see CommandBuffer.hpp).
    const float spreadWidth  = shadow.Size.x + 2.0f * shadow.Spread;
    const float spreadHeight = shadow.Size.y + 2.0f * shadow.Spread;
    if (spreadWidth <= 0.0f || spreadHeight <= 0.0f) { return; }

    // Pad the offscreen silhouette texture by the blur radius on every side so BlurPassGL3's
    // kernel (which samples up to `radius` texels either way) has real content to read at the
    // silhouette's own edges, instead of clamped-edge repeats of the boundary pixel.
    const float pad = std::max(shadow.BlurRadius, 0.0f);
    const int texWidth  = std::max(1, static_cast<int>(std::ceil(spreadWidth + 2.0f * pad)));
    const int texHeight = std::max(1, static_cast<int>(std::ceil(spreadHeight + 2.0f * pad)));

    // Save the framebuffer/viewport this call disturbs -- RenderShadowBatch() renders into its
    // own silhouette FBO and (via _blurPass) its own ping-pong targets mid-sequence, then must
    // hand control back to whatever framebuffer/viewport DrawBatches()'s caller had bound, so the
    // next batch in the same loop (if any) keeps drawing into the right target.
    int previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    int previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    EnsureShadowSilhouetteTarget(texWidth, texHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, _shadowSilhouetteFbo);
    glViewport(0, 0, texWidth, texHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND); // a single opaque-white shape on a cleared target -- nothing to blend against.

    // Fill colour is opaque white: RenderRectBatch()'s shader premultiplies by its own AA coverage
    // (`color = vFillColor * fillAlpha`), so with FillColor={1,1,1,1} the rendered RGBA channels
    // all equal the shape's coverage at that pixel -- exactly the mask BlurPassGL3 needs to blur,
    // and exactly what the composite pass below re-tints with the shadow's real color.
    Batch silhouetteBatch;
    silhouetteBatch.Kind = BatchKind::Rect;
    silhouetteBatch.Vertices =
        BuildRectQuadVertices({pad, pad}, {spreadWidth, spreadHeight}, shadow.Radii, {1.0f, 1.0f, 1.0f, 1.0f});
    silhouetteBatch.Indices = {0, 1, 2, 0, 2, 3};
    RenderRectBatch(silhouetteBatch);

    const unsigned int blurredTexture = _blurPass.Apply(_shadowSilhouetteTex, texWidth, texHeight, shadow.BlurRadius);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(previousFbo));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // DrawBatches() set this before the loop began;
                                                        // restored here since the silhouette pass above disabled it.

    // Top-left of the padded silhouette texture, in the shape's own coordinate space, plus the
    // shadow's drop offset -- per PHASE_34_PROPOSAL.md's DrawShadow section ("composite it behind
    // the shape at the specified offset").
    const Widgets::Vec2 compositePosition{
        shadow.Position.x - shadow.Spread - pad + shadow.Offset.x,
        shadow.Position.y - shadow.Spread - pad + shadow.Offset.y,
    };
    const Widgets::Vec2 compositeSize{static_cast<float>(texWidth), static_cast<float>(texHeight)};

    Batch compositeBatch;
    compositeBatch.Kind = BatchKind::Image;
    compositeBatch.Texture = Rendering::TextureId(static_cast<std::uint64_t>(blurredTexture));
    compositeBatch.Vertices = BuildImageQuadVertices(compositePosition, compositeSize, shadow.ShadowColor);
    compositeBatch.Indices = {0, 1, 2, 0, 2, 3};
    RenderImageBatch(compositeBatch);
}

void NativeRendererGL3::EnsureLayerTarget(std::size_t depth, int width, int height) {
    if (_layerTargets.size() <= depth) { _layerTargets.resize(depth + 1); }

    LayerTarget& target = _layerTargets[depth];
    if (target.Fbo != 0 && target.Width == width && target.Height == height) { return; }

    if (target.Fbo != 0) { glDeleteFramebuffers(1, &target.Fbo); }
    if (target.Tex != 0) { glDeleteTextures(1, &target.Tex); }

    glGenTextures(1, &target.Tex);
    glBindTexture(GL_TEXTURE_2D, target.Tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &target.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.Tex, 0);
    IMF_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    target.Width  = width;
    target.Height = height;
}

void NativeRendererGL3::PushLayer(LayerOp op, float opacity, Rendering::BlendMode mode) {
    int previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    int previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    const int width  = previousViewport[2];
    const int height = previousViewport[3];
    const std::size_t depth = _layerStack.size();
    EnsureLayerTarget(depth, width, height);

    LayerFrame frame;
    frame.Op = op;
    frame.Opacity = opacity;
    frame.Mode = mode;
    frame.ParentFbo = static_cast<unsigned int>(previousFbo);
    frame.ParentViewport[0] = previousViewport[0];
    frame.ParentViewport[1] = previousViewport[1];
    frame.ParentViewport[2] = previousViewport[2];
    frame.ParentViewport[3] = previousViewport[3];
    frame.TargetIndex = depth;
    frame.Width  = width;
    frame.Height = height;
    _layerStack.push_back(frame);

    const LayerTarget& target = _layerTargets[depth];
    glBindFramebuffer(GL_FRAMEBUFFER, target.Fbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void NativeRendererGL3::CompositeOpacityLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    int previousBlendSrcRgb = 0, previousBlendDstRgb = 0, previousBlendSrcAlpha = 0, previousBlendDstAlpha = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    const bool blendWasEnabled = glIsEnabled(GL_BLEND) != 0;

    glEnable(GL_BLEND);
    // Correct premultiplied-alpha "over" compositing -- the layer's own texture is already
    // premultiplied by its own coverage/alpha, unlike DrawBatches()'s default
    // (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), which assumes a non-premultiplied source. See
    // .claude/DECISIONS.md's Phase 34.5 entry for the derivation of why this specifically matters
    // here (Opacity < 1 is this feature's entire point, unlike the ~1px AA-edge case elsewhere in
    // this file where the two formulas' difference is negligible and left as-is).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // TintColor = (Opacity,Opacity,Opacity,Opacity) scales every channel of the already-
    // premultiplied source texture uniformly by Opacity -- correctly dims a premultiplied color's
    // effective opacity (kImageFragmentSource computes texture(uTexture,uv) * TintColor * mask,
    // mask == 1 here since Radii is all zero -- an unrounded, full-viewport quad).
    Batch compositeBatch;
    compositeBatch.Kind = BatchKind::Image;
    compositeBatch.Texture = Rendering::TextureId(static_cast<std::uint64_t>(target.Tex));
    compositeBatch.Vertices = BuildImageQuadVertices({0.0f, 0.0f},
                                                      {static_cast<float>(frame.Width), static_cast<float>(frame.Height)},
                                                      {frame.Opacity, frame.Opacity, frame.Opacity, frame.Opacity});
    compositeBatch.Indices = {0, 1, 2, 0, 2, 3};
    RenderImageBatch(compositeBatch);

    glBlendFuncSeparate(static_cast<unsigned int>(previousBlendSrcRgb), static_cast<unsigned int>(previousBlendDstRgb),
                         static_cast<unsigned int>(previousBlendSrcAlpha),
                         static_cast<unsigned int>(previousBlendDstAlpha));
    if (!blendWasEnabled) { glDisable(GL_BLEND); }
}

void NativeRendererGL3::EnsureBackdropTarget(int width, int height) {
    if (_backdropTex != 0 && _backdropWidth == width && _backdropHeight == height) { return; }

    if (_backdropTex != 0) { glDeleteTextures(1, &_backdropTex); }

    glGenTextures(1, &_backdropTex);
    glBindTexture(GL_TEXTURE_2D, _backdropTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    _backdropWidth  = width;
    _backdropHeight = height;
}

void NativeRendererGL3::EnsureBlendProgram() {
    if (_blendProgram != 0) { return; }

    _blendProgram = LinkProgram(kBlendVertexSource, kBlendFragmentSource);
    _blendSourceLoc = glGetUniformLocation(_blendProgram, "uSourceTexture");
    _blendBackdropLoc = glGetUniformLocation(_blendProgram, "uBackdropTexture");
    _blendModeLoc = glGetUniformLocation(_blendProgram, "uMode");

    glGenVertexArrays(1, &_blendVao);
    glGenBuffers(1, &_blendVbo);

    glBindVertexArray(_blendVao);
    glBindBuffer(GL_ARRAY_BUFFER, _blendVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kBlendFullscreenQuad), kBlendFullscreenQuad.data(), GL_STATIC_DRAW);

    constexpr auto stride = static_cast<GLsizei>(4 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

void NativeRendererGL3::CompositeBlendLayer(const LayerFrame& frame) {
    const LayerTarget& target = _layerTargets[frame.TargetIndex];

    EnsureBlendProgram();
    EnsureBackdropTarget(frame.Width, frame.Height);

    // Copy the parent target's CURRENT content (it's already bound -- PopLayer() rebinds it
    // before calling this) into _backdropTex, since a fragment shader has no other way to read a
    // framebuffer's own existing pixel value. See this method's own .hpp doc comment.
    glBindTexture(GL_TEXTURE_2D, _backdropTex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, frame.Width, frame.Height);

    const bool blendWasEnabled = glIsEnabled(GL_BLEND) != 0;
    // The shader itself computes the full Porter-Duff-composited result (both textures already
    // incorporated) -- no fixed-function blending should run on top of its output.
    glDisable(GL_BLEND);

    glUseProgram(_blendProgram);
    glBindVertexArray(_blendVao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, target.Tex);
    glUniform1i(_blendSourceLoc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _backdropTex);
    glUniform1i(_blendBackdropLoc, 1);
    glUniform1i(_blendModeLoc, static_cast<int>(frame.Mode));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Leave the active texture unit at TEXTURE0 (whatever it's bound to is irrelevant -- whichever
    // Render*Batch() draws next always rebinds it) so DrawBatches()'s own outer restore, which only
    // tracks TEXTURE0, ends up correct regardless of what ran in between.
    glActiveTexture(GL_TEXTURE0);
    if (blendWasEnabled) { glEnable(GL_BLEND); }
}

void NativeRendererGL3::PopLayer() {
    // A PopLayer with no matching Push{Opacity,Blend}Layer is a malformed Rendering::CommandBuffer
    // -- a caller bug, not a runtime condition this internal renderer recovers from.
    IMF_ASSERT(!_layerStack.empty());

    const LayerFrame frame = _layerStack.back();
    _layerStack.pop_back();

    glBindFramebuffer(GL_FRAMEBUFFER, frame.ParentFbo);
    glViewport(frame.ParentViewport[0], frame.ParentViewport[1], frame.ParentViewport[2], frame.ParentViewport[3]);

    if (frame.Op == LayerOp::PushOpacity) {
        CompositeOpacityLayer(frame);
    } else {
        CompositeBlendLayer(frame);
    }
}

void NativeRendererGL3::HandleLayerMarker(const Batch& batch) {
    const auto& markers = std::get<std::vector<LayerVertex>>(batch.Vertices);
    if (markers.empty()) { return; }
    const LayerVertex& marker = markers.front();

    switch (marker.Op) {
        case LayerOp::PushOpacity:
        case LayerOp::PushBlend:
            PushLayer(marker.Op, marker.Opacity, marker.Mode);
            break;
        case LayerOp::Pop:
            PopLayer();
            break;
    }
}

void NativeRendererGL3::DrawBatches(const std::vector<Batch>& batches) {
    // Save GL state this call touches so a caller mixing NativeRendererGL3 output with other GL/ImGui
    // drawing in the same frame gets it back unchanged. In RenderMode::DeferredReplay this runs
    // inside an ImDrawList callback invoked by ImGui_ImplOpenGL3_RenderDrawData() — that function
    // also emits its own ImDrawCallback_ResetRenderState entry right after ours (see RenderDeferred()),
    // but restoring here too keeps DrawBatches() correct standalone, independent of what follows it.
    int previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    int previousVao = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    const bool blendWasEnabled = glIsEnabled(GL_BLEND) != 0;
    int previousBlendSrcRgb = 0, previousBlendDstRgb = 0, previousBlendSrcAlpha = 0, previousBlendDstAlpha = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    int previousActiveTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    int previousTexture0 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Batches can interleave Rect/Image/Text (BatchBuilder flushes on command-type change), so each
    // Render*Batch() binds its own program/VAO/viewport uniform rather than this loop assuming
    // one kind for the whole buffer, the way it could when only BatchKind::Rect existed.
    for (const Batch& batch : batches) {
        switch (batch.Kind) {
            case BatchKind::Rect:
                RenderRectBatch(batch);
                break;
            case BatchKind::Image:
                RenderImageBatch(batch);
                break;
            case BatchKind::Shadow:
                RenderShadowBatch(batch);
                break;
            case BatchKind::Layer:
                HandleLayerMarker(batch);
                break;
            case BatchKind::Text:
                // A BatchKind::Text batch only exists here at all when _textRenderer's own
                // ITextLayoutProvider (handed to _batchBuilder in Render(), below) resolved it --
                // so _textRenderer is guaranteed non-null whenever this case is reached. The null
                // check stays anyway: a stray Text batch with no attached renderer is silently
                // dropped rather than misrouted into RenderImageBatch(), which would crash via
                // std::get<vector<ImageVertex>> on a std::vector<TextVertex>.
                if (_textRenderer != nullptr) { _textRenderer->RenderTextBatch(batch); }
                break;
        }
    }

    // A PopLayer with no matching Push{Opacity,Blend}Layer (or vice versa) is a malformed
    // Rendering::CommandBuffer -- a caller bug, not something this internal renderer recovers
    // from. A well-formed buffer always ends with every pushed layer popped, which itself already
    // restores the original framebuffer/viewport (see PushLayer()/PopLayer()), so nothing else
    // here needs to special-case a nonempty stack beyond catching it in debug builds.
    IMF_ASSERT(_layerStack.empty());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(previousTexture0));
    glActiveTexture(static_cast<unsigned int>(previousActiveTexture));
    glBindVertexArray(static_cast<unsigned int>(previousVao));
    glUseProgram(static_cast<unsigned int>(previousProgram));
    glBlendFuncSeparate(static_cast<unsigned int>(previousBlendSrcRgb), static_cast<unsigned int>(previousBlendDstRgb),
                         static_cast<unsigned int>(previousBlendSrcAlpha),
                         static_cast<unsigned int>(previousBlendDstAlpha));
    if (!blendWasEnabled) { glDisable(GL_BLEND); }
}

void NativeRendererGL3::RenderImmediate() { DrawBatches(_batchBuilder.Batches()); }

void NativeRendererGL3::ExecuteDeferredDraw(const ImDrawList* /*parentList*/, const ImDrawCmd* cmd) {
    // Takes ownership back and frees it on return — ImGui invokes this callback exactly once,
    // when it rasterizes the draw command this callback was attached to (see RenderDeferred()).
    std::unique_ptr<PendingDraw> pending(static_cast<PendingDraw*>(cmd->UserCallbackData));
    pending->Self->DrawBatches(pending->Batches);
}

void NativeRendererGL3::RenderDeferred() {
    // Owns a copy of the batches, not a reference into _batchBuilder — the next Render() call
    // (next frame) rebuilds _batchBuilder's internal storage in place, which would otherwise race
    // against this callback if it hadn't fired yet (see PendingDraw's own comment).
    auto* pending  = new PendingDraw{this, _batchBuilder.Batches()};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCallback(&NativeRendererGL3::ExecuteDeferredDraw, pending);
    // Tells ImGui_ImplOpenGL3_RenderDrawData() to re-bind its own shader/VAO/blend state after our
    // callback runs, so the ImGui draw commands that follow in this same window are unaffected by
    // whatever DrawBatches() just bound.
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

Result<Rendering::FontId> NativeRendererGL3::LoadFont(const Utility::Path& path, float sizePixels) {
    if (_textRenderer == nullptr) { return std::unexpected(Error::FontLoadFailed); }
    return _textRenderer->LoadFont(path, sizePixels);
}

void NativeRendererGL3::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();
    _batchBuilder.SetTextLayoutProvider(_textRenderer != nullptr ? _textRenderer->LayoutProvider() : nullptr);
    _batchBuilder.Build(buffer);

    if (_mode == RenderMode::DeferredReplay) {
        RenderDeferred();
    } else {
        RenderImmediate();
    }
}

} // namespace ImFrame::Internal
