/**
 * @file     NativeRendererGL3.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real OpenGL 3.3 draw calls
 *
 * @internal
 * The first real `NativeRenderer` backend (Phase 32.4) — unlike
 * `Internal::ImGuiCompatRenderer`, this issues genuine `glDrawElements()`
 * calls against hand-written shaders, not `ImDrawList` calls. Supports
 * `BatchKind::Rect` (Phase 32.4), `BatchKind::Image` (Phase 32.9),
 * `BatchKind::Shadow` (Phase 34.4, via `RenderShadowBatch()` — render the
 * shadow's silhouette into an offscreen texture, blur it with `_blurPass`,
 * then composite the tinted result behind the shape at its offset, per
 * `PHASE_34_PROPOSAL.md`'s `DrawShadow` section), `BatchKind::Layer`
 * (Phase 34.5, via `PushLayer()`/`PopLayer()` — see those methods' own
 * comments for the offscreen-layer-stack design), and `BatchKind::BackdropBlur`
 * (Phase 34.6, via `RenderBackdropBlurBatch()` — copy the region behind the
 * blur into a texture, blur it, composite it back in place, cropped to the
 * requested rect; rate-limited per `Render()` call by `MaxBackdropBlurPerFrame`).
 * `DrawPath` still has no live producer anywhere in the tree, so CPU
 * polyline tesselation stays speculative and there is no `BatchKind::Path`.
 *
 * `BatchKind::Image` treats `Rendering::TextureId::Value()` as a raw GL
 * texture name (`GLuint`) — the same interpretation `ImGuiCompatRenderer`
 * and ImGui's own OpenGL3 backend already give the identical bit pattern
 * (both ultimately do `glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)id)`).
 * This is not a registry-index scheme (no `TextureAtlas`/`ImageLoader`
 * indirection) — `Widgets::Image` (the only live `DrawImage` producer as of
 * Phase 32.8) already constructs its `TextureId` as a reinterpreted raw GL
 * texture name, so that is the only interpretation consistent with reality
 * today. See `.claude/DECISIONS.md`, Phase 32.9.
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
 * `glslang`/SPIR-V build-time pipeline. `Shaders/SDFRect.glsl` targets
 * `#version 450` with explicit `layout(binding=N)` qualifiers (required for
 * Vulkan-semantics SPIR-V generation); this backend's actual GL context is
 * requested as 3.3 core (`GLFWOpenGL3Backend.cpp`), which does not support
 * those qualifiers without an extension. The two shaders implement the same
 * rounded-box SDF logic, kept in sync by hand — matching the same
 * documented duplication already accepted between `SDFRect.glsl` and
 * `Image.glsl` themselves.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "BlurPassGL3.hpp"
#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

// Forward-declared (not included) to keep <imgui.h> out of this header — only NativeRendererGL3.cpp
// needs it. Both are structs in the global namespace in Dear ImGui (matches the `ImFont*`/
// `ImFontAtlas*` forward-declare precedent in IconFont.hpp — see .claude/DECISIONS.md, Phase 9).
struct ImDrawList;
struct ImDrawCmd;

namespace ImFrame::Internal {

/**
 * @class    ITextRenderer
 * @brief    Pluggable GL3 text-rendering capability `NativeRendererGL3` delegates `BatchKind::Text` to
 *
 * @internal
 * `NativeRendererGL3` itself has zero dependency on `FontRegistry`/`TextShaper`/`GlyphAtlas` — it
 * builds and passes every test with `IMF_BUILD_NATIVE_RENDERER=ON` alone, `IMF_BUILD_TEXT_MSDF`
 * left `OFF` (verified directly across Phase 33.5/33.6). This interface is the seam that makes
 * that possible: the concrete implementation (`TextRendererGL3`, compiled only when both
 * `IMF_BUILD_NATIVE_RENDERER` and `IMF_BUILD_TEXT_MSDF` are enabled) owns the real font pipeline,
 * the glyph-atlas GL texture, and the `MSDFText` GL3 shader; `NativeRendererGL3` only ever calls
 * through this abstract pointer. See `.claude/DECISIONS.md`, Phase 33.7.
 *
 * @since    3.0.0
 */
class ITextRenderer {
public:
    virtual ~ITextRenderer() noexcept = default;

    /// The `ITextLayoutProvider` `NativeRendererGL3` should hand its `BatchBuilder` before each `Build()`.
    [[nodiscard]] virtual ITextLayoutProvider* LayoutProvider() noexcept = 0;

    /// Issues the real GL draw call(s) for one closed `BatchKind::Text` batch.
    virtual void RenderTextBatch(const Batch& batch) = 0;

    /// See `IRenderer::LoadFont()` — `NativeRendererGL3::LoadFont()` forwards to this directly.
    [[nodiscard]] virtual Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) = 0;
};

/**
 * @class    NativeRendererGL3
 * @brief    Renders `Rendering::DrawRect`/`DrawImage` batches via real OpenGL 3.3 draw calls
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

    /// Forwards to the attached `ITextRenderer::LoadFont()`, or `Error::FontLoadFailed` if none is attached.
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all GL resources. Safe to call multiple times, including before any `Render()` call.
    void Shutdown() override;

    /**
     * @brief    Attaches (or clears, with `nullptr`) the `BatchKind::Text` rendering delegate.
     * @param[in] textRenderer  Non-owning; must outlive this `NativeRendererGL3` while attached.
     *                          Unattached (the default) makes `DrawText` behave exactly as it did
     *                          before Phase 33.7 — laid out by nothing, non-batchable, dropped.
     */
    void AttachTextRenderer(ITextRenderer* textRenderer) noexcept { _textRenderer = textRenderer; }

    /**
     * @brief    Sets how many `Rendering::DrawBackdropBlur` operations `Render()` processes per call.
     * @param[in] maxPerFrame  Default 4, per `PHASE_34_PROPOSAL.md`'s Backdrop Blur section.
     *
     * Additional `DrawBackdropBlur` commands beyond this in the same `Render()` call degrade
     * gracefully — that region is left showing its already-rendered (unblurred) content, logged at
     * `Logger::Debug`, rather than failing or unconditionally paying an unbounded per-frame cost
     * for an expensive effect.
     */
    void SetMaxBackdropBlurPerFrame(int maxPerFrame) noexcept { _maxBackdropBlurPerFrame = maxPerFrame; }

private:
    void EnsureInitialized();
    void RenderImmediate();
    void RenderDeferred();
    void DrawBatches(const std::vector<Batch>& batches);
    void RenderRectBatch(const Batch& batch);
    void RenderImageBatch(const Batch& batch);
    void RenderShadowBatch(const Batch& batch);
    void EnsureShadowSilhouetteTarget(int width, int height);
    void HandleLayerMarker(const Batch& batch);
    void RenderBackdropBlurBatch(const Batch& batch);
    void EnsureBackdropBlurCopyTarget(int width, int height);

    /// `ImDrawList::AddCallback()` trampoline for `RenderMode::DeferredReplay` — see NativeRendererGL3.cpp.
    static void ExecuteDeferredDraw(const ImDrawList* parentList, const ImDrawCmd* cmd);

    bool       _initialized = false;
    RenderMode _mode;

    unsigned int _rectProgram          = 0; ///< GLuint linked shader program for BatchKind::Rect.
    int          _rectViewportSizeLoc  = -1; ///< glGetUniformLocation("uViewportSize") cache.

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ebo = 0;

    unsigned int _imageProgram         = 0;  ///< GLuint linked shader program for BatchKind::Image.
    int          _imageViewportSizeLoc = -1;  ///< glGetUniformLocation("uViewportSize") cache.
    int          _imageTextureLoc      = -1;  ///< glGetUniformLocation("uTexture") cache.

    unsigned int _imageVao = 0;
    unsigned int _imageVbo = 0;
    unsigned int _imageEbo = 0;

    /**
     * @brief    Begins a new offscreen compositing layer (Phase 34.5).
     * @param[in] op       `LayerOp::PushOpacity` or `LayerOp::PushBlend` — never `Pop`.
     * @param[in] opacity  Meaningful only when `op == PushOpacity`.
     * @param[in] mode     Meaningful only when `op == PushBlend`.
     *
     * Saves the currently-bound framebuffer/viewport onto `_layerStack` (so the matching
     * `PopLayer()` knows where to composite back into and how), then binds a pooled offscreen
     * target (`_layerTargets[depth]`, same size as the current viewport, lazily (re)allocated by
     * `EnsureLayerTarget()`) cleared to transparent — every batch between this call and the
     * matching `PopLayer()` therefore renders into the new layer instead of wherever `Render()`'s
     * caller originally bound, with no change needed in `RenderRectBatch()`/`RenderImageBatch()`/
     * `RenderShadowBatch()` themselves, since they already just draw into whatever framebuffer is
     * currently bound.
     */
    void PushLayer(LayerOp op, float opacity, Rendering::BlendMode mode);

    /**
     * @brief    Ends the innermost open layer, compositing it back into its parent (Phase 34.5).
     *
     * Pops `_layerStack`, rebinds the popped frame's saved parent framebuffer/viewport, then
     * composites via `CompositeOpacityLayer()` or `CompositeBlendLayer()` depending on which kind
     * of layer this was. `IMF_ASSERT`s the stack isn't already empty — a `PopLayer` with no
     * matching `Push{Opacity,Blend}Layer` is a malformed `Rendering::CommandBuffer`, a programming
     * error in the caller, not a runtime condition this internal renderer recovers from.
     */
    void PopLayer();

    void EnsureLayerTarget(std::size_t depth, int width, int height);

    /// One entry in `_layerStack` — everything `PopLayer()` needs to composite a layer back and
    /// restore the state that was active before its matching `Push{Opacity,Blend}Layer`.
    struct LayerFrame {
        LayerOp              Op = LayerOp::PushOpacity;      ///< Never `Pop` — see `PushLayer()`.
        float                Opacity = 1.0f;                 ///< Meaningful only when `Op == PushOpacity`.
        Rendering::BlendMode Mode = Rendering::BlendMode::Normal; ///< Meaningful only when `Op == PushBlend`.
        unsigned int         ParentFbo = 0;
        int                  ParentViewport[4] = {0, 0, 0, 0};
        std::size_t          TargetIndex = 0; ///< Index into `_layerTargets` this frame rendered into.
        int                  Width  = 0;
        int                  Height = 0;
    };

    /// Composites an opacity layer back into its parent: a plain textured quad, correct
    /// premultiplied-alpha-over blending (`GL_ONE`, `GL_ONE_MINUS_SRC_ALPHA`) — see this method's
    /// `.cpp` comment for why that differs from `DrawBatches()`'s own default blend func.
    void CompositeOpacityLayer(const LayerFrame& frame);

    /**
     * @brief    Composites a blend-mode layer back into its parent (Phase 34.5).
     *
     * Real per-pixel blend modes (`Multiply`, `Screen`, ... ) need the *destination*'s own current
     * color at each pixel, which plain `glBlendFunc` fixed-function blending cannot read — the
     * blend equation only ever sees fixed src/dst *factors*, never the destination's actual color
     * value, so a nonlinear function of both (e.g. `Cb * Cs`) can't be expressed that way. This
     * copies the parent target's current content into `_backdropTex` (via `glCopyTexSubImage2D` —
     * the same technique Phase 34.6's backdrop blur will use), then draws a fullscreen quad with
     * `_blendProgram`, a shader that samples *both* the layer's own texture and `_backdropTex`,
     * computes the selected `Rendering::BlendMode`'s formula in GLSL, and writes the fully
     * Porter-Duff-composited result directly with `GL_BLEND` disabled — the shader's own output
     * already incorporates the destination, so no fixed-function blending runs on top of it.
     */
    void CompositeBlendLayer(const LayerFrame& frame);

    void EnsureBlendProgram();
    void EnsureBackdropTarget(int width, int height);

    /// One pooled offscreen render target, indexed by nesting depth in `_layerTargets` — reused
    /// across `Push`/`Pop` pairs and across frames, resized only when the requested size changes.
    struct LayerTarget {
        unsigned int Fbo = 0;
        unsigned int Tex = 0;
        int          Width  = 0;
        int          Height = 0;
    };

    BatchBuilder _batchBuilder;

    /// See `AttachTextRenderer()`/`ITextRenderer`. Not owned.
    ITextRenderer* _textRenderer = nullptr;

    /// `RenderShadowBatch()`'s Gaussian blur (Phase 34.4) — see `BlurPassGL3`'s own doc comment.
    BlurPassGL3 _blurPass;

    /// Offscreen target `RenderShadowBatch()` renders a shadow's rounded-rect silhouette into,
    /// before handing it to `_blurPass`. Resized on demand, same as `BlurPassGL3`'s own targets.
    unsigned int _shadowSilhouetteFbo    = 0;
    unsigned int _shadowSilhouetteTex    = 0;
    int          _shadowSilhouetteWidth  = 0;
    int          _shadowSilhouetteHeight = 0;

    /// Currently-open layers, outermost first — see `PushLayer()`/`PopLayer()`.
    std::vector<LayerFrame>  _layerStack;
    /// Pooled offscreen targets, indexed by nesting depth (`_layerStack.size()` at push time).
    std::vector<LayerTarget> _layerTargets;

    /// `CompositeBlendLayer()`'s two-texture blend-mode shader (Phase 34.5).
    unsigned int _blendProgram       = 0;
    int          _blendSourceLoc     = -1;
    int          _blendBackdropLoc  = -1;
    int          _blendModeLoc       = -1;
    unsigned int _blendVao = 0;
    unsigned int _blendVbo = 0;

    /// The parent target's copied-back content for `CompositeBlendLayer()`. Resized on demand.
    unsigned int _backdropTex    = 0;
    int          _backdropWidth  = 0;
    int          _backdropHeight = 0;

    /// `RenderBackdropBlurBatch()`'s own copy-then-blur target (Phase 34.6) — separate from
    /// `_backdropTex` (Phase 34.5's blend-layer backdrop copy): different purpose, different
    /// expected size, and there's no guarantee the two operations can never nest in the future.
    unsigned int _backdropBlurCopyTex    = 0;
    int          _backdropBlurCopyWidth  = 0;
    int          _backdropBlurCopyHeight = 0;

    /// See `SetMaxBackdropBlurPerFrame()`. `_backdropBlurCountThisFrame` resets to 0 at the start
    /// of every `Render()` call — "per frame" means "per `Render()` call" for this renderer.
    int _maxBackdropBlurPerFrame    = 4;
    int _backdropBlurCountThisFrame = 0;
};

} // namespace ImFrame::Internal
