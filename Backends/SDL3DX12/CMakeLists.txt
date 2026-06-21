cmake_minimum_required(VERSION 3.25)

# ─── SDL3 + DirectX 12 backend static library ─────────────────────────────────
add_library(ImFrame_SDL3DX12 STATIC
    SDL3DX12Backend.cpp
    DX12SwapChain.cpp
    FrameResources.cpp
    DescriptorAllocator.cpp
    # Reused directly from Phase 20 rather than copied — SDL3's event model is
    # platform-agnostic, so the same translation unit compiles unchanged here
    # (mirrors Phase 21's reuse of the same file for the Metal backend).
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL3Vulkan/InputTranslation.cpp
)

target_include_directories(ImFrame_SDL3DX12
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# d3d12.lib/dxgi.lib/dxguid.lib/shcore.lib ship with the Windows SDK, not
# vcpkg — find_library() does not reliably locate them at configure time
# under the MSVC generator, so they're linked by literal name instead and
# resolved by the linker's default Windows SDK library search path (the same
# convention used by Microsoft's own DirectX-Graphics-Samples CMake builds).
target_link_libraries(ImFrame_SDL3DX12
    # SDL3 and the D3D12/DXGI system libs are PUBLIC because SDL3DX12Backend.hpp
    # and its sibling headers store ID3D12Device4*/ID3D12CommandQueue*/HWND/etc.
    # by value — consumers including these headers need the same headers/libs
    # (mirrors the Phase 20/21 PUBLIC-linkage decision).
    PUBLIC  ImFrame                # ImFrame public headers + cxx_std_23
            SDL3::SDL3
            d3d12.lib
            dxgi.lib
            dxguid.lib
    PRIVATE shcore.lib              # DPI awareness fallback — confined to SDL3DX12Backend.cpp
            user32.lib              # SetProcessDpiAwarenessContext
            imgui::imgui
            imframe_warnings
)

target_compile_features(ImFrame_SDL3DX12 PRIVATE cxx_std_23)
