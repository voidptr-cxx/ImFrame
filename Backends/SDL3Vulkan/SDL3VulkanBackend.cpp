/**
 * @file     SDL3VulkanBackend.cpp
 * @brief    SDL3 + Vulkan 1.3 backend — Init, Poll, BeginFrame, EndFrame, Shutdown
 *
 * @internal
 * Initialisation sequence:
 *   InitSDL → CreateInstance → CreateDebugMessenger → create SDL window +
 *   Vulkan surface → SelectPhysicalDevice → CreateDevice → InitVMA →
 *   CreateDescriptorPool → CreateViewportCommandPool →
 *   CreateSwapChainFor(primary) → AllocateFrameResources(primary) →
 *   InitImGui → show window. Font atlas texture upload happens lazily on
 *   first RenderDrawData() via imgui's dynamic texture update system.
 *
 * All GPU memory goes through VMA. No vkAllocateMemory calls appear here.
 * No VkRenderPass or VkFramebuffer objects — dynamic rendering only.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "SDL3VulkanBackend.hpp"
#include "InputTranslation.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <SDL3/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ImFrame::Internal {

namespace {

constexpr int   DEFAULT_DESCRIPTOR_POOL_SIZE = 1000;
constexpr float DELTA_TIME_MAX               = 0.1f; // cap to prevent physics blow-up
constexpr float DEFAULT_DELTA_TIME           = 1.0f / 60.0f;
constexpr float CLEAR_R                      = 0.06f;
constexpr float CLEAR_G                      = 0.06f;
constexpr float CLEAR_B                      = 0.06f;

// Clamp float delta to [0, DELTA_TIME_MAX].
float ClampDelta(float dt) noexcept
{
    if (dt < 0.0f)           return 0.0f;
    if (dt > DELTA_TIME_MAX) return DELTA_TIME_MAX;
    return dt;
}

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

SDL3VulkanBackend::~SDL3VulkanBackend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::Init(const WindowConfig& config)
{
    if (_initialised) return std::unexpected(Error::AlreadyInitialised);
    if (config.Width <= 0 || config.Height <= 0) return std::unexpected(Error::InvalidArgument);

    _primaryConfig   = config;
    _framesInFlight  = (config.FramesInFlight > 0) ? config.FramesInFlight : 2;

    if (auto r = InitSDL(config); !r) return r;
    if (auto r = CreateInstance(config); !r) { SDL_Vulkan_UnloadLibrary(); SDL_Quit(); return r; }
    if (auto r = CreateDebugMessenger(); !r) {
        vkDestroyInstance(_instance, nullptr); SDL_Vulkan_UnloadLibrary(); SDL_Quit(); return r;
    }

    // Create primary SDL window.
    SDL_WindowFlags wflags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                             SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    _primary.sdlWindow = SDL_CreateWindow(
        std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!_primary.sdlWindow) {
        Shutdown();
        return std::unexpected(Error::WindowCreationFailed);
    }
    _primary.config = config;

    // Create Vulkan surface for the primary window.
    if (!SDL_Vulkan_CreateSurface(_primary.sdlWindow, _instance, nullptr, &_primary.surface)) {
        Shutdown();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    if (auto r = SelectPhysicalDevice(); !r) { Shutdown(); return r; }
    if (auto r = CreateDevice();         !r) { Shutdown(); return r; }
    if (auto r = InitVMA();              !r) { Shutdown(); return r; }
    if (auto r = CreateDescriptorPool(); !r) { Shutdown(); return r; }
    if (auto r = CreateViewportCommandPool(); !r) { Shutdown(); return r; }
    if (auto r = CreateSwapChainFor(_primary);       !r) { Shutdown(); return r; }
    if (auto r = AllocateFrameResources(_primary);   !r) { Shutdown(); return r; }
    if (auto r = InitImGui(config);      !r) { Shutdown(); return r; }

    // Font atlas texture is created lazily by ImGui_ImplVulkan_RenderDrawData()
    // via the dynamic texture update system (ImGuiBackendFlags_RendererHasTextures) —
    // no explicit upload call needed with this imgui version.
    SDL_ShowWindow(_primary.sdlWindow);

    // Seed the SDL window-ID → handle map for the primary window.
    SDL_WindowID primaryId = SDL_GetWindowID(_primary.sdlWindow);
    _sdlIdToHandle[primaryId] = PrimaryWindow;

    _lastPerfCount = SDL_GetPerformanceCounter();
    _initialised   = true;
    return {};
}

// ─── InitSDL ──────────────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::InitSDL(const WindowConfig& /*config*/)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return std::unexpected(Error::WindowCreationFailed);
    }
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // Must be loaded before SDL_Vulkan_GetInstanceExtensions()/SDL_Vulkan_CreateSurface()
    // are called — CreateInstance() runs before the SDL window exists, so the
    // window's implicit SDL_WINDOW_VULKAN load doesn't happen in time.
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        SDL_Quit();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── CreateInstance ───────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateInstance(const WindowConfig& config)
{
    // Collect required SDL3 extensions.
    uint32_t sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts) return std::unexpected(Error::GraphicsInitFailed);

    std::vector<const char*> extensions(sdlExts, sdlExts + sdlExtCount);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    std::vector<const char*> layers;

#if !defined(NDEBUG)
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Request validation layer if available.
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availLayers.data());

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    for (const auto& lp : availLayers) {
        if (std::strcmp(lp.layerName, validationLayer) == 0) {
            layers.push_back(validationLayer);
            break;
        }
    }
#endif

    std::string appName(config.Title);

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "ImFrame";
    appInfo.engineVersion      = VK_MAKE_VERSION(2, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();

    if (vkCreateInstance(&ci, nullptr, &_instance) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    // Load extension function pointers.
    _pfnSetObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(_instance, "vkSetDebugUtilsObjectNameEXT"));
    _pfnDestroyDebugMsgr = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));

    return {};
}

// ─── CreateDebugMessenger ─────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateDebugMessenger()
{
#if defined(NDEBUG)
    return {};
#else
    auto vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!vkCreateDebugUtilsMessengerEXT) return {};

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = DebugCallback;
    ci.pUserData       = this;

    vkCreateDebugUtilsMessengerEXT(_instance, &ci, nullptr, &_debugMessenger);
    return {};
#endif
}

// ─── SelectPhysicalDevice ─────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::SelectPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(_instance, &count, nullptr);
    if (count == 0) return std::unexpected(Error::GraphicsInitFailed);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(_instance, &count, devices.data());

    VkPhysicalDevice best  = VK_NULL_HANDLE;
    int              bestScore = -1;

    for (auto dev : devices) {
        // Hard requirements: must support the primary surface and VK_KHR_swapchain.
        uint32_t qFamCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qFamCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfps(qFamCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qFamCount, qfps.data());

        bool hasGraphics = false;
        for (const auto& qfp : qfps) {
            if (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                hasGraphics = true;
                break;
            }
        }
        if (!hasGraphics) continue;

        VkBool32 surfaceSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, 0, _primary.surface, &surfaceSupport);
        if (!surfaceSupport) continue;

        // Check VK_KHR_swapchain is available.
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (const auto& ext : exts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) continue;

        // Score by device type.
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = 0;
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score = 1000; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 100;  break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            score = 1;    break;
            default:                                     score = 0;    break;
        }

        // Tie-break by device-local VRAM.
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(dev, &memProps);
        for (uint32_t h = 0; h < memProps.memoryHeapCount; ++h) {
            if (memProps.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                score += static_cast<int>(memProps.memoryHeaps[h].size / (1024 * 1024 * 1024));
            }
        }

        if (score > bestScore) {
            bestScore      = score;
            best           = dev;
        }
    }

    if (best == VK_NULL_HANDLE) return std::unexpected(Error::GraphicsInitFailed);
    _physicalDevice = best;
    return {};
}

// ─── CreateDevice ─────────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateDevice()
{
    // Select queue families.
    uint32_t qFamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qFamCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfps(qFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qFamCount, qfps.data());

    _graphicsFamily = UINT32_MAX;
    _presentFamily  = UINT32_MAX;
    uint32_t universalFamily = UINT32_MAX;

    for (uint32_t i = 0; i < qFamCount; ++i) {
        // VK_QUEUE_GRAPHICS_BIT only — a compute-only family cannot record
        // vkCmdBeginRendering/dynamic-rendering draw commands.
        bool graphics = (qfps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, _primary.surface, &present);

        if (graphics && present) {
            universalFamily = i;
        }
        if (graphics && _graphicsFamily == UINT32_MAX) _graphicsFamily = i;
        if (present  && _presentFamily  == UINT32_MAX) _presentFamily  = i;
    }

    if (universalFamily != UINT32_MAX) {
        _graphicsFamily = universalFamily;
        _presentFamily  = universalFamily;
    }

    if (_graphicsFamily == UINT32_MAX || _presentFamily == UINT32_MAX) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    float qPriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    auto addQueue = [&](uint32_t family) {
        for (const auto& qci : queueCIs) {
            if (qci.queueFamilyIndex == family) return;
        }
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &qPriority;
        queueCIs.push_back(qci);
    };
    addQueue(_graphicsFamily);
    addQueue(_presentFamily);

    // Required device extensions.
    std::vector<const char*> devExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // macOS portability subset (MoltenVK).
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availExts(extCount);
    vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extCount, availExts.data());
    for (const auto& ext : availExts) {
        if (std::strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
            devExts.push_back("VK_KHR_portability_subset");
            break;
        }
    }

    // Enable Vulkan 1.3 features.
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;
    features13.maintenance4     = VK_TRUE;

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &features13;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(queueCIs.size());
    dci.pQueueCreateInfos       = queueCIs.data();
    dci.enabledExtensionCount   = static_cast<uint32_t>(devExts.size());
    dci.ppEnabledExtensionNames = devExts.data();

    if (vkCreateDevice(_physicalDevice, &dci, nullptr, &_device) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    vkGetDeviceQueue(_device, _graphicsFamily, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _presentFamily,  0, &_presentQueue);
    return {};
}

// ─── InitVMA ──────────────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::InitVMA()
{
    VmaAllocatorCreateInfo ci{};
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.physicalDevice   = _physicalDevice;
    ci.device           = _device;
    ci.instance         = _instance;

    if (vmaCreateAllocator(&ci, &_vmaAllocator) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── CreateDescriptorPool ─────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 5> poolSizes{{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DEFAULT_DESCRIPTOR_POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                DEFAULT_DESCRIPTOR_POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          DEFAULT_DESCRIPTOR_POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100 },
    }};

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = DEFAULT_DESCRIPTOR_POOL_SIZE + 200;
    ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    ci.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(_device, &ci, nullptr, &_descriptorPool) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    NameObject(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
               reinterpret_cast<uint64_t>(_descriptorPool), "ImFrame_DescriptorPool");
    return {};
}

// ─── CreateViewportCommandPool ────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateViewportCommandPool()
{
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = _graphicsFamily;

    if (vkCreateCommandPool(_device, &ci, nullptr, &_viewportCmdPool) != VK_SUCCESS) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    NameObject(VK_OBJECT_TYPE_COMMAND_POOL,
               reinterpret_cast<uint64_t>(_viewportCmdPool), "ImFrame_ViewportCmdPool");
    return {};
}

// ─── CreateSwapChainFor ───────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::CreateSwapChainFor(WindowData& wd)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);

    SwapChainDesc desc{};
    desc.physicalDevice = _physicalDevice;
    desc.device         = _device;
    desc.surface        = wd.surface;
    desc.vsyncMode      = wd.config.VSync;
    desc.hdrOutput      = wd.config.HDROutput;
    desc.graphicsFamily = _graphicsFamily;
    desc.presentFamily  = _presentFamily;
    desc.drawableWidth  = w;
    desc.drawableHeight = h;
    desc.minImageCount  = static_cast<uint32_t>(_framesInFlight) + 1;
    desc.oldSwapchain   = wd.swapChain.handle;
    desc.setObjectName  = [this](VkObjectType type, uint64_t h2, const char* name) {
        NameObject(type, h2, name);
    };
    desc.tag = "Primary";

    auto oldHandle = wd.swapChain.handle;
    VoidResult r = wd.swapChain.Create(desc);
    // Destroy old swap chain after creating the new one.
    if (oldHandle != VK_NULL_HANDLE && wd.swapChain.handle != oldHandle) {
        vkDestroySwapchainKHR(_device, oldHandle, nullptr);
    }
    return r;
}

// ─── AllocateFrameResources ───────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::AllocateFrameResources(WindowData& wd)
{
    wd.frames.resize(static_cast<std::size_t>(_framesInFlight));

    VkSemaphoreCreateInfo semCI{};
    semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Create fence in signaled state so the first frame doesn't deadlock.
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCI.queueFamilyIndex = _graphicsFamily;

    for (std::size_t i = 0; i < wd.frames.size(); ++i) {
        auto& f = wd.frames[i];
        if (vkCreateSemaphore(_device, &semCI,  nullptr, &f.imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(_device, &semCI,  nullptr, &f.renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence    (_device, &fenceCI, nullptr, &f.inFlightFence)           != VK_SUCCESS ||
            vkCreateCommandPool(_device, &poolCI, nullptr, &f.commandPool)             != VK_SUCCESS) {
            return std::unexpected(Error::GraphicsInitFailed);
        }

        VkCommandBufferAllocateInfo cbAlloc{};
        cbAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAlloc.commandPool        = f.commandPool;
        cbAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAlloc.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(_device, &cbAlloc, &f.commandBuffer) != VK_SUCCESS) {
            return std::unexpected(Error::GraphicsInitFailed);
        }

        // Name frame resources in debug builds.
        std::string tag = "Frame" + std::to_string(i);
        NameObject(VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(f.imageAvailableSemaphore),
                   (tag + "_ImageAvailSem").c_str());
        NameObject(VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(f.renderFinishedSemaphore),
                   (tag + "_RenderDoneSem").c_str());
        NameObject(VK_OBJECT_TYPE_FENCE,
                   reinterpret_cast<uint64_t>(f.inFlightFence), (tag + "_InFlightFence").c_str());
        NameObject(VK_OBJECT_TYPE_COMMAND_POOL,
                   reinterpret_cast<uint64_t>(f.commandPool), (tag + "_CmdPool").c_str());
        NameObject(VK_OBJECT_TYPE_COMMAND_BUFFER,
                   reinterpret_cast<uint64_t>(f.commandBuffer), (tag + "_CmdBuf").c_str());
    }
    return {};
}

// ─── InitImGui ────────────────────────────────────────────────────────────────

VoidResult SDL3VulkanBackend::InitImGui(const WindowConfig& config)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (config.Docking)   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (config.Viewports) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForVulkan(_primary.sdlWindow)) {
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    VkPipelineRenderingCreateInfoKHR pipelineRendering{};
    pipelineRendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRendering.colorAttachmentCount    = 1;
    pipelineRendering.pColorAttachmentFormats = &_primary.swapChain.format;

    ImGui_ImplVulkan_InitInfo vkInfo{};
    vkInfo.Instance                                      = _instance;
    vkInfo.PhysicalDevice                                = _physicalDevice;
    vkInfo.Device                                        = _device;
    vkInfo.QueueFamily                                   = _graphicsFamily;
    vkInfo.Queue                                         = _graphicsQueue;
    vkInfo.DescriptorPool                                = _descriptorPool;
    vkInfo.PipelineInfoMain.RenderPass                   = VK_NULL_HANDLE; // dynamic rendering
    vkInfo.MinImageCount                                 = static_cast<uint32_t>(_framesInFlight);
    vkInfo.ImageCount                                    = _primary.swapChain.imageCount;
    vkInfo.PipelineInfoMain.MSAASamples                  = VK_SAMPLE_COUNT_1_BIT;
    vkInfo.UseDynamicRendering                           = true;
    vkInfo.PipelineInfoMain.PipelineRenderingCreateInfo  = pipelineRendering;

    if (!ImGui_ImplVulkan_Init(&vkInfo)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo SDL3VulkanBackend::Poll()
{
    IMF_ASSERT(_initialised);

    namespace IT = InputTranslation;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT:
                _shouldClose = true;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                _inputQueue.push_back(IT::TranslateKey(event.key));
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (auto opt = IT::TranslateTextInput(event.text)) {
                    _inputQueue.push_back(*opt);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                _inputQueue.push_back(IT::TranslateMouseButton(event.button));
                break;

            case SDL_EVENT_MOUSE_MOTION:
                _inputQueue.push_back(IT::TranslateMouseMotion(event.motion));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                _inputQueue.push_back(IT::TranslateMouseWheel(event.wheel));
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                _inputQueue.push_back(IT::TranslateGamepadAxis(event.gaxis));
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                _inputQueue.push_back(IT::TranslateGamepadButton(event.gbutton));
                break;

            case SDL_EVENT_FINGER_DOWN:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Began));
                break;
            case SDL_EVENT_FINGER_MOTION:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Moved));
                break;
            case SDL_EVENT_FINGER_UP:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Ended));
                break;

            case SDL_EVENT_WINDOW_RESIZED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowResize(event.window, wh));
                // Flag swap chain recreation for the affected window.
                if (wh == PrimaryWindow) {
                    _primary.needsResize = true;
                } else if (auto* sec = FindWindow(wh)) {
                    sec->needsResize = true;
                }
                break;
            }

            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowFocus(wh, true));
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowFocus(wh, false));
                break;
            }

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowClose(wh));
                if (wh == PrimaryWindow) _shouldClose = true;
                break;
            }

            default:
                break;
        }
    }

    // ── Delta time ────────────────────────────────────────────────────────────
    uint64_t now  = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    float dt = (_lastPerfCount > 0 && freq > 0)
                   ? ClampDelta(static_cast<float>(now - _lastPerfCount) / static_cast<float>(freq))
                   : DEFAULT_DELTA_TIME;
    _lastPerfCount = now;

    // ── Display refresh interval ──────────────────────────────────────────────
    float refreshInterval = DEFAULT_DELTA_TIME;
    SDL_DisplayID displayId = SDL_GetDisplayForWindow(_primary.sdlWindow);
    if (displayId != 0) {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
        if (mode && mode->refresh_rate > 0.0f) {
            refreshInterval = 1.0f / mode->refresh_rate;
        }
    }

    // ── Active windows ────────────────────────────────────────────────────────
    std::vector<WindowHandle> active;
    active.reserve(1 + _secondaryWindows.size());
    active.push_back(PrimaryWindow);
    for (const auto& [h, _] : _secondaryWindows) active.push_back(h);

    return FrameInfo{
        .ShouldClose            = _shouldClose,
        .DeltaTime              = dt,
        .DisplayRefreshInterval = refreshInterval,
        .ActiveWindows          = std::move(active),
    };
}

// ─── BeginFrame ───────────────────────────────────────────────────────────────

void SDL3VulkanBackend::BeginFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    // Handle pending resize.
    if (wd.needsResize) {
        RecreateSwapChain(wd);
        wd.needsResize = false;
    }

    // Skip frame if window has zero area (minimized / occluded).
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &pw, &ph);
    if (pw == 0 || ph == 0) {
        wd.frameStarted = false;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];

    // Wait for the previous use of this frame slot to complete.
    vkWaitForFences(_device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences  (_device, 1, &frame.inFlightFence);

    // Reset the per-frame command pool (O(1) bulk free of all allocations).
    vkResetCommandPool(_device, frame.commandPool, 0);

    // Acquire the next swap chain image.
    if (!AcquireNextImage(wd)) {
        // OUT_OF_DATE — swap chain was just recreated; skip this frame.
        wd.frameStarted = false;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    wd.frameStarted = true;

    // ── Record: image layout transition → begin dynamic rendering ─────────────
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

    // Transition swap chain image: ??? → COLOR_ATTACHMENT_OPTIMAL.
    VkImageMemoryBarrier2 toAttachment{};
    toAttachment.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toAttachment.srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
    toAttachment.srcAccessMask       = VK_ACCESS_2_NONE;
    toAttachment.dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toAttachment.dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toAttachment.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    toAttachment.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toAttachment.image               = wd.swapChain.images[wd.imageIndex];
    toAttachment.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo dep{};
    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &toAttachment;
    vkCmdPipelineBarrier2(frame.commandBuffer, &dep);

    // Begin dynamic rendering with a clear.
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView   = wd.swapChain.imageViews[wd.imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue  = { .color = { .float32 = { CLEAR_R, CLEAR_G, CLEAR_B, 1.0f } } };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea           = { {0, 0}, wd.swapChain.extent };
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void SDL3VulkanBackend::EndFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    ImGui::Render();

    if (!wd.frameStarted) {
        ImGui::EndFrame();
        return;
    }

    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];

    // Record ImGui draw commands into the active dynamic rendering scope.
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);

    vkCmdEndRendering(frame.commandBuffer);

    // Transition swap chain image: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR.
    VkImageMemoryBarrier2 toPresent{};
    toPresent.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toPresent.srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresent.srcAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.dstStageMask        = VK_PIPELINE_STAGE_2_NONE;
    toPresent.dstAccessMask       = VK_ACCESS_2_NONE;
    toPresent.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image               = wd.swapChain.images[wd.imageIndex];
    toPresent.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo dep{};
    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &toPresent;
    vkCmdPipelineBarrier2(frame.commandBuffer, &dep);

    vkEndCommandBuffer(frame.commandBuffer);

    SubmitAndPresent(wd);

    wd.frameIndex = (wd.frameIndex + 1) % static_cast<uint32_t>(_framesInFlight);
    wd.frameStarted = false;
}

// ─── AcquireNextImage ─────────────────────────────────────────────────────────

bool SDL3VulkanBackend::AcquireNextImage(WindowData& wd)
{
    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];
    VkResult result = vkAcquireNextImageKHR(
        _device, wd.swapChain.handle, UINT64_MAX,
        frame.imageAvailableSemaphore, VK_NULL_HANDLE, &wd.imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain(wd);
        return false;
    }
    // VK_SUBOPTIMAL_KHR: continue this frame but flag resize for next frame.
    if (result == VK_SUBOPTIMAL_KHR) {
        wd.needsResize = true;
    }
    return (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
}

// ─── SubmitAndPresent ─────────────────────────────────────────────────────────

void SDL3VulkanBackend::SubmitAndPresent(WindowData& wd)
{
    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &frame.imageAvailableSemaphore;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &frame.commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &frame.renderFinishedSemaphore;

    vkQueueSubmit(_graphicsQueue, 1, &submit, frame.inFlightFence);

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &frame.renderFinishedSemaphore;
    present.swapchainCount     = 1;
    present.pSwapchains        = &wd.swapChain.handle;
    present.pImageIndices      = &wd.imageIndex;

    VkResult result = vkQueuePresentKHR(_presentQueue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        wd.needsResize = true;
    }
}

// ─── RecreateSwapChain ────────────────────────────────────────────────────────

void SDL3VulkanBackend::RecreateSwapChain(WindowData& wd)
{
    // Fence waits in BeginFrame ensure the GPU has finished the previous frame
    // in this slot before we recreate. vkDeviceWaitIdle is NOT called here.
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);
    if (w == 0 || h == 0) return; // still minimized

    SwapChainDesc desc{};
    desc.physicalDevice = _physicalDevice;
    desc.device         = _device;
    desc.surface        = wd.surface;
    desc.vsyncMode      = wd.config.VSync;
    desc.hdrOutput      = wd.config.HDROutput;
    desc.graphicsFamily = _graphicsFamily;
    desc.presentFamily  = _presentFamily;
    desc.drawableWidth  = w;
    desc.drawableHeight = h;
    desc.minImageCount  = static_cast<uint32_t>(_framesInFlight) + 1;
    desc.oldSwapchain   = wd.swapChain.handle; // driver reuses physical memory
    desc.setObjectName  = [this](VkObjectType type, uint64_t h2, const char* name) {
        NameObject(type, h2, name);
    };

    SwapChain newSC;
    if (!newSC.Create(desc)) return; // recreation failed; keep old swap chain

    // The per-slot fence waited on in BeginFrame only guarantees that slot's
    // own work is done — a present from the *other* in-flight slot can still
    // be outstanding on the queue. Bound (not unbounded) wait on the affected
    // queues only, scoped to this occasional resize event — not called per-frame,
    // so it does not violate the "no per-frame stall" invariant.
    vkQueueWaitIdle(_graphicsQueue);
    if (_presentQueue != _graphicsQueue) vkQueueWaitIdle(_presentQueue);

    // Destroy old image views (swap chain images are destroyed by driver with oldSwapchain).
    for (auto view : wd.swapChain.imageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(_device, view, nullptr);
    }
    // Destroy the old swap chain object.
    if (wd.swapChain.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, wd.swapChain.handle, nullptr);
    }

    wd.swapChain = std::move(newSC);
    // Reset image index — it will be re-acquired next BeginFrame.
    wd.imageIndex = 0;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void SDL3VulkanBackend::Shutdown()
{
    if (!_initialised && _device == VK_NULL_HANDLE) return;

    if (_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device);
    }

    // Destroy secondary windows.
    for (auto& [_, wd] : _secondaryWindows) {
        for (auto& f : wd.frames) {
            if (f.commandBuffer != VK_NULL_HANDLE)
                vkFreeCommandBuffers(_device, f.commandPool, 1, &f.commandBuffer);
            if (f.commandPool            != VK_NULL_HANDLE) vkDestroyCommandPool(_device, f.commandPool, nullptr);
            if (f.imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
            if (f.renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);
            if (f.inFlightFence           != VK_NULL_HANDLE) vkDestroyFence(_device, f.inFlightFence, nullptr);
        }
        wd.swapChain.Destroy(_device);
        if (wd.surface    != VK_NULL_HANDLE) vkDestroySurfaceKHR(_instance, wd.surface, nullptr);
        if (wd.sdlWindow)                    SDL_DestroyWindow(wd.sdlWindow);
    }
    _secondaryWindows.clear();
    _sdlIdToHandle.clear();

    // ImGui shutdown.
    if (_device != VK_NULL_HANDLE && _instance != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    // Destroy primary window resources.
    for (auto& f : _primary.frames) {
        if (f.commandBuffer != VK_NULL_HANDLE)
            vkFreeCommandBuffers(_device, f.commandPool, 1, &f.commandBuffer);
        if (f.commandPool            != VK_NULL_HANDLE) vkDestroyCommandPool(_device, f.commandPool, nullptr);
        if (f.imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
        if (f.renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);
        if (f.inFlightFence           != VK_NULL_HANDLE) vkDestroyFence(_device, f.inFlightFence, nullptr);
    }
    _primary.frames.clear();
    _primary.swapChain.Destroy(_device);
    if (_primary.surface   != VK_NULL_HANDLE) vkDestroySurfaceKHR(_instance, _primary.surface, nullptr);
    _primary.surface = VK_NULL_HANDLE;
    if (_primary.sdlWindow) { SDL_DestroyWindow(_primary.sdlWindow); _primary.sdlWindow = nullptr; }

    // Viewport command pool and descriptor pool.
    if (_viewportCmdPool   != VK_NULL_HANDLE) vkDestroyCommandPool(_device, _viewportCmdPool, nullptr);
    if (_descriptorPool    != VK_NULL_HANDLE) vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);

    // VMA, device, debug messenger, instance, SDL.
    if (_vmaAllocator      != VK_NULL_HANDLE) vmaDestroyAllocator(_vmaAllocator);
    if (_device            != VK_NULL_HANDLE) vkDestroyDevice(_device, nullptr);
    if (_debugMessenger    != VK_NULL_HANDLE && _pfnDestroyDebugMsgr) {
        _pfnDestroyDebugMsgr(_instance, _debugMessenger, nullptr);
    }
    if (_instance          != VK_NULL_HANDLE) vkDestroyInstance(_instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    // Reset all handles to sentinel values.
    _instance          = VK_NULL_HANDLE;
    _debugMessenger    = VK_NULL_HANDLE;
    _physicalDevice    = VK_NULL_HANDLE;
    _device            = VK_NULL_HANDLE;
    _vmaAllocator      = VK_NULL_HANDLE;
    _descriptorPool    = VK_NULL_HANDLE;
    _viewportCmdPool   = VK_NULL_HANDLE;
    _graphicsQueue     = VK_NULL_HANDLE;
    _presentQueue      = VK_NULL_HANDLE;
    _initialised       = false;
    _shouldClose       = false;
    _lastPerfCount     = 0;
    _inputQueue.clear();
    _drainBuffer.clear();
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* SDL3VulkanBackend::NativeHandle() const
{
    return _primary.sdlWindow;
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void SDL3VulkanBackend::CancelClose() noexcept
{
    _shouldClose = false;
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float SDL3VulkanBackend::WindowDpiScale(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return 1.0f;
    return SDL_GetWindowDisplayScale(win);
}

// ─── WindowSize ───────────────────────────────────────────────────────────────

WindowExtent SDL3VulkanBackend::WindowSize(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return {};
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    return WindowExtent{ w, h };
}

// ─── WindowIsMinimized ────────────────────────────────────────────────────────

bool SDL3VulkanBackend::WindowIsMinimized(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return false;
    return (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) != 0;
}

// ─── WindowIsFocused ──────────────────────────────────────────────────────────

bool SDL3VulkanBackend::WindowIsFocused(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return false;
    return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

// ─── CreateWindow ─────────────────────────────────────────────────────────────

WindowHandle SDL3VulkanBackend::CreateWindow(const WindowConfig& config)
{
    IMF_ASSERT(_initialised);

    WindowData wd{};
    wd.config = config;

    SDL_WindowFlags wflags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    wd.sdlWindow = SDL_CreateWindow(
        std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!wd.sdlWindow) return PrimaryWindow;

    if (!SDL_Vulkan_CreateSurface(wd.sdlWindow, _instance, nullptr, &wd.surface)) {
        SDL_DestroyWindow(wd.sdlWindow);
        return PrimaryWindow;
    }

    if (!CreateSwapChainFor(wd) || !AllocateFrameResources(wd)) {
        wd.swapChain.Destroy(_device);
        if (wd.surface   != VK_NULL_HANDLE) vkDestroySurfaceKHR(_instance, wd.surface, nullptr);
        SDL_DestroyWindow(wd.sdlWindow);
        return PrimaryWindow;
    }

    WindowHandle wh = _nextHandle++;
    SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
    _sdlIdToHandle[sdlId] = wh;

    _inputQueue.push_back(WindowResizeEvent{ wh, config.Width, config.Height });
    _secondaryWindows[wh] = std::move(wd);
    return wh;
}

// ─── DestroyWindow ────────────────────────────────────────────────────────────

void SDL3VulkanBackend::DestroyWindow(WindowHandle handle)
{
    if (handle == PrimaryWindow) return;

    auto it = _secondaryWindows.find(handle);
    if (it == _secondaryWindows.end()) return;

    // Full idle wait — secondary destruction is infrequent.
    vkDeviceWaitIdle(_device);

    WindowData& wd = it->second;
    for (auto& f : wd.frames) {
        if (f.commandBuffer != VK_NULL_HANDLE)
            vkFreeCommandBuffers(_device, f.commandPool, 1, &f.commandBuffer);
        if (f.commandPool            != VK_NULL_HANDLE) vkDestroyCommandPool(_device, f.commandPool, nullptr);
        if (f.imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
        if (f.renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);
        if (f.inFlightFence           != VK_NULL_HANDLE) vkDestroyFence(_device, f.inFlightFence, nullptr);
    }
    wd.swapChain.Destroy(_device);
    if (wd.surface   != VK_NULL_HANDLE) vkDestroySurfaceKHR(_instance, wd.surface, nullptr);
    if (wd.sdlWindow) {
        SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
        _sdlIdToHandle.erase(sdlId);
        SDL_DestroyWindow(wd.sdlWindow);
    }

    _secondaryWindows.erase(it);
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> SDL3VulkanBackend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext SDL3VulkanBackend::GetNativeGraphicsContext() const
{
    return VulkanContext{
        .Instance            = _instance,
        .PhysicalDevice      = _physicalDevice,
        .Device              = _device,
        .GraphicsQueueFamily = _graphicsFamily,
        .GraphicsQueue       = _graphicsQueue,
        .ViewportCommandPool = _viewportCmdPool,
        .DescriptorPool      = _descriptorPool,
        .SwapchainImageFormat = static_cast<uint32_t>(_primary.swapChain.format),
    };
}

// ─── NameObject ───────────────────────────────────────────────────────────────

void SDL3VulkanBackend::NameObject(VkObjectType type, uint64_t object, const char* name) const
{
    if (!_pfnSetObjectName || !_device || object == 0) return;
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType   = type;
    info.objectHandle = object;
    info.pObjectName  = name;
    _pfnSetObjectName(_device, &info);
}

// ─── FindWindow ───────────────────────────────────────────────────────────────

SDL3VulkanBackend::WindowData* SDL3VulkanBackend::FindWindow(WindowHandle handle)
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

const SDL3VulkanBackend::WindowData* SDL3VulkanBackend::FindWindow(WindowHandle handle) const
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

WindowHandle SDL3VulkanBackend::HandleForSDLWindow(SDL_WindowID id) const
{
    auto it = _sdlIdToHandle.find(id);
    return (it != _sdlIdToHandle.end()) ? it->second : PrimaryWindow;
}

// ─── DebugCallback ────────────────────────────────────────────────────────────

VKAPI_ATTR VkBool32 VKAPI_CALL SDL3VulkanBackend::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT        /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userdata*/)
{
    if (!data || !data->pMessage) return VK_FALSE;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::fprintf(stderr, "[ImFrame][Vulkan][ERROR] %s\n", data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[ImFrame][Vulkan][WARN]  %s\n", data->pMessage);
    }
    return VK_FALSE;
}

} // namespace ImFrame::Internal
