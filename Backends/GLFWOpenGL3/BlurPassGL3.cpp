/**
 * @file     BlurPassGL3.cpp
 * @brief    `BlurPassGL3` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-07
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "BlurPassGL3.hpp"

#include "ImFrame/Core/Error.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>

namespace ImFrame::Internal {

namespace {

const char* kBlurVertexSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inUv;

out vec2 vUv;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    vUv = inUv;
}
)GLSL";

// One program handles both the horizontal and vertical pass -- uDirection selects the sampling
// axis (see BlurPassGL3.hpp's file comment for why weights are computed inline instead of via a
// precomputed UBO). uRadius is pre-clamped to [0, 64] in C++ before upload; the loop bound below
// is itself dynamic (a GLSL fragment shader may branch/loop on a uniform-derived value), not
// unrolled at compile time.
const char* kBlurFragmentSource = R"GLSL(
#version 330 core

in vec2 vUv;
out vec4 outColor;

uniform sampler2D uTexture;
uniform vec2      uTexelSize;
uniform vec2      uDirection;
uniform float     uRadius;

void main() {
    int   radius = int(uRadius);
    float sigma  = max(uRadius * 0.5, 1e-4);
    float twoSigmaSq = 2.0 * sigma * sigma;

    vec4  sum = vec4(0.0);
    float weightSum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        float weight = exp(-float(i * i) / twoSigmaSq);
        vec2  offset = uDirection * uTexelSize * float(i);
        sum += texture(uTexture, vUv + offset) * weight;
        weightSum += weight;
    }
    outColor = sum / weightSum;
}
)GLSL";

/// NDC-space fullscreen quad (two triangles), UV in [0,1] with V flipped to match GL's
/// bottom-left texture origin -- matches every other real-pixel test/renderer in this codebase
/// that already accounts for this (e.g. `ScratchFramebuffer::ReadPixels()`'s row-flip).
constexpr std::array<float, 24> kFullscreenQuad = {
    // clang-format off
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
    // clang-format on
};

[[nodiscard]] unsigned int CompileShaderStage(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    IMF_ASSERT(compiled != 0); // a compile failure here is a bug in the hardcoded source above.
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

/// Allocates (or reallocates) one `GL_RGBA8` render-target texture + FBO pair at `width`x`height`.
void CreateTarget(unsigned int& fbo, unsigned int& tex, int width, int height) {
    if (fbo != 0) { glDeleteFramebuffers(1, &fbo); }
    if (tex != 0) { glDeleteTextures(1, &tex); }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // CLAMP_TO_EDGE -- the default GL_REPEAT would pull in opposite-edge content for taps near a
    // border, since the blur kernel samples outside [0,1] UV range for edge/corner texels.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    IMF_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

} // namespace

BlurPassGL3::~BlurPassGL3() { Shutdown(); }

void BlurPassGL3::Shutdown() {
    if (_fboA != 0) { glDeleteFramebuffers(1, &_fboA); _fboA = 0; }
    if (_texA != 0) { glDeleteTextures(1, &_texA); _texA = 0; }
    if (_fboB != 0) { glDeleteFramebuffers(1, &_fboB); _fboB = 0; }
    if (_texB != 0) { glDeleteTextures(1, &_texB); _texB = 0; }
    _targetWidth  = 0;
    _targetHeight = 0;

    if (_vbo != 0) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
    if (_vao != 0) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
    if (_program != 0) { glDeleteProgram(_program); _program = 0; }

    _initialized = false;
}

void BlurPassGL3::EnsureInitialized() {
    if (_initialized) { return; }

    _program      = LinkProgram(kBlurVertexSource, kBlurFragmentSource);
    _textureLoc   = glGetUniformLocation(_program, "uTexture");
    _texelSizeLoc = glGetUniformLocation(_program, "uTexelSize");
    _directionLoc = glGetUniformLocation(_program, "uDirection");
    _radiusLoc    = glGetUniformLocation(_program, "uRadius");

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kFullscreenQuad), kFullscreenQuad.data(), GL_STATIC_DRAW);

    constexpr auto stride = static_cast<GLsizei>(4 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);

    _initialized = true;
}

void BlurPassGL3::EnsureTargets(int width, int height) {
    if (_targetWidth == width && _targetHeight == height && _fboA != 0) { return; }

    CreateTarget(_fboA, _texA, width, height);
    CreateTarget(_fboB, _texB, width, height);
    _targetWidth  = width;
    _targetHeight = height;
}

void BlurPassGL3::RenderPass(unsigned int inputTexture, unsigned int targetFbo, int width, int height,
                              float directionX, float directionY, float radius) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, width, height);

    glUseProgram(_program);
    glBindVertexArray(_vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(_textureLoc, 0);

    glUniform2f(_texelSizeLoc, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
    glUniform2f(_directionLoc, directionX, directionY);
    glUniform1f(_radiusLoc, radius);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

unsigned int BlurPassGL3::Apply(unsigned int sourceTexture, int width, int height, float radius) {
    if (radius <= 0.0f) { return sourceTexture; }
    const float clampedRadius = std::min(radius, 64.0f);

    // Save every piece of state the two passes touch BEFORE EnsureInitialized()/EnsureTargets()
    // run -- both lazily create GL objects (CreateTarget() in particular binds each new FBO and
    // leaves it bound), so capturing "previous" state after them would, on the first Apply() call
    // (or any call whose width/height differs from the last, forcing a target reallocation),
    // record one of THIS class's own just-created FBOs as the caller's "previous" framebuffer --
    // silently corrupting the caller's own binding once this function returns. This was a real,
    // shipped bug from Phase 34.3, undetected because RenderShadowBatch() (Phase 34.4) — the only
    // caller until Phase 34.6 — always rebinds its own saved framebuffer itself right after
    // calling Apply(), independently papering over whatever Apply() left behind. Phase 34.6's
    // backdrop-blur composite has no such redundant rebind and exposed it directly. See
    // .claude/DECISIONS.md's Phase 34.6 entry.
    int previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    int previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    int previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    int previousVao = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    int previousActiveTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    int previousTexture0 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    const bool blendWasEnabled = glIsEnabled(GL_BLEND) != 0;

    EnsureInitialized();
    EnsureTargets(width, height);

    // Each pass's fullscreen quad overwrites every texel of its target -- disable blending so the
    // write is a plain replace, not composited against whatever that target previously held.
    glDisable(GL_BLEND);

    RenderPass(sourceTexture, _fboA, width, height, 1.0f, 0.0f, clampedRadius); // horizontal -> _texA
    RenderPass(_texA, _fboB, width, height, 0.0f, 1.0f, clampedRadius);         // vertical   -> _texB

    if (blendWasEnabled) { glEnable(GL_BLEND); }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(previousTexture0));
    glActiveTexture(static_cast<unsigned int>(previousActiveTexture));
    glBindVertexArray(static_cast<unsigned int>(previousVao));
    glUseProgram(static_cast<unsigned int>(previousProgram));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(previousFbo));

    return _texB;
}

} // namespace ImFrame::Internal
