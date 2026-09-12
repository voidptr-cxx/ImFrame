cmake_minimum_required(VERSION 3.25)

# ─── SDL3 + Vulkan 1.3 backend static library ─────────────────────────────────
add_library(ImFrame_SDL3Vulkan STATIC
    SDL3VulkanBackend.cpp
    SwapChain.cpp
    InputTranslation.cpp
    ViewportVulkan.cpp
)

# ── Phase 35.1: NativeRendererVulkan (opt-in, same IMF_BUILD_NATIVE_RENDERER gate and
# Backends/-not-src/Rendering/Renderers/Backends/ placement reasoning as NativeRendererGL3 —
# see .claude/DECISIONS.md, Phase 32.4 and Phase 35.1) ──
if(IMF_BUILD_NATIVE_RENDERER)
    target_sources(ImFrame_SDL3Vulkan PRIVATE NativeRendererVulkan.cpp)
    # NativeRendererVulkan.cpp reaches into src/Rendering/Renderers/ (BatchBuilder.hpp,
    # IRenderer.hpp) — an internal ImFrame header directory, not part of ImFrame's own PUBLIC
    # include interface. It also consumes Shaders/SDFRect.{vert,frag}.hpp's embedded SPIR-V
    # (Phase 32.2's cmake/CompileShader.cmake pipeline) — the first real consumer of that output.
    target_include_directories(ImFrame_SDL3Vulkan PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(ImFrame_SDL3Vulkan PRIVATE ImFrame_Shaders)
endif()

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
