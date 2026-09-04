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
#include <imgui.h>

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
    float mask = 1.0 - smoothstep(-1.0, 1.0, RoundedBoxSdf(vLocal, vHalfSize, r));
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
