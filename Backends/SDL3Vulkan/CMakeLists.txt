cmake_minimum_required(VERSION 3.25)

# ─── SDL3 + Vulkan 1.3 backend static library ─────────────────────────────────
add_library(ImFrame_SDL3Vulkan STATIC
    SDL3VulkanBackend.cpp
    SwapChain.cpp
    InputTranslation.cpp
    ViewportVulkan.cpp
)

# Expose own source directory so consumers can:
#   #include "SDL3VulkanBackend.hpp"
target_include_directories(ImFrame_SDL3Vulkan
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ImFrame_SDL3Vulkan
    # SDL3, Vulkan, and VMA are PUBLIC because SDL3VulkanBackend.hpp stores
    # VkInstance/VkDevice/VmaAllocator/SDL_Window* by value — consumers
    # including the header need these include paths too.
    PUBLIC  ImFrame                                    # ImFrame public headers + cxx_std_23
            SDL3::SDL3
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
    PRIVATE imgui::imgui
            imframe_warnings
)

target_compile_features(ImFrame_SDL3Vulkan PRIVATE cxx_std_23)

# Tell VMA to pull in Vulkan 1.3 headers.
target_compile_definitions(ImFrame_SDL3Vulkan PRIVATE
    VMA_VULKAN_VERSION=1003000
)
