/**
 * @file     NativeRendererGL3.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real OpenGL 3.3 draw calls
 *
 * @internal
 * The first real `NativeRenderer` backend (Phase 32.4) — unlike
 * `Internal::ImGuiCompatRenderer`, this issues genuine `glDrawElements()`
 * calls against a hand-written shader, not `ImDrawList` calls. Scoped to
 * `Internal::BatchKind::Rect` only: `DrawImage`/`DrawPath` have no live
 * producer anywhere in the tree yet (same reasoning as Phase 32.3's
 * `BatchBuilder` scoping — see `.claude/DECISIONS.md`, Phase 32.4), so
 * building GL texture-upload/path-tesselation support now would be
 * speculative. An `Image`-kind batch trips an `IMF_ASSERT` rather than being
 * silently dropped or silently mis-rendered.
 *
 * Lives in `Backends/GLFWOpenGL3/`, not `src/Rendering/Renderers/Backends/`
 * as `PHASE_32_PROPOSAL.md`'s literal "New Files" list states — it needs
 * `<glad/glad.h>`, and this project's own architecture invariants restrict
 * GL/platform headers to `Backends/` (`include/ImFrame/` and `src/` must stay
 * backend-agnostic). A documented deviation, not an oversight — see
 * `.claude/DECISIONS.md`, Phase 32.4.
 *
 * Its shaders are hand-written `#version 330 core` GLSL, compiled at runtime
 * via the normal `glCompileShader()` API — **not** the Phase 32.2
 * `glslang`/SPIR-V build-time pipeline. `Shaders/Rect.glsl` targets
 * `#version 450` with explicit `layout(binding=N)` qualifiers (required for
 * Vulkan-semantics SPIR-V generation); this backend's actual GL context is
 * requested as 3.3 core (`GLFWOpenGL3Backend.cpp`), which does not support
 * those qualifiers without an extension. The two shaders implement the same
 * rounded-box SDF logic, kept in sync by hand — matching the same
 * documented duplication already accepted between `Rect.glsl` and
 * `Image.glsl` themselves.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

namespace ImFrame::Internal {

/**
 * @class    NativeRendererGL3
 * @brief    Renders `Rendering::DrawRect` batches via real OpenGL 3.3 draw calls
 *
 * @internal
 * Must be called with an OpenGL 3.3+ core-profile context current on the calling thread
 * (matching every other GL-touching type in this codebase). GL resources (shader program,
 * VAO/VBO/EBO) are created lazily on the first `Render()` call rather than in the constructor,
 * so constructing an instance never requires a context to already exist.
 *
 * The target framebuffer/viewport is whatever is currently bound and reported by
 * `glGetIntegerv(GL_VIEWPORT, ...)` at `Render()` time — `IRenderer::Render()`'s signature is
 * unchanged from Phase 31 (no viewport-size parameter added), matching how every other
 * GL-viewport-dependent call in this codebase already expects the caller to have set the
 * viewport/framebuffer binding correctly beforehand.
 *
 * @since    3.0.0
 */
class NativeRendererGL3 final : public IRenderer {
public:
    NativeRendererGL3() = default;
    ~NativeRendererGL3() override;

    NativeRendererGL3(const NativeRendererGL3&) = delete;
    NativeRendererGL3& operator=(const NativeRendererGL3&) = delete;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Releases all GL resources. Safe to call multiple times, including before any `Render()` call.
    void Shutdown() override;

private:
    void EnsureInitialized();
    void RenderRectBatch(const Batch& batch);

    bool _initialized = false;

    unsigned int _rectProgram          = 0; ///< GLuint linked shader program for BatchKind::Rect.
    int          _rectViewportSizeLoc  = -1; ///< glGetUniformLocation("uViewportSize") cache.

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ebo = 0;

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
