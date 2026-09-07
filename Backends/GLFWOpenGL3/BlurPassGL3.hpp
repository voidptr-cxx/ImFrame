/**
 * @file     BlurPassGL3.hpp
 * @brief    Separable Gaussian blur for `NativeRendererGL3`, implemented as two fragment-shader passes
 *
 * @internal
 * `PHASE_34_PROPOSAL.md`'s Gaussian Blur section is written for compute
 * shaders (two `GaussianBlurH.comp`/`GaussianBlurV.comp` passes) — but
 * `NativeRendererGL3`/`GLFWOpenGL3Backend` target OpenGL 3.3 core exclusively
 * (see `NativeRendererGL3.hpp`'s own file comment), and compute shaders need
 * GL 4.3+. This was flagged as an open architectural wrinkle at the end of
 * Phase 34.2 (`.claude/PHASE_STATUS.md`) and is resolved here per
 * `PHASE_35_PROPOSAL.md`'s own answer for this exact backend: "On OpenGL 3.3
 * (minimum), blur is implemented via fragment shader ping-pong." No GL
 * version bump. See `.claude/DECISIONS.md`, Phase 34.3.
 *
 * `GaussianBlurH.comp`/`GaussianBlurV.comp` are intentionally not added yet —
 * they belong to the compute-capable backends (Vulkan/Metal/DX12/WebGPU)
 * Phase 35 builds, and adding them now with no consumer would be speculative.
 *
 * A single fragment shader program handles both the horizontal and vertical
 * pass (selected per-call via a `uDirection` uniform) rather than two
 * separate programs — the two passes are identical except for which axis
 * they sample along, and `PHASE_34_PROPOSAL.md`'s own weight-precomputation
 * detail ("Kernel weights are precomputed for radii 1-64 pixels and stored
 * as UBO arrays") is written for the compute-shader path; this fragment-
 * shader path computes Gaussian weights inline per fragment instead — a UBO
 * is unnecessary machinery when the shader already has direct access to
 * `uRadius` and can evaluate `exp()` itself, and no measured performance
 * problem motivates the added complexity. Radius is clamped to [0, 64] to
 * match that same proposal's stated supported range; the proposal's "larger
 * radii use multiple passes at reduced resolution" mip-chain optimisation is
 * not implemented — out of scope for this sub-phase, not silently dropped.
 *
 * Blurring the source texture's raw RGBA linearly (no premultiply/unmultiply
 * step) is correct here specifically because `NativeRendererGL3`'s own
 * `DrawRect`/`DrawImage` fragment shaders already write a premultiplied-by-
 * coverage result (`color = vFillColor * fillAlpha`, so RGB and alpha both
 * go to zero together at a fully-transparent AA edge) — there is no
 * unmultiplied-alpha color fringing to correct for, unlike the general case
 * of blurring an arbitrary straight-alpha RGBA image.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-07
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

namespace ImFrame::Internal {

/**
 * @class    BlurPassGL3
 * @brief    Applies a separable Gaussian blur to a GL texture via two fragment-shader ping-pong passes
 *
 * @internal
 * GL resources (shader program, fullscreen-quad VAO/VBO, ping-pong FBOs/
 * textures) are created lazily on the first `Apply()` call, matching
 * `NativeRendererGL3`'s own lazy-initialization convention — constructing an
 * instance never requires a GL context to already exist. The ping-pong
 * targets are (re)allocated only when the requested size changes, so
 * repeated `Apply()` calls at a stable size (the common case, one per
 * widget's shadow/backdrop per frame) allocate nothing after the first.
 *
 * @since    3.0.0
 */
class BlurPassGL3 {
public:
    BlurPassGL3() = default;
    ~BlurPassGL3();

    BlurPassGL3(const BlurPassGL3&) = delete;
    BlurPassGL3& operator=(const BlurPassGL3&) = delete;

    /**
     * @brief    Blurs `sourceTexture` with a separable Gaussian kernel and returns the result.
     * @param[in] sourceTexture  An existing `GL_RGBA8` 2D texture, exactly `width` x `height`. Not modified.
     * @param[in] width          Texture width, in texels. Must match `sourceTexture`'s actual width.
     * @param[in] height         Texture height, in texels. Must match `sourceTexture`'s actual height.
     * @param[in] radius         Blur radius in pixels, clamped to `[0, 64]`. `<= 0` is a no-op.
     * @return   `sourceTexture` unchanged if `radius <= 0`; otherwise a `GL_RGBA8` texture name
     *           owned by this `BlurPassGL3`, valid until the next `Apply()` call or destruction.
     *           The caller must not delete it.
     *
     * Saves and restores every piece of GL state this call touches (bound framebuffer, viewport,
     * program, VAO, active texture unit, `GL_TEXTURE0` binding, blend enable) — safe to call from
     * the middle of another renderer's own draw sequence, matching `NativeRendererGL3::DrawBatches()`'s
     * own save/restore convention.
     */
    [[nodiscard]] unsigned int Apply(unsigned int sourceTexture, int width, int height, float radius);

    /// Releases all GL resources. Safe to call multiple times, including before any `Apply()` call.
    void Shutdown();

private:
    void EnsureInitialized();
    void EnsureTargets(int width, int height);
    void RenderPass(unsigned int inputTexture, unsigned int targetFbo, int width, int height, float directionX,
                     float directionY, float radius);

    bool _initialized = false;

    unsigned int _program      = 0;
    int          _textureLoc   = -1;
    int          _texelSizeLoc = -1;
    int          _directionLoc = -1;
    int          _radiusLoc    = -1;

    unsigned int _vao = 0;
    unsigned int _vbo = 0;

    /// Ping-pong render targets: pass 1 (horizontal) reads `sourceTexture`, writes `_texA`; pass 2
    /// (vertical) reads `_texA`, writes `_texB` (the value `Apply()` returns). Reallocated only
    /// when the requested size differs from `_targetWidth`/`_targetHeight`.
    unsigned int _fboA = 0;
    unsigned int _texA = 0;
    unsigned int _fboB = 0;
    unsigned int _texB = 0;
    int          _targetWidth  = 0;
    int          _targetHeight = 0;
};

} // namespace ImFrame::Internal
