/**
 * @file     NativeRendererVulkan.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real Vulkan 1.3 draw calls
 *
 * @internal
 * The first Phase 35 backend (Phase 35.1) — mirrors `NativeRendererGL3`'s own
 * starting scope (Phase 32.4): `BatchKind::Rect` only, via a real graphics
 * pipeline, no batching-kind interleaving, no text, no effects yet. Those
 * follow in later 35.x sub-phases, exactly the same incremental order GL3
 * itself was built in across Phases 32-34.
 *
 * Phase 35.2 adds `BatchKind::Image`. `Rendering::TextureId::Value()` is
 * reinterpreted as a raw `VkImageView` handle, sampled through one
 * renderer-owned shared `VkSampler` — the Vulkan analogue of
 * `NativeRendererGL3`'s own "raw handle, reinterpreted directly, no registry
 * indirection" convention for its own (GL) texture type. Unlike a GL texture
 * name, a `VkImageView` alone isn't enough to bind+sample in a single step —
 * Vulkan's descriptor-set model needs an explicit combined-image-sampler
 * descriptor naming both the view and a sampler, updated to point at
 * whichever texture the current batch uses. Because a descriptor set's
 * *content* is resolved by the GPU at command-buffer *execution* time, not
 * at `vkCmdBindDescriptorSets` *recording* time, reusing a single Image
 * descriptor set object across multiple different-textured `Image` batches
 * recorded into the same not-yet-submitted command buffer would make every
 * one of those draws sample whichever texture was written *last* — see
 * `RenderImageBatch()`'s own comment. `Render()` therefore allocates one
 * fresh descriptor set per `Image` batch from a pool it resets every call
 * (`EnsureImageDescriptorCapacity()`), the same "grow, never shrink,
 * recreate-on-demand" convention `EnsureVertexIndexCapacity()` already uses
 * for the streaming vertex/index buffers.
 *
 * Unlike `NativeRendererGL3` (which relies on OpenGL's implicit "currently
 * bound context/framebuffer" — a real `GLFWOpenGL3Backend` window makes its
 * GL context current on the calling thread, and `glGetIntegerv(GL_VIEWPORT,
 * ...)` reads whatever's already bound), Vulkan has no implicit "current"
 * anything — every resource (device, queue, command pool, target image view)
 * must be explicit. This class is therefore constructed with explicit
 * Vulkan handles (not routed through the public `Rendering::VulkanContext` —
 * that struct exists for Phase 24 Viewport's *application-facing* use, a
 * different, external consumer with different needs; this class is as
 * backend-internal as `NativeRendererGL3` itself, just for a graphics API
 * with no implicit context to lean on) and needs an explicit `SetTarget()`
 * call before its first `Render()`.
 *
 * `Render()` currently records, submits, and `vkWaitForFences()`s its own
 * one-shot command buffer synchronously on every call — a deliberate,
 * documented stopgap matching `ViewportFramebufferVulkan::EndRender()`'s own
 * already-accepted `vkQueueWaitIdle()` CPU stall (see that file's own
 * comment: "Phase 24.x: replace... enabling true GPU-pipeline parallelism").
 * Integrating this renderer into a *live* per-frame command buffer (so it
 * records into the same command buffer the application's own frame is
 * already building, with no extra submission/wait) is deferred to whichever
 * later 35.x sub-phase actually wires `NativeRendererVulkan` into
 * `SDL3VulkanBackend`'s real frame loop — this sub-phase only proves the
 * pipeline/shader/buffer mechanics work correctly in isolation, matching how
 * `NativeRendererGL3_test.cpp`'s own first tests needed no `Application`/
 * `Viewport` machinery either.
 *
 * Consumes `Shaders/SDFRect.glsl`'s compiled SPIR-V directly (Phase 32.2's
 * `cmake/CompileShader.cmake` pipeline, embedded as
 * `ImFrame::Internal::Shaders::kSDFRectVertexSpirv`/`kSDFRectFragmentSpirv`)
 * — the first real consumer of that pipeline's SPIR-V output; `NativeRendererGL3`
 * only ever used the same generated header's raw-GLSL-source half.
 *
 * Phase 35.19 fixes a race shared by every renderer-internal composite draw (shadow,
 * opacity/blend layer, backdrop blur). Each wrote one shared quad buffer and rewrote one shared
 * descriptor set in place, but a layer pop records its composite into the same still-recording
 * command buffer as any earlier composite (only Shadow/BackdropBlur submit first). The quad data is
 * read at execution time, so earlier draws read the last write, and updating a bound descriptor set
 * invalidated the whole command buffer (a validation error). Every internal quad draw now gets its
 * own `_internalQuadVertexBuffer` slot and every composite its own freshly allocated descriptor set,
 * each written exactly once per `Render()` call.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-12
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "BlurPassVulkan.hpp"
#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>

namespace ImFrame::Internal {

/**
 * @class    NativeRendererVulkan
 * @brief    Renders `Rendering::DrawRect` batches via a real Vulkan 1.3 graphics pipeline
 *
 * @internal
 * GL resources — sorry, Vulkan resources (shader modules, descriptor set layout, pipeline layout,
 * pipeline, descriptor pool, streaming vertex/index buffers) are created lazily on the first
 * `Render()` call, matching `NativeRendererGL3::EnsureInitialized()`'s own lazy-initialization
 * convention — constructing an instance never issues a single Vulkan call.
 *
 * @since    3.0.1
 */
class NativeRendererVulkan final : public IRenderer {
public:
    /**
     * @brief    Constructs a renderer bound to the given (not owned) Vulkan device/queue/pool.
     * @param[in] device               A valid, already-created `VkDevice`.
     * @param[in] allocator            A `VmaAllocator` created against the same device.
     * @param[in] graphicsQueue        A queue from `graphicsQueueFamily` supporting graphics ops.
     * @param[in] graphicsQueueFamily  The queue family index `graphicsQueue` belongs to.
     * @param[in] commandPool          Pool `Render()` allocates its one-shot command buffer from.
     *                                 Must support `vkResetCommandBuffer` (i.e. created with
     *                                 `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`).
     * @param[in] colorFormat          `VkFormat` of the target image view(s) passed to `SetTarget()`
     *                                 — fixed at construction time, matching this sub-phase's scope
     *                                 (one target format for this renderer's whole lifetime).
     *
     * None of these handles are owned — the caller (a real backend, or a test) is responsible for
     * their lifetime outliving this object and for calling `Shutdown()` before destroying them.
     */
    NativeRendererVulkan(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue,
                         std::uint32_t graphicsQueueFamily, VkCommandPool commandPool, VkFormat colorFormat);
    ~NativeRendererVulkan() override;

    NativeRendererVulkan(const NativeRendererVulkan&) = delete;
    NativeRendererVulkan& operator=(const NativeRendererVulkan&) = delete;

    /**
     * @brief    Sets the offscreen target `Render()` draws into.
     * @param[in] targetImage  The `VkImage` `targetView` is a view of — Phase 35.5's
     *                         `CompositeBlendLayer()` needs the real image (to `vkCmdCopyImage`
     *                         its current content into `_backdropImage`), which no Vulkan call can
     *                         recover from a `VkImageView` alone (the same reason `BlurPassVulkan`'s
     *                         `Apply()` returns a `BlurResult{Image; View}` pair, not a bare view —
     *                         see that class's own `.hpp` comment).
     * @param[in] targetView  A `VkImageView` of `targetImage`, format `colorFormat` (the
     *                        constructor's own parameter), usage including
     *                        `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`, already in
     *                        `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` layout at the time
     *                        `Render()` is called (this class does not transition it — see this
     *                        file's own comment on why there is no implicit "currently bound"
     *                        target the way `NativeRendererGL3` has via `GL_VIEWPORT`) — except
     *                        for the brief, internal `COLOR_ATTACHMENT_OPTIMAL <-> TRANSFER_SRC_OPTIMAL`
     *                        round-trip `CompositeBlendLayer()` performs around its own backdrop
     *                        copy, always restoring it before `Render()` returns.
     * @param[in] width       Target width, in pixels.
     * @param[in] height      Target height, in pixels.
     *
     * Must be called at least once before the first `Render()` call.
     */
    void SetTarget(VkImage targetImage, VkImageView targetView, std::uint32_t width, std::uint32_t height) noexcept;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Caps how many `Rendering::DrawBackdropBlur` commands actually run their (real GPU cost)
    /// copy+blur+composite per `Render()` call — matches
    /// `NativeRendererGL3::SetMaxBackdropBlurPerFrame()`'s identical rate-limiting role and default.
    void SetMaxBackdropBlurPerFrame(int maxPerFrame) noexcept { _maxBackdropBlurPerFrame = maxPerFrame; }

    /// Always fails — no text pipeline exists yet (Phase 35.1's scope is `BatchKind::Rect` only).
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all Vulkan resources this renderer itself created. Safe to call multiple times,
    /// including before any `Render()` call. Does not touch the constructor's borrowed handles.
    void Shutdown() override;

private:
    /// One entry on the layer stack (Phase 35.5) — mirrors `NativeRendererGL3::LayerFrame` field
    /// for field, `ParentFbo`/`ParentViewport` replaced by `ParentView` (this renderer has no
    /// separate "current viewport" state to restore — every layer, and the real target, is always
    /// `_targetWidth` x `_targetHeight`).
    struct LayerFrame {
        LayerOp               Op      = LayerOp::PushOpacity;
        float                  Opacity = 1.0f;
        Rendering::BlendMode   Mode    = Rendering::BlendMode::Normal;
        VkImage                ParentImage  = VK_NULL_HANDLE; ///< Needed only by `CompositeBlendLayer()`'s backdrop copy.
        VkImageView            ParentView   = VK_NULL_HANDLE; ///< What to resume rendering into on `PopLayer()`.
        VkImageLayout          ParentLayout = VK_IMAGE_LAYOUT_GENERAL; ///< `_targetImage`'s own layout at depth 0
                                                                       ///< (`COLOR_ATTACHMENT_OPTIMAL`); `GENERAL`
                                                                       ///< for any nested layer parent.
        std::size_t            TargetIndex = 0;              ///< Index into `_layerTargets`.
    };

    /// One depth level's offscreen layer target — mirrors `NativeRendererGL3::LayerTarget`.
    /// `COLOR_ATTACHMENT_BIT | SAMPLED_BIT` usage, kept in `VK_IMAGE_LAYOUT_GENERAL` permanently,
    /// the same "valid for both roles, so never transitioned" convention `_shadowSilhouetteImage`
    /// already uses (Phase 35.4).
    struct LayerTarget {
        VkImage       Image      = VK_NULL_HANDLE;
        VmaAllocation Allocation = VK_NULL_HANDLE;
        VkImageView   View       = VK_NULL_HANDLE;
        std::uint32_t Width      = 0;
        std::uint32_t Height     = 0;
    };

    void EnsureInitialized();
    void EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes);
    void EnsureImageDescriptorCapacity(std::size_t neededSets);

    /// Grows `_internalQuadVertexBuffer` to hold `slots` quads (never shrinks) and resets its slot
    /// cursor — called once per `Render()` call, before recording, while nothing is in flight
    /// (Phase 35.19).
    void EnsureInternalQuadCapacity(std::size_t slots);
    /// Copies one quad's vertices into the next unused `_internalQuadVertexBuffer` slot this
    /// `Render()` call and returns that slot's byte offset, for `vkCmdBindVertexBuffers()`.
    VkDeviceSize WriteInternalQuad(const void* vertices, std::size_t bytes);

    /// Resets both composite descriptor pools (growing them if needed) and allocates exactly
    /// `compositeSets` image-layout sets (binding 0 pre-written with `_perFrameUbo`) and
    /// `blendSets` blend-layout sets for this `Render()` call — called before recording, while
    /// nothing is in flight (Phase 35.19).
    void PrepareCompositeDescriptorSets(std::size_t compositeSets, std::size_t blendSets);
    VkDescriptorSet NextCompositeDescriptorSet();
    VkDescriptorSet NextBlendDescriptorSet();
    /// Writes binding 1 (the `VK_IMAGE_LAYOUT_GENERAL` texture) of a freshly taken composite set.
    void WriteCompositeTexture(VkDescriptorSet set, VkImageView view);
    void RenderRectBatch(VkCommandBuffer cmd, const Batch& batch, std::size_t& vertexByteOffset,
                        std::size_t& indexByteOffset);

    /**
     * @brief    Records one `BatchKind::Image` batch's draw call.
     * @param[in] imageDescriptorSet  A descriptor set from `_imageDescriptorSets`, already written
     *                                (by `Render()`, before this call) with `batch.Texture`'s view
     *                                at binding 1 and `_perFrameUbo` at binding 0 — see this
     *                                class's own file comment on why the write can't happen inside
     *                                this method, reused across batches, the way `RenderRectBatch()`
     *                                reuses `_rectDescriptorSet` (which has no per-batch texture).
     */
    void RenderImageBatch(VkCommandBuffer cmd, const Batch& batch, VkDescriptorSet imageDescriptorSet,
                        std::size_t& vertexByteOffset, std::size_t& indexByteOffset);

    /**
     * @brief    Renders one `BatchKind::Shadow` batch: silhouette, blur, composite (Phase 35.4).
     * @param[in,out] cmd  The main frame command buffer `Render()`'s loop is otherwise recording
     *                     into. Replaced with a *new* command buffer on return — see this class's
     *                     own file comment on why a shadow can't be recorded into the same command
     *                     buffer as everything around it the way `Rect`/`Image` batches are.
     *
     * `BlurPassVulkan::Apply()` is a fully self-contained, synchronously-awaited submission on its
     * own — it cannot be recorded as commands into `Render()`'s own not-yet-submitted command
     * buffer the way `RenderRectBatch()`/`RenderImageBatch()` are. So this method: (1) ends and
     * submits `cmd` as accumulated so far, (2) renders the shape's silhouette into
     * `_shadowSilhouetteView` via its own one-shot command buffer (reusing `_rectPipeline`), (3)
     * calls `_blurPass.Apply()`, (4) records the blurred, tinted composite draw and returns it as
     * a brand new, freshly-begun `cmd` for `Render()`'s loop to keep recording into.
     */
    void RenderShadowBatch(VkCommandBuffer& cmd, const Batch& batch);

    void EnsureShadowSilhouetteTarget(std::uint32_t width, std::uint32_t height);

    /**
     * @brief    Dispatches one `BatchKind::Layer` marker (Phase 35.5) to `PushLayer()`/`PopLayer()`.
     * @param[in,out] cmd  See `RenderShadowBatch()` — unlike that method, `cmd` is never replaced
     *                     here (no separate submission is needed for layer push/pop, only ending
     *                     and re-beginning rendering instances within the same command buffer),
     *                     but is still taken by reference for signature symmetry with the loop
     *                     that calls both.
     */
    void HandleLayerMarker(VkCommandBuffer cmd, const Batch& batch);
    void PushLayer(VkCommandBuffer cmd, LayerOp op, float opacity, Rendering::BlendMode mode);
    void PopLayer(VkCommandBuffer cmd);
    void CompositeOpacityLayer(VkCommandBuffer cmd, const LayerFrame& frame);
    /// Copies `frame.ParentImage`'s current content into `_backdropImage` — must run with no
    /// rendering instance active (`vkCmdCopyImage` is a transfer command). `PopLayer()` calls this
    /// *before* beginning the parent's own rendering instance; `CompositeBlendLayer()` below (the
    /// actual draw) runs *after*, once that instance is active.
    void CopyBackdropForBlend(VkCommandBuffer cmd, const LayerFrame& frame);
    void CompositeBlendLayer(VkCommandBuffer cmd, const LayerFrame& frame);
    void EnsureLayerTarget(std::size_t depth, std::uint32_t width, std::uint32_t height);
    void EnsureBackdropTarget(std::uint32_t width, std::uint32_t height);

    /**
     * @brief    Renders one `BatchKind::BackdropBlur` batch (Phase 35.6): copy, blur, cropped composite.
     * @param[in,out] cmd  See `RenderShadowBatch()` — replaced with a *new* command buffer on
     *                     return, for the same reason: `_blurPass.Apply()` is its own separate,
     *                     synchronously-awaited submission, and the backdrop copy itself needs the
     *                     target's content from every batch recorded (not yet submitted) so far to
     *                     actually be on the GPU already, which means flushing first, exactly like
     *                     `RenderShadowBatch()`'s own first step.
     */
    void RenderBackdropBlurBatch(VkCommandBuffer& cmd, const Batch& batch);
    void EnsureBackdropBlurCopyTarget(std::uint32_t width, std::uint32_t height);

    /// Begins (allocates, `vkBeginCommandBuffer`s, `vkCmdBeginRendering`s with `LOAD_OP_LOAD`
    /// against `_targetView`, sets viewport/scissor) a fresh one-shot command buffer targeting
    /// the real render target — the shape every `Render()` call's *initial* command buffer
    /// already had, factored out so `RenderShadowBatch()` can resume it identically after its own
    /// side submissions.
    [[nodiscard]] VkCommandBuffer BeginMainCommandBuffer();

    /// Ends the current rendering instance and command buffer, submits it, and synchronously
    /// waits (`_submitFence`) — the second half of `Render()`'s own "deliberate synchronous
    /// stall" convention (see this class's own file comment), factored out so `RenderShadowBatch()`
    /// can flush the accumulated command buffer before its own side submissions.
    void EndAndSubmitMainCommandBuffer(VkCommandBuffer cmd);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    VkDevice              _device              = VK_NULL_HANDLE;
    VmaAllocator          _allocator           = VK_NULL_HANDLE;
    VkQueue               _graphicsQueue       = VK_NULL_HANDLE;
    std::uint32_t         _graphicsQueueFamily = 0;
    VkCommandPool         _commandPool         = VK_NULL_HANDLE;
    VkFormat              _colorFormat         = VK_FORMAT_UNDEFINED;

    // ─── Render target (set via SetTarget()) ───────────────────────────────────
    VkImage       _targetImage  = VK_NULL_HANDLE;
    VkImageView   _targetView   = VK_NULL_HANDLE;
    std::uint32_t _targetWidth  = 0;
    std::uint32_t _targetHeight = 0;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    VkShaderModule        _rectVertexModule       = VK_NULL_HANDLE;
    VkShaderModule        _rectFragmentModule     = VK_NULL_HANDLE;
    VkDescriptorSetLayout _rectDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      _rectPipelineLayout     = VK_NULL_HANDLE;
    VkPipeline            _rectPipeline           = VK_NULL_HANDLE;

    /// Small private pool for this renderer's own *stable* (created-once, never-reallocated)
    /// descriptor sets — `_rectDescriptorSet` only; Image's own per-batch sets live in
    /// `_imageDescriptorPool` instead (see this class's own file comment on why they can't be
    /// stable). Not the backend's shared descriptor pool (`ViewportFramebufferVulkan`'s own
    /// `descriptorPool` constructor parameter is sized/typed for ImGui's combined-image-sampler
    /// descriptors, not a uniform buffer; owning a dedicated pool here avoids a type/capacity
    /// mismatch entirely).
    VkDescriptorPool _descriptorPool   = VK_NULL_HANDLE;
    VkDescriptorSet  _rectDescriptorSet = VK_NULL_HANDLE;

    /// `SDFRect.glsl`/`Image.glsl`'s shared `layout(binding=0) uniform PerFrame { vec2 ViewportSize; }`
    /// — one buffer, referenced by both `_rectDescriptorSet`'s and every per-batch Image descriptor
    /// set's own binding 0 (a descriptor is just a reference to this buffer; nothing stops two
    /// different descriptor sets, even across different set layouts, from pointing at the same
    /// one). Host-visible, persistently mapped (VMA `HOST_ACCESS_SEQUENTIAL_WRITE`) — updated once
    /// per `Render()` call.
    VkBuffer      _perFrameUbo           = VK_NULL_HANDLE;
    VmaAllocation _perFrameUboAllocation = VK_NULL_HANDLE;
    void*         _perFrameUboMapped     = nullptr;

    // ─── BatchKind::Image (Phase 35.2) ──────────────────────────────────────────
    VkShaderModule        _imageVertexModule        = VK_NULL_HANDLE;
    VkShaderModule        _imageFragmentModule      = VK_NULL_HANDLE;
    VkDescriptorSetLayout _imageDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      _imagePipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            _imagePipeline            = VK_NULL_HANDLE;

    /// Same shader modules/vertex input/pipeline layout as `_imagePipeline` — only the blend state
    /// differs (`ONE`/`ONE_MINUS_SRC_ALPHA`, correct for compositing an already-premultiplied
    /// renderer-owned source, vs. `_imagePipeline`'s own `SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA` for a
    /// real, non-premultiplied `DrawImage` source). Used by `CompositeOpacityLayer()` (Phase 35.5)
    /// — see that method's own comment, mirroring `NativeRendererGL3::CompositeOpacityLayer()`'s
    /// identical blend-factor override.
    VkPipeline _premultipliedImagePipeline = VK_NULL_HANDLE;

    /// Shared by every `Image` batch — the Vulkan equivalent of the fixed `GL_LINEAR`/
    /// `GL_CLAMP_TO_EDGE` texture parameters `NativeRendererGL3`'s own textures are created with;
    /// there is no per-`DrawImage`-command sampling-parameter field to honour differently.
    VkSampler _linearSampler = VK_NULL_HANDLE;

    /// Reset (`vkResetDescriptorPool`) at the start of every `Render()` call, then one fresh
    /// descriptor set is allocated per `Image` batch that frame — see this class's own file
    /// comment on why these can't be stable, reused sets the way `_rectDescriptorSet` is. Grown
    /// (never shrunk), the same "recreate on demand" convention `_vertexBuffer`/`_indexBuffer`
    /// already use.
    VkDescriptorPool             _imageDescriptorPool         = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> _imageDescriptorSets; ///< Reused capacity across `Render()` calls.
    std::size_t                 _imageDescriptorPoolCapacitySets = 0;

    /// Streaming vertex/index buffers, re-uploaded per `Render()` call — host-visible, persistently
    /// mapped, grown (never shrunk) on demand, matching `NativeRendererGL3`'s own `GL_STREAM_DRAW`
    /// re-upload-every-batch convention. Shared by `Rect` and `Image` batches alike — plain bytes at
    /// a growing offset, kind-agnostic.
    VkBuffer      _vertexBuffer               = VK_NULL_HANDLE;
    VmaAllocation _vertexBufferAllocation     = VK_NULL_HANDLE;
    void*         _vertexBufferMapped         = nullptr;
    std::size_t   _vertexBufferCapacityBytes  = 0;

    VkBuffer      _indexBuffer                = VK_NULL_HANDLE;
    VmaAllocation _indexBufferAllocation      = VK_NULL_HANDLE;
    void*         _indexBufferMapped          = nullptr;
    std::size_t   _indexBufferCapacityBytes   = 0;

    /// Reused every `Render()` call — signalled once the one-shot command buffer completes, so
    /// `Render()` can safely reuse/overwrite the streaming buffers on its *next* call.
    VkFence _submitFence = VK_NULL_HANDLE;

    // ─── BatchKind::Shadow (Phase 35.4) ──────────────────────────────────────────
    BlurPassVulkan _blurPass;

    /// Offscreen target `RenderShadowBatch()` renders a shadow's rounded-rect silhouette into,
    /// before handing it to `_blurPass`. Usage includes both `COLOR_ATTACHMENT_BIT` (the
    /// silhouette render) and `STORAGE_BIT` (`_blurPass`'s own `imageLoad`), kept in
    /// `VK_IMAGE_LAYOUT_GENERAL` permanently — `GENERAL` is valid for both a colour attachment and
    /// a storage image, so no layout transition is needed between the two uses, matching
    /// `BlurPassVulkan`'s own "stay in `GENERAL` forever" convention for its ping-pong targets.
    /// Resized on demand, same as `BlurPassVulkan`'s own targets.
    VkImage       _shadowSilhouetteImage           = VK_NULL_HANDLE;
    VmaAllocation _shadowSilhouetteImageAllocation = VK_NULL_HANDLE;
    VkImageView   _shadowSilhouetteView            = VK_NULL_HANDLE;
    std::uint32_t _shadowSilhouetteWidth           = 0;
    std::uint32_t _shadowSilhouetteHeight          = 0;

    /// Dedicated buffers for every renderer-internal one-quad draw (shadow silhouette and composite,
    /// opacity-layer composite, backdrop-blur composite) — kept separate from `_vertexBuffer`/
    /// `_indexBuffer`. **One slot per quad draw per `Render()` call** (Phase 35.19), each sized for
    /// the larger of `RectVertex`/`ImageVertex` x 4, grown (never shrunk) by
    /// `EnsureInternalQuadCapacity()` and handed out in order by `WriteInternalQuad()`. The
    /// original single reused quad (Phase 35.4) was only safe when a submission separated every
    /// write from the draw that last read it — true for Shadow, not for a layer pop, which records
    /// into the same still-open command buffer as any earlier composite; this host-visible memory
    /// is read at *execution* time, so every such draw read the last write. The index buffer holds
    /// one constant quad, written once, shared by every slot.
    VkBuffer      _internalQuadVertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation _internalQuadVertexAllocation = VK_NULL_HANDLE;
    void*         _internalQuadVertexMapped     = nullptr;
    std::size_t   _internalQuadCapacitySlots    = 0;
    std::size_t   _nextInternalQuadSlot         = 0;
    VkBuffer      _internalQuadIndexBuffer      = VK_NULL_HANDLE;
    VmaAllocation _internalQuadIndexAllocation  = VK_NULL_HANDLE;
    void*         _internalQuadIndexMapped      = nullptr;

    /// `_imagePipeline`-compatible descriptor sets (reusing `_imageDescriptorSetLayout`) for
    /// compositing a *renderer-owned* `VK_IMAGE_LAYOUT_GENERAL` texture (`_blurPass`'s blurred
    /// output, or a popped opacity layer's target) — unlike every real `DrawImage` texture (always
    /// `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`), these never leave `GENERAL`. **One set per
    /// composite draw per `Render()` call** (Phase 35.19), allocated fresh by
    /// `PrepareCompositeDescriptorSets()` and written exactly once by whichever composite takes it
    /// via `NextCompositeDescriptorSet()`. The original single set, rewritten before each
    /// composite, was invalid: updating a descriptor set that a recording command buffer has bound
    /// (without `UPDATE_AFTER_BIND`) invalidates that command buffer outright.
    VkDescriptorPool             _compositeDescriptorPool         = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> _compositeDescriptorSets;
    std::size_t                  _compositeDescriptorCapacitySets = 0;
    std::size_t                  _nextCompositeDescriptorSet      = 0;

    // ─── BatchKind::Layer (Phase 35.5) ──────────────────────────────────────────
    std::vector<LayerFrame>  _layerStack;
    std::vector<LayerTarget> _layerTargets; ///< Indexed by stack depth, grown on demand, never shrunk.

    /// `Blend.glsl`'s pipeline — real per-pixel Porter-Duff blend-mode compositing for
    /// `PushBlendLayer`/`PopLayer`, mirroring `NativeRendererGL3`'s own blend shader/program.
    /// Needs no vertex input state at all (the fullscreen quad is generated from `gl_VertexIndex`
    /// in `Blend.glsl` itself) and no `PerFrame` UBO (its vertex shader outputs fixed NDC
    /// coordinates directly, not a pixel-space-to-NDC conversion).
    VkShaderModule        _blendVertexModule        = VK_NULL_HANDLE;
    VkShaderModule        _blendFragmentModule      = VK_NULL_HANDLE;
    VkDescriptorSetLayout _blendDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      _blendPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            _blendPipeline            = VK_NULL_HANDLE;

    /// `Blend.glsl` descriptor sets (two combined-image-sampler bindings: the popped layer's own
    /// texture, and `_backdropImage`) — one per `CompositeBlendLayer()` call per `Render()` call,
    /// for the same reason as `_compositeDescriptorSets` (Phase 35.19).
    VkDescriptorPool             _blendDescriptorPool         = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> _blendDescriptorSets;
    std::size_t                  _blendDescriptorCapacitySets = 0;
    std::size_t                  _nextBlendDescriptorSet      = 0;

    /// A copy of the parent target's current content at `PopLayer()` time (a fragment shader has
    /// no other way to read a colour attachment's own existing pixel value) — `COLOR_ATTACHMENT_BIT`
    /// would be unused here (never rendered into directly, only `vkCmdCopyImage`'d into), so usage
    /// is `TRANSFER_DST_BIT | SAMPLED_BIT` only, kept in `VK_IMAGE_LAYOUT_GENERAL` permanently
    /// (valid for both a copy destination and sampling, the same convention as this class's other
    /// renderer-owned images).
    VkImage       _backdropImage           = VK_NULL_HANDLE;
    VmaAllocation _backdropImageAllocation = VK_NULL_HANDLE;
    VkImageView   _backdropView            = VK_NULL_HANDLE;
    std::uint32_t _backdropWidth           = 0;
    std::uint32_t _backdropHeight          = 0;

    // ─── BatchKind::BackdropBlur (Phase 35.6) ───────────────────────────────────
    /// A copy of the padded region `RenderBackdropBlurBatch()` is about to blur — kept separate
    /// from `_backdropImage` (different role/lifecycle: sized to the padded *requested rect*, not
    /// the whole target, and read by `_blurPass` via `imageLoad`/`imageStore` rather than sampled),
    /// matching `NativeRendererGL3`'s own identical choice to keep `_backdropBlurCopyTex` distinct
    /// from `_backdropTex` despite the conceptual overlap. `STORAGE_BIT` (this is `_blurPass`'s own
    /// `Apply()` source) `| TRANSFER_DST_BIT` (the copy destination), kept in
    /// `VK_IMAGE_LAYOUT_GENERAL` permanently, the same "valid for both roles" convention every
    /// other renderer-owned image in this class already uses.
    VkImage       _backdropBlurCopyImage           = VK_NULL_HANDLE;
    VmaAllocation _backdropBlurCopyImageAllocation = VK_NULL_HANDLE;
    VkImageView   _backdropBlurCopyView            = VK_NULL_HANDLE;
    std::uint32_t _backdropBlurCopyWidth           = 0;
    std::uint32_t _backdropBlurCopyHeight          = 0;

    /// See `SetMaxBackdropBlurPerFrame()`. `_backdropBlurCountThisFrame` resets to 0 at the start
    /// of every `Render()` call — "per frame" means "per `Render()` call", matching
    /// `NativeRendererGL3`'s own identical rate-limit and reset point.
    int _maxBackdropBlurPerFrame    = 4;
    int _backdropBlurCountThisFrame = 0;

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
