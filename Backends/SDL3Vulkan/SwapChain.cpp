/**
 * @file     SwapChain.cpp
 * @brief    Vulkan swap chain creation, format/present-mode selection, and destruction
 *
 * @internal
 * Surface format priority (SDR): B8G8R8A8_SRGB → R8G8B8A8_SRGB → first available.
 * Surface format priority (HDR): R16G16B16A16_SFLOAT+EXTENDED_SRGB_LINEAR → SDR fallback.
 * Present mode priority: On=FIFO, Off=IMMEDIATE→FIFO, Adaptive=MAILBOX→FIFO_RELAXED→FIFO.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "SwapChain.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ImFrame::Internal {

namespace {

// ─── Surface format selection ─────────────────────────────────────────────────

VkSurfaceFormatKHR SelectSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats,
    bool hdrOutput)
{
    if (hdrOutput) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
                f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
                return f;
            }
        }
        // Fall through to SDR if HDR format is unavailable.
    }
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats.front();
}

// ─── Present mode selection ───────────────────────────────────────────────────

VkPresentModeKHR SelectPresentMode(
    const std::vector<VkPresentModeKHR>& modes,
    VSyncMode vsync)
{
    auto has = [&](VkPresentModeKHR m) {
        return std::find(modes.begin(), modes.end(), m) != modes.end();
    };

    switch (vsync) {
        case VSyncMode::Off:
            if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        case VSyncMode::Adaptive:
            if (has(VK_PRESENT_MODE_MAILBOX_KHR))       return VK_PRESENT_MODE_MAILBOX_KHR;
            if (has(VK_PRESENT_MODE_FIFO_RELAXED_KHR))  return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            break;
        case VSyncMode::On:
        default:
            break;
    }
    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed available on all Vulkan implementations
}

// ─── Extent clamping ─────────────────────────────────────────────────────────

VkExtent2D SelectExtent(
    const VkSurfaceCapabilitiesKHR& caps,
    int drawableWidth,
    int drawableHeight)
{
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent; // fixed-size surface
    }
    return VkExtent2D{
        std::clamp(static_cast<uint32_t>(drawableWidth),
                   caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(drawableHeight),
                   caps.minImageExtent.height, caps.maxImageExtent.height),
    };
}

} // anonymous namespace

// ─── SwapChain::Create ────────────────────────────────────────────────────────

VoidResult SwapChain::Create(const SwapChainDesc& desc)
{
    // ── Query surface capabilities, formats, present modes ────────────────────
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            desc.physicalDevice, desc.surface, &caps) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(desc.physicalDevice, desc.surface, &formatCount, nullptr);
    if (formatCount == 0) return std::unexpected(Error::GraphicsInitFailed);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(desc.physicalDevice, desc.surface, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(desc.physicalDevice, desc.surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) return std::unexpected(Error::GraphicsInitFailed);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        desc.physicalDevice, desc.surface, &presentModeCount, presentModes.data());

    // ── Select format, present mode, extent, image count ─────────────────────
    VkSurfaceFormatKHR surfFmt = SelectSurfaceFormat(formats, desc.hdrOutput);
    VkPresentModeKHR   present = SelectPresentMode(presentModes, desc.vsyncMode);
    VkExtent2D         ext     = SelectExtent(caps, desc.drawableWidth, desc.drawableHeight);

    uint32_t imgCount = caps.minImageCount + 1;
    if (desc.minImageCount > imgCount) imgCount = desc.minImageCount;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount) {
        imgCount = caps.maxImageCount;
    }

    // ── Create swap chain ─────────────────────────────────────────────────────
    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = desc.surface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = surfFmt.format;
    ci.imageColorSpace  = surfFmt.colorSpace;
    ci.imageExtent      = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // TRANSFER_SRC enables ReadPixels() to copy the presented image to a
    // staging buffer; nearly universally supported, but checked defensively.
    supportsReadback = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (supportsReadback) {
        ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = present;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = desc.oldSwapchain;

    uint32_t queueFamilies[] = { desc.graphicsFamily, desc.presentFamily };
    if (desc.graphicsFamily != desc.presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = queueFamilies;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(desc.device, &ci, nullptr, &handle) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    format     = surfFmt.format;
    colorSpace = surfFmt.colorSpace;
    extent     = ext;

    // ── Retrieve swap chain images ────────────────────────────────────────────
    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(desc.device, handle, &actualCount, nullptr);
    images.resize(actualCount);
    vkGetSwapchainImagesKHR(desc.device, handle, &actualCount, images.data());
    imageCount = actualCount;

    // ── Create per-image views ────────────────────────────────────────────────
    imageViews.resize(actualCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < actualCount; ++i) {
        VkImageViewCreateInfo viewCi{};
        viewCi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCi.image    = images[i];
        viewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCi.format   = format;
        viewCi.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        viewCi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(desc.device, &viewCi, nullptr, &imageViews[i]) != VK_SUCCESS) {
            Destroy(desc.device);
            return std::unexpected(Error::GraphicsInitFailed);
        }

        // ── Name objects in debug builds ──────────────────────────────────────
        if (desc.setObjectName) {
            std::string nameImg  = std::string(desc.tag) + "_SwapImage_"  + std::to_string(i);
            std::string nameView = std::string(desc.tag) + "_SwapView_"   + std::to_string(i);
            desc.setObjectName(VK_OBJECT_TYPE_IMAGE,      reinterpret_cast<uint64_t>(images[i]),      nameImg.c_str());
            desc.setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageViews[i]), nameView.c_str());
        }
    }

    if (desc.setObjectName) {
        std::string scName = std::string(desc.tag) + "_SwapChain";
        desc.setObjectName(VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(handle), scName.c_str());
    }

    return {};
}

// ─── SwapChain::Destroy ───────────────────────────────────────────────────────

void SwapChain::Destroy(VkDevice device)
{
    for (auto view : imageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    }
    imageViews.clear();
    images.clear();
    imageCount = 0;

    if (handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

    format           = VK_FORMAT_UNDEFINED;
    colorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    extent           = {};
    supportsReadback = false;
}

} // namespace ImFrame::Internal
