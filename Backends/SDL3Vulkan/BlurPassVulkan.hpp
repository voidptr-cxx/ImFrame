/**
 * @file     BlurPassVulkan.hpp
 * @brief    Separable Gaussian blur for `NativeRendererVulkan`, implemented as two compute-shader passes
 *
 * @internal
 * The Vulkan counterpart to `BlurPassGL3` (Phase 34.3) — mirrors its public shape (`Apply()`/
 * `Shutdown()`, lazy initialization, reallocate-ping-pong-targets-only-on-size-change) but via
 * two `vkCmdDispatch` compute passes against `Shaders/GaussianBlur.glsl`'s compiled SPIR-V,
 * matching `PHASE_35_PROPOSAL.md`'s own description of the Vulkan backend's blur ("compute
 * shaders use `vkCmdDispatch`. Blur ping-pong uses transient `VkImage` resources allocated via
 * VMA"). See `GaussianBlur.glsl`'s own file comment for why the Gaussian weight formula is
 * computed inline (matching `BlurPassGL3`'s identical choice) rather than precomputed into a UBO
 * array the way the original Phase 34 proposal's compute-shader section describes.
 *
 * `Apply()` is entirely self-contained: it allocates its own one-shot command buffer, records
 * both passes (with the mandatory memory barrier between them — `vkCmdDispatch` calls on the
 * same queue give no implicit ordering guarantee for the *data* one writes and the next reads,
 * unlike a render pass's subpass dependencies), submits, and `vkWaitForFences()`s synchronously
 * before returning — the same deliberate, documented stopgap `NativeRendererVulkan::Render()`
 * already uses (see that class's own file comment). This keeps `Apply()` trivially testable in
 * isolation, matching `BlurPassGL3::Apply()`'s own "just call it, get a real result back"
 * contract, with no caller-managed command buffer lifetime.
 *
 * Ping-pong targets and the shared source-facing descriptor set's binding are all kept in
 * `VK_IMAGE_LAYOUT_GENERAL` for their entire lifetime once created — the layout both
 * `imageLoad`/`imageStore` require, and there is no other use (sampling, attachment) that would
 * benefit from a more specific layout, so no per-pass layout transition is needed, only the
 * one-time `UNDEFINED -> GENERAL` transition when a target is first (re)allocated.
 *
 * `Apply()`'s `source` is a precondition, not something this class manages: the caller must
 * already have it in `VK_IMAGE_LAYOUT_GENERAL`, with any prior writes to it already complete and
 * visible (e.g. resolved by that write's own fence wait, mirroring how
 * `NativeRendererVulkan::Render()`'s own synchronous stall makes its output visible to whatever
 * runs next with no extra barrier needed). `Apply()` never transitions or barriers the source
 * image itself, only its own owned ping-pong targets.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>

namespace ImFrame::Internal {

/**
 * @class    BlurPassVulkan
 * @brief    Applies a separable Gaussian blur to a `VK_FORMAT_R8G8B8A8_UNORM` image via two compute-shader ping-pong passes
 *
 * @internal
 * Vulkan resources (shader module, descriptor set layout/pool/sets, pipeline layout, pipeline,
 * ping-pong images) are created lazily on the first `Apply()` call, matching
 * `NativeRendererVulkan::EnsureInitialized()`'s own lazy-initialization convention —
 * constructing an instance never issues a single Vulkan call.
 *
 * @since    3.0.1
 */
/// A blurred result is always both handles together -- `VkImageView` alone can't be read back,
/// barriered, or copied by the caller afterward (no Vulkan call recovers a `VkImage` from a
/// `VkImageView`), unlike `BlurPassGL3::Apply()`'s single `unsigned int` GL texture name, which
/// already serves both roles at once.
struct BlurResult {
    VkImage     Image = VK_NULL_HANDLE;
    VkImageView View  = VK_NULL_HANDLE;
};

class BlurPassVulkan {
public:
    /**
     * @brief    Constructs a blur pass bound to the given (not owned) Vulkan device/queue/pool.
     * @param[in] device        A valid, already-created `VkDevice`.
     * @param[in] allocator     A `VmaAllocator` created against the same device.
     * @param[in] graphicsQueue A queue supporting compute operations (any graphics-capable queue
     *                          on this codebase's supported hardware also supports compute).
     * @param[in] commandPool   Pool `Apply()` allocates its one-shot command buffer from. Must
     *                          support `vkResetCommandBuffer` (i.e. created with
     *                          `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`).
     *
     * None of these handles are owned — the caller is responsible for their lifetime outliving
     * this object and for calling `Shutdown()` before destroying them.
     */
    BlurPassVulkan(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool);
    ~BlurPassVulkan();

    BlurPassVulkan(const BlurPassVulkan&) = delete;
    BlurPassVulkan& operator=(const BlurPassVulkan&) = delete;

    /**
     * @brief    Blurs `source` with a separable Gaussian kernel and returns the result.
     * @param[in] source  The image to blur. Not modified. Must already be
     *                    `VK_FORMAT_R8G8B8A8_UNORM`, exactly `width` x `height`, usage including
     *                    `VK_IMAGE_USAGE_STORAGE_BIT`, and in `VK_IMAGE_LAYOUT_GENERAL` — see
     *                    this class's own file comment on why `Apply()` does not transition it
     *                    itself. `source.Image` is otherwise unused by the blur itself (binding
     *                    is by view); it exists on this parameter only so the `radius <= 0`
     *                    no-op path below can hand back a complete, consistent `BlurResult`.
     * @param[in] width   Image width, in texels. Must match `source`'s actual width.
     * @param[in] height  Image height, in texels. Must match `source`'s actual height.
     * @param[in] radius  Blur radius in pixels, clamped to `[0, 64]`. `<= 0` is a no-op.
     * @return   `source` unchanged if `radius <= 0`; otherwise a `VK_FORMAT_R8G8B8A8_UNORM`
     *           result (image + view, both in `VK_IMAGE_LAYOUT_GENERAL`, usage including
     *           `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` so a caller can read or copy it) owned by this
     *           `BlurPassVulkan`, valid until the next `Apply()` call or destruction. The caller
     *           must not destroy either handle.
     */
    [[nodiscard]] BlurResult Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius);

    /// Releases all Vulkan resources this pass itself created. Safe to call multiple times,
    /// including before any `Apply()` call. Does not touch the constructor's borrowed handles.
    void Shutdown();

private:
    void EnsureInitialized();
    void EnsureTargets(std::uint32_t width, std::uint32_t height);
    void RecordPass(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, std::uint32_t width, std::uint32_t height,
                    float directionX, float directionY, float radius);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    VkDevice      _device      = VK_NULL_HANDLE;
    VmaAllocator  _allocator   = VK_NULL_HANDLE;
    VkQueue       _queue       = VK_NULL_HANDLE;
    VkCommandPool _commandPool = VK_NULL_HANDLE;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    VkShaderModule        _computeModule       = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      _pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            _pipeline            = VK_NULL_HANDLE;

    /// Two sets sharing `_descriptorSetLayout` (binding 0 = readonly source storage image,
    /// binding 1 = writeonly destination storage image): `_descriptorSetA`'s binding 1 (= `_viewA`)
    /// and `_descriptorSetB` (binding 0 = `_viewA`, binding 1 = `_viewB`) are written once, in
    /// `EnsureTargets()`, and never change again at a given size — only `_descriptorSetA`'s
    /// binding 0 changes, once per `Apply()` call, to that call's own `sourceView`. Unlike
    /// `NativeRendererVulkan`'s per-batch Image descriptor sets, no "content resolved at
    /// execution time" hazard exists here: each `Apply()` call is one fully self-contained,
    /// synchronously-awaited submission — there is never more than one not-yet-submitted command
    /// buffer referencing these sets at a time.
    VkDescriptorPool _descriptorPool  = VK_NULL_HANDLE;
    VkDescriptorSet  _descriptorSetA  = VK_NULL_HANDLE;
    VkDescriptorSet  _descriptorSetB  = VK_NULL_HANDLE;

    /// Ping-pong targets: pass 1 (horizontal) reads the caller's `sourceView`, writes `_imageA`;
    /// pass 2 (vertical) reads `_imageA`, writes `_imageB` (the view `Apply()` returns).
    /// Reallocated only when the requested size differs from `_targetWidth`/`_targetHeight`, kept
    /// in `VK_IMAGE_LAYOUT_GENERAL` for their whole lifetime once created (see this file's own
    /// comment on why no per-pass transition is needed).
    VkImage       _imageA           = VK_NULL_HANDLE;
    VmaAllocation _imageAAllocation = VK_NULL_HANDLE;
    VkImageView   _viewA            = VK_NULL_HANDLE;
    VkImage       _imageB           = VK_NULL_HANDLE;
    VmaAllocation _imageBAllocation = VK_NULL_HANDLE;
    VkImageView   _viewB            = VK_NULL_HANDLE;
    std::uint32_t _targetWidth      = 0;
    std::uint32_t _targetHeight     = 0;

    /// Set by `EnsureTargets()` when it just (re)allocated `_imageA`/`_imageB` — tells the next
    /// `Apply()` call's own command buffer to record the one-time `UNDEFINED -> GENERAL`
    /// transition for both, then cleared. Deferred to `Apply()`'s own command buffer because
    /// `EnsureTargets()` itself has none available (image *creation* needs no command buffer;
    /// the layout *transition* does).
    bool _targetsNeedInitialTransition = false;

    /// Reused every `Apply()` call — signalled once the one-shot command buffer completes, so
    /// `Apply()` can safely reuse `_imageA`/`_imageB` on its *next* call.
    VkFence _submitFence = VK_NULL_HANDLE;
};

} // namespace ImFrame::Internal
