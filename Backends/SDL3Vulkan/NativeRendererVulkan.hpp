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
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-12
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

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
     * @param[in] targetView  A `VkImageView` of format `colorFormat` (the constructor's own
     *                        parameter), usage including `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`,
     *                        already in `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` layout at the
     *                        time `Render()` is called (this class does not transition it — see
     *                        this file's own comment on why there is no implicit "currently bound"
     *                        target the way `NativeRendererGL3` has via `GL_VIEWPORT`).
     * @param[in] width       Target width, in pixels.
     * @param[in] height      Target height, in pixels.
     *
     * Must be called at least once before the first `Render()` call.
     */
    void SetTarget(VkImageView targetView, std::uint32_t width, std::uint32_t height) noexcept;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Always fails — no text pipeline exists yet (Phase 35.1's scope is `BatchKind::Rect` only).
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all Vulkan resources this renderer itself created. Safe to call multiple times,
    /// including before any `Render()` call. Does not touch the constructor's borrowed handles.
    void Shutdown() override;

private:
    void EnsureInitialized();
    void EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes);
    void RenderRectBatch(VkCommandBuffer cmd, const Batch& batch, std::size_t& vertexByteOffset,
                        std::size_t& indexByteOffset);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    VkDevice              _device              = VK_NULL_HANDLE;
    VmaAllocator          _allocator           = VK_NULL_HANDLE;
    VkQueue               _graphicsQueue       = VK_NULL_HANDLE;
    std::uint32_t         _graphicsQueueFamily = 0;
    VkCommandPool         _commandPool         = VK_NULL_HANDLE;
    VkFormat              _colorFormat         = VK_FORMAT_UNDEFINED;

    // ─── Render target (set via SetTarget()) ───────────────────────────────────
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

    /// Small private pool for this renderer's own `PerFrame` UBO descriptor set — not the
    /// backend's shared descriptor pool (`ViewportFramebufferVulkan`'s own `descriptorPool`
    /// constructor parameter is sized/typed for ImGui's combined-image-sampler descriptors, not a
    /// uniform buffer; owning a dedicated pool here avoids a type/capacity mismatch entirely).
    VkDescriptorPool _descriptorPool       = VK_NULL_HANDLE;
    VkDescriptorSet  _perFrameDescriptorSet = VK_NULL_HANDLE;

    /// `SDFRect.glsl`'s `layout(binding=0) uniform PerFrame { vec2 ViewportSize; }`. Host-visible,
    /// persistently mapped (VMA `HOST_ACCESS_SEQUENTIAL_WRITE`) — updated once per `Render()` call.
    VkBuffer      _perFrameUbo           = VK_NULL_HANDLE;
    VmaAllocation _perFrameUboAllocation = VK_NULL_HANDLE;
    void*         _perFrameUboMapped     = nullptr;

    /// Streaming vertex/index buffers, re-uploaded per `Render()` call — host-visible, persistently
    /// mapped, grown (never shrunk) on demand, matching `NativeRendererGL3`'s own `GL_STREAM_DRAW`
    /// re-upload-every-batch convention.
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

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
