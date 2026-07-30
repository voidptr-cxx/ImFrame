/**
 * @file     NativeRendererGL3.cpp
 * @brief    `NativeRendererGL3` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "NativeRendererGL3.hpp"

#include "ImFrame/Core/Error.hpp"

#include <glad/glad.h>

#include <cstddef>

namespace ImFrame::Internal {

namespace {

// Hand-written #version 330 core equivalent of Shaders/Rect.glsl's rounded-box SDF logic — see
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

    float fillAlpha = 1.0 - smoothstep(-1.0, 1.0, dist);
    vec4 color = vFillColor * fillAlpha;

    if (vStrokeWidth > 0.0) {
        float strokeDist = abs(dist) - vStrokeWidth * 0.5;
        float strokeAlpha = 1.0 - smoothstep(-1.0, 1.0, strokeDist);
        color = mix(color, vStrokeColor, strokeAlpha * vStrokeColor.a);
    }

    outColor = color;
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

} // namespace

NativeRendererGL3::~NativeRendererGL3() { Shutdown(); }

void NativeRendererGL3::Shutdown() {
    if (_vbo != 0) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
    if (_ebo != 0) { glDeleteBuffers(1, &_ebo); _ebo = 0; }
    if (_vao != 0) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
    if (_rectProgram != 0) { glDeleteProgram(_rectProgram); _rectProgram = 0; }
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

    _initialized = true;
}

void NativeRendererGL3::RenderRectBatch(const Batch& batch) {
    const auto& vertices = std::get<std::vector<RectVertex>>(batch.Vertices);
    if (vertices.empty()) { return; }

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(RectVertex)), vertices.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.Indices.size() * sizeof(std::uint32_t)),
                 batch.Indices.data(), GL_STREAM_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.Indices.size()), GL_UNSIGNED_INT, nullptr);
}

void NativeRendererGL3::Render(const Rendering::CommandBuffer& buffer) {
    EnsureInitialized();

    // Save GL state this call touches so a caller mixing NativeRendererGL3 output with other GL/ImGui
    // drawing in the same frame gets it back unchanged — the Phase 32.1 "how does NativeRenderer output
    // composite with direct ImGui calls in the same frame" question (see .claude/DECISIONS.md) is still
    // open, but leaving GL state dirty would make that question strictly worse, not just unresolved.
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

    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const auto viewportWidth = static_cast<float>(viewport[2]);
    const auto viewportHeight = static_cast<float>(viewport[3]);

    _batchBuilder.Build(buffer);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(_rectProgram);
    glUniform2f(_rectViewportSizeLoc, viewportWidth, viewportHeight);
    glBindVertexArray(_vao);

    for (const Batch& batch : _batchBuilder.Batches()) {
        if (batch.Kind == BatchKind::Rect) {
            RenderRectBatch(batch);
        } else {
            // BatchKind::Image has no live producer yet (see this file's header comment) —
            // loud in test/debug builds rather than silently dropping the batch.
            IMF_ASSERT(false && "NativeRendererGL3: Image batches are not yet supported (Phase 32.4 scope)");
        }
    }

    glBindVertexArray(static_cast<unsigned int>(previousVao));
    glUseProgram(static_cast<unsigned int>(previousProgram));
    glBlendFuncSeparate(static_cast<unsigned int>(previousBlendSrcRgb), static_cast<unsigned int>(previousBlendDstRgb),
                         static_cast<unsigned int>(previousBlendSrcAlpha),
                         static_cast<unsigned int>(previousBlendDstAlpha));
    if (!blendWasEnabled) { glDisable(GL_BLEND); }
}

} // namespace ImFrame::Internal
