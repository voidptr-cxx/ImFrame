cmake_minimum_required(VERSION 3.25)

if(EMSCRIPTEN)
    # ─── Emscripten (browser) WebGPU backend static library ───────────────────
    # UNVERIFIED — written and reviewed against emsdk 6.0.0's real headers and
    # the emdawnwebgpu port, but never built. No wasm32-emscripten vcpkg
    # triplet has been stood up for ImFrame's other dependencies
    # (freetype/implot/catch2/imgui) this phase — see DECISIONS.md and
    # PHASE_STATUS.md for the explicit scoping decision. This target is
    # provided so a future session has the CMake wiring ready to attempt.
    add_library(ImFrame_EmscriptenWebGPU STATIC
        EmscriptenWebGPUBackend.cpp
        WebGPUSurface.cpp
        WebGPUDeviceSetup.cpp
        Platform/SurfaceEmscripten.cpp
        Platform/InputTranslationEmscripten.cpp
    )

    target_include_directories(ImFrame_EmscriptenWebGPU
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # emdawnwebgpu requires linking with em++ (C++), not plain emcc — see
    # emdawnwebgpu.port.py's linker_setup(): "emdawnwebgpu requires C++."
    target_link_options(ImFrame_EmscriptenWebGPU PUBLIC
        "--use-port=emdawnwebgpu"
    )
    target_compile_options(ImFrame_EmscriptenWebGPU PUBLIC
        "--use-port=emdawnwebgpu"
    )

    target_link_libraries(ImFrame_EmscriptenWebGPU
        PUBLIC  ImFrame
        PRIVATE imgui::imgui
                imframe_warnings
    )

    target_compile_features(ImFrame_EmscriptenWebGPU PRIVATE cxx_std_23)
    return()
endif()

# ─── SDL3 + Dawn WebGPU backend static library (native) ───────────────────────
add_library(ImFrame_DawnWebGPU STATIC
    DawnWebGPUBackend.cpp
    WebGPUSurface.cpp
    WebGPUDeviceSetup.cpp
    NativeSurface.cpp
    ViewportWebGPU.cpp
    # Reused directly from Phase 20 — SDL3's event model is platform-agnostic,
    # so the same translation unit compiles unchanged here (mirrors Phase
    # 21/22's reuse of the same file).
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL3Vulkan/InputTranslation.cpp
)

target_include_directories(ImFrame_DawnWebGPU
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ImFrame_DawnWebGPU
    # SDL3 and Dawn are PUBLIC because DawnWebGPUBackend.hpp and WebGPUSurface.hpp
    # store WGPUDevice/WGPUQueue/WGPUSurface/SDL_Window* by value — consumers
    # including these headers need the same headers/libs (mirrors the
    # Phase 20-22 PUBLIC-linkage decision).
    PUBLIC  ImFrame                # ImFrame public headers + cxx_std_23
            SDL3::SDL3
            dawn::webgpu_dawn
    PRIVATE imgui::imgui
            imframe_warnings
)

target_compile_features(ImFrame_DawnWebGPU PRIVATE cxx_std_23)
