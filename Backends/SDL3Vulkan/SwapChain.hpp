/**
 * @file     SwapChain.hpp
 * @brief    Vulkan swap chain encapsulation for a single SDL3 window surface
 *
 * `SwapChain` manages the VkSwapchainKHR, its images, and per-image colour
 * attachment views. It is created once per window and recreated on resize.
 * Resize recreation passes the old handle as `oldSwapchain` so the driver can
 * reuse physical memory without a full idle stall.
 *
 * Surface format, present mode, and image count are selected inside `Create()`
 * using the heuristics specified in the Phase 20 proposal.
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

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Core/Error.hpp"

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

namespace ImFrame::Internal {

/**
 * @struct SwapChainDesc
 * @brief  Parameters for `SwapChain::Create()`
 * @since  2.0.0
 */
struct SwapChainDesc {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; ///< Used to query surface caps/formats.
    VkDevice         device         = VK_NULL_HANDLE; ///< Owning logical device.
    VkSurfaceKHR     surface        = VK_NULL_HANDLE; ///< Target presentation surface.
    VSyncMode        vsyncMode      = VSyncMode::On;
    bool             hdrOutput      = false;
    uint32_t         graphicsFamily = 0;              ///< Graphics queue family index.
    uint32_t         presentFamily  = 0;              ///< Present queue family index.
    int              drawableWidth  = 0;              ///< Drawable pixel width from SDL.
    int              drawableHeight = 0;              ///< Drawable pixel height from SDL.
    uint32_t         minImageCount  = 3;              ///< Minimum images to request.
    VkSwapchainKHR   oldSwapchain   = VK_NULL_HANDLE; ///< Previous handle for zero-stall recreation.
    /// Optional debug naming callback — `(type, handle, name)`.
    std::function<void(VkObjectType, uint64_t, const char*)> setObjectName;
    std::string_view tag;                             ///< Name prefix for object labels.
};

/**
 * @struct SwapChain
 * @brief  Owns a VkSwapchainKHR together with its images and colour attachment views
 *
 * Lifetime mirrors the window surface. Call `Create()` after surface creation and
 * `Destroy()` before surface destruction. During resize: create a new SwapChain
 * (with `desc.oldSwapchain` set to the current handle), then destroy the old one.
 *
 * @since 2.0.0
 */
struct SwapChain {
    VkSwapchainKHR           handle     = VK_NULL_HANDLE;     ///< The swap chain.
    VkFormat                 format     = VK_FORMAT_UNDEFINED; ///< Colour attachment format.
    VkColorSpaceKHR          colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D               extent     = {};
    std::vector<VkImage>     images;     ///< Swap chain images (not owned — destroyed with the swap chain).
    std::vector<VkImageView> imageViews; ///< Per-image colour attachment view (owned).
    uint32_t                 imageCount = 0;

    /**
     * @brief   Create the swap chain and its per-image views.
     *
     * @param[in]  desc  Creation parameters.
     * @return  Empty result on success; an Error on failure.
     * @throws  Nothing.
     */
    VoidResult Create(const SwapChainDesc& desc);

    /**
     * @brief   Destroy all owned objects. Safe to call on a default-constructed instance.
     *
     * @param[in]  device  The owning logical device.
     */
    void Destroy(VkDevice device);
};

} // namespace ImFrame::Internal
