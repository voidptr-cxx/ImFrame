/**
 * @file     NativeRendererGL3.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real OpenGL 3.3 draw calls
 *
 * @internal
 * The first real `NativeRenderer` backend (Phase 32.4) — unlike
 * `Internal::ImGuiCompatRenderer`, this issues genuine `glDrawElements()`
 * calls against a hand-written shader, not `ImDrawList` calls. Scoped to
 * `Internal::BatchKind::Rect` only. `DrawPath` still has no live producer
 * anywhere in the tree, so CPU polyline tesselation stays speculative.
 * `DrawImage` is a different story since Phase 32.8: `Widgets::Image`'s
 * non-interactive `Paint()` path now pushes real `DrawImage` commands, so an
 * `Image`-kind batch here is a genuinely reachable runtime path today (e.g.
 * `app.UseRenderer(make_unique<NativeRendererGL3>(...))` plus any
 * non-interactive `ImageWidget` in the tree), not a hypothetical one — it
 * still trips an `IMF_ASSERT` rather than being silently dropped or
 * silently mis-rendered, because no GL texture-upload/registry path exists
 * yet to render it correctly (that remains real, undesigned follow-on work —
 * see `.claude/DECISIONS.md`, Phase 32.8).
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

// Forward-declared (not included) to keep <imgui.h> out of this header — only NativeRendererGL3.cpp
// needs it. Both are structs in the global namespace in Dear ImGui (matches the `ImFont*`/
// `ImFontAtlas*` forward-declare precedent in IconFont.hpp — see .claude/DECISIONS.md, Phase 9).
struct ImDrawList;
struct ImDrawCmd;

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
 * `glGetIntegerv(GL_VIEWPORT, ...)` at draw time — `IRenderer::Render()`'s signature is
 * unchanged from Phase 31 (no viewport-size parameter added), matching how every other
 * GL-viewport-dependent call in this codebase already expects the caller to have set the
 * viewport/framebuffer binding correctly beforehand.
 *
 * `RenderMode::DeferredReplay` (Phase 32.6) fixes the compositing bug discovered in Phase 32.5:
 * `RootBridge::BeginRootWindow()`'s `ImGui::Begin()` only *queues* the root window's own
 * background into ImGui's draw list — ImGui does not *rasterize* that queued content until
 * `ImGui::Render()`/`ImGui_ImplOpenGL3_RenderDrawData()` run later, in `EndFrame()`. A
 * `RenderMode::Immediate` draw issued in between (the only mode Phase 32.4/32.5 had) always
 * rasterizes to the framebuffer *before* that deferred background, so the background silently
 * paints over it regardless of program order. `DeferredReplay` instead records the batches and
 * pushes a `ImDrawList::AddCallback()` entry onto the *current* ImGui window's own draw list
 * (obtained via `ImGui::GetWindowDrawList()` — this mode therefore requires an active ImGui
 * window, i.e. must be called between `ImGui::Begin()`/`ImGui::End()`, exactly where
 * `Reconciler::Show()` calls it). `ImGui_ImplOpenGL3_RenderDrawData()` then invokes that
 * callback at the exact point in the draw list where it was recorded, so the native draw
 * rasterizes in the correct position relative to the window's own (deferred) content — after
 * the background, not before it. A trailing `ImDrawCallback_ResetRenderState` entry tells
 * `ImGui_ImplOpenGL3_RenderDrawData()` to re-bind its own GL state afterward, so subsequent
 * ImGui draw commands in the same list are unaffected.
 *
 * `RenderMode::Immediate` is retained (and remains the default) because it needs no active
 * ImGui window/frame at all — `NativeRendererGL3_test.cpp`'s direct-FBO tests construct a
 * renderer and call `Render()` with no ImGui frame in progress, which `DeferredReplay` cannot
 * support (`ImGui::GetWindowDrawList()` asserts outside `Begin()/End()`).
 *
 * @since    3.0.0
 */
class NativeRendererGL3 final : public IRenderer {
public:
    /// Selects how `Render()` turns batches into GL draw calls — see this class's own comment.
    enum class RenderMode : unsigned char {
        Immediate,      ///< Draws synchronously inside `Render()` (Phase 32.4 behaviour). Default.
        DeferredReplay, ///< Queues an `ImDrawList` callback on the current ImGui window (Phase 32.6).
    };

    /**
     * @brief    Constructs a renderer in the given mode.
     * @param[in] mode  `RenderMode::Immediate` (default) or `RenderMode::DeferredReplay`.
     */
    explicit NativeRendererGL3(RenderMode mode = RenderMode::Immediate);
    ~NativeRendererGL3() override;

    NativeRendererGL3(const NativeRendererGL3&) = delete;
    NativeRendererGL3& operator=(const NativeRendererGL3&) = delete;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Releases all GL resources. Safe to call multiple times, including before any `Render()` call.
    void Shutdown() override;

private:
    void EnsureInitialized();
    void RenderImmediate();
    void RenderDeferred();
    void DrawBatches(const std::vector<Batch>& batches);
    void RenderRectBatch(const Batch& batch);

    /// `ImDrawList::AddCallback()` trampoline for `RenderMode::DeferredReplay` — see NativeRendererGL3.cpp.
    static void ExecuteDeferredDraw(const ImDrawList* parentList, const ImDrawCmd* cmd);

    bool       _initialized = false;
    RenderMode _mode;

    unsigned int _rectProgram          = 0; ///< GLuint linked shader program for BatchKind::Rect.
    int          _rectViewportSizeLoc  = -1; ///< glGetUniformLocation("uViewportSize") cache.

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ebo = 0;

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
