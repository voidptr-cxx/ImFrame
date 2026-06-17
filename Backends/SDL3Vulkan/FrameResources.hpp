/**
 * @file     FrameResources.hpp
 * @brief    Per-frame-in-flight Vulkan synchronisation and command recording resources
 *
 * Each frame slot owns the fences, semaphores, command pool, and command buffer
 * needed to record and submit one frame of work independently from concurrently
 * in-flight frames. The backend indexes into a per-window array via
 * `frameIndex % framesInFlight`.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace ImFrame::Internal {

/**
 * @struct FrameResources
 * @brief  Vulkan objects owned by a single frame-in-flight slot for one window
 *
 * @since 2.0.0
 */
struct FrameResources {
    VkSemaphore     imageAvailableSemaphore = VK_NULL_HANDLE; ///< Signaled when the swap chain image is ready.
    VkSemaphore     renderFinishedSemaphore = VK_NULL_HANDLE; ///< Signaled when rendering is complete.
    VkFence         inFlightFence           = VK_NULL_HANDLE; ///< CPU-side fence; waited at frame start, created signaled.
    VkCommandPool   commandPool             = VK_NULL_HANDLE; ///< Reset once per frame (O(1) bulk free).
    VkCommandBuffer commandBuffer           = VK_NULL_HANDLE; ///< Re-recorded every frame.
};

} // namespace ImFrame::Internal
