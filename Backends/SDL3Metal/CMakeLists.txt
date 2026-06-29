cmake_minimum_required(VERSION 3.25)

# Metal backend sources are Objective-C++ (.mm) — must be enabled before this
# target is declared. Only reached when IMF_BUILD_METAL_BACKEND is ON and the
# platform is Apple (root CMakeLists.txt guards add_subdirectory() with APPLE).
enable_language(OBJCXX)

# ─── System frameworks (no vcpkg package — Metal/QuartzCore/AppKit ship with the macOS SDK) ──
find_library(METAL_FRAMEWORK      Metal)
find_library(QUARTZCORE_FRAMEWORK QuartzCore)
find_library(APPKIT_FRAMEWORK     AppKit)

if(NOT METAL_FRAMEWORK OR NOT QUARTZCORE_FRAMEWORK OR NOT APPKIT_FRAMEWORK)
    message(FATAL_ERROR "ImFrame_SDL3Metal requires the Metal, QuartzCore, and AppKit frameworks (macOS only)")
endif()

# ─── SDL3 + Metal backend static library ──────────────────────────────────────
add_library(ImFrame_SDL3Metal STATIC
    SDL3MetalBackend.mm
    MetalLayer.mm
    ViewportMetal.mm
    # Reused directly from Phase 20 rather than copied — SDL3's event model is
    # platform-agnostic, so the same translation unit compiles unchanged here.
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL3Vulkan/InputTranslation.cpp
)

# ARC throughout (Phase 21 invariant) — CMake's OBJCXX support does not enable
# ARC by default, so it must be requested explicitly per .mm file.
set_source_files_properties(
    SDL3MetalBackend.mm
    MetalLayer.mm
    ViewportMetal.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc"
)

# Expose own source directory so consumers can:
#   #include "SDL3MetalBackend.hpp"
target_include_directories(ImFrame_SDL3Metal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ImFrame_SDL3Metal
    # SDL3, Metal, and QuartzCore are PUBLIC because SDL3MetalBackend.hpp and
    # MetalLayer.hpp store id<MTLDevice>/id<MTLCommandQueue>/CAMetalLayer* by
    # value — consumers including these headers need the same frameworks
    # (mirrors the Phase 20 Vulkan backend's PUBLIC-linkage decision).
    PUBLIC  ImFrame                # ImFrame public headers + cxx_std_23
            SDL3::SDL3
            ${METAL_FRAMEWORK}
            ${QUARTZCORE_FRAMEWORK}
    PRIVATE ${APPKIT_FRAMEWORK}     # NSScreen — confined to MetalLayer.mm's HDR capability check
            imgui::imgui
            imframe_warnings
)

target_compile_features(ImFrame_SDL3Metal PRIVATE cxx_std_23)
