cmake_minimum_required(VERSION 3.25)

find_package(Catch2 3 REQUIRED)
include(CTest)
include(Catch)

# ─── Backend tests ────────────────────────────────────────────────────────────
if(IMF_BACKEND_GLFW_OPENGL3)
    add_executable(ImFrame_Tests_Backends
        Backends/GLFWOpenGL3_test.cpp
        Backends/OpenGL3Conformance_test.cpp
    )

    target_include_directories(ImFrame_Tests_Backends PRIVATE
        ${CMAKE_SOURCE_DIR}
    )

    target_link_libraries(ImFrame_Tests_Backends
        PRIVATE
            ImFrame
            ImFrame_GLFWOpenGL3
            Catch2::Catch2WithMain
    )

    target_compile_features(ImFrame_Tests_Backends PRIVATE cxx_std_23)

    catch_discover_tests(ImFrame_Tests_Backends
        PROPERTIES LABELS unit
    )
endif()

# ─── Phase 19: InputEvent / Headless / MultiWindow tests ─────────────────────
# These three targets use HeadlessBackend (compiled into ImFrame core).
# ${CMAKE_SOURCE_DIR} is added as PRIVATE include so the test TUs can reach
# "Backends/Headless/HeadlessBackend.hpp" directly.

add_executable(ImFrame_Tests_Backends_InputEvent
    Backends/InputEvent_test.cpp
)
target_include_directories(ImFrame_Tests_Backends_InputEvent PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Backends_InputEvent
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Backends_InputEvent PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Backends_InputEvent PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Backends_InputEvent PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Backends_Headless
    Backends/HeadlessBackend_test.cpp
)
target_include_directories(ImFrame_Tests_Backends_Headless PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Backends_Headless
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Backends_Headless PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Backends_Headless PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Backends_MultiWindow
    Backends/MultiWindow_test.cpp
)
target_include_directories(ImFrame_Tests_Backends_MultiWindow PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Backends_MultiWindow
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Backends_MultiWindow PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Backends_MultiWindow PROPERTIES LABELS unit)

# ─── Utility tests ────────────────────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_File
    Utility/File_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_File
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_File PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_File PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Dir
    Utility/Directory_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Dir
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Dir PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Dir PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Watch
    Utility/FileWatcher_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Watch
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Watch PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Watch PROPERTIES LABELS unit)

# ─── Phase 4: Concurrency tests ───────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Thread
    Utility/Thread_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Thread
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Thread PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Thread PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Pool
    Utility/ThreadPool_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Pool
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Pool PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Pool PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Worker
    Utility/BackgroundWorker_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Worker
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Worker PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Worker PROPERTIES LABELS "unit;tsan")

# ─── Phase 5: Event system tests ──────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Delegate
    Utility/Delegate_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Delegate
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Delegate PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Delegate PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Signal
    Utility/Signal_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Signal
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Signal PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Signal PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_EventBus
    Utility/EventBus_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_EventBus
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_EventBus PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_EventBus PROPERTIES LABELS "unit;tsan")

# ─── Phase 6: Supporting utility tests ───────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Timer
    Utility/Timer_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Timer
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Timer PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Timer PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Logger
    Utility/Logger_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Logger
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Logger PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Logger PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Config
    Utility/Config_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Config
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Config PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Config PROPERTIES LABELS unit)

# ─── Phase 7: App tests ───────────────────────────────────────────────────────

add_executable(ImFrame_Tests_App_Application
    App/Application_test.cpp
)
target_include_directories(ImFrame_Tests_App_Application PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_App_Application
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_App_Application PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_App_Application PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_App_DockSpace
    App/DockSpace_test.cpp
)
target_link_libraries(ImFrame_Tests_App_DockSpace
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_App_DockSpace PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_App_DockSpace PROPERTIES LABELS unit)

# ─── Phase 8: Theme tests ─────────────────────────────────────────────────────

add_executable(ImFrame_Tests_Theme_ColorToken
    Theme/ColorToken_test.cpp
)
target_link_libraries(ImFrame_Tests_Theme_ColorToken
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Theme_ColorToken PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Theme_ColorToken PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Theme_Theme
    Theme/Theme_test.cpp
)
target_link_libraries(ImFrame_Tests_Theme_Theme
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Theme_Theme PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Theme_Theme PROPERTIES LABELS unit)

# ─── Phase 9: Icon font tests ─────────────────────────────────────────────────

add_executable(ImFrame_Tests_Icons_IconFont
    Icons/IconFont_test.cpp
)
target_link_libraries(ImFrame_Tests_Icons_IconFont
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Icons_IconFont PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Icons_IconFont PROPERTIES LABELS unit)

# Copy the vendored FA6 solid font next to the test binary so FindSolidFont()
# resolves "Assets/Fonts/fa-solid-900.ttf" relative to the working directory.
add_custom_command(TARGET ImFrame_Tests_Icons_IconFont POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ImFrame_Tests_Icons_IconFont>/Assets/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/fa-solid-900.ttf"
        "$<TARGET_FILE_DIR:ImFrame_Tests_Icons_IconFont>/Assets/Fonts/fa-solid-900.ttf"
    COMMENT "Copying FA6 solid font to test output directory"
)

# ─── Phase 12: Animation tests ────────────────────────────────────────────────
# Pure arithmetic — no ImGui context required.

foreach(_anim
    Tween
    AnimatedValue
    Sequence
)
    add_executable("ImFrame_Tests_Anim_${_anim}"
        "Anim/${_anim}_test.cpp"
    )
    target_link_libraries("ImFrame_Tests_Anim_${_anim}"
        PRIVATE ImFrame Catch2::Catch2WithMain
    )
    target_compile_features("ImFrame_Tests_Anim_${_anim}" PRIVATE cxx_std_23)
    catch_discover_tests("ImFrame_Tests_Anim_${_anim}"
        PROPERTIES LABELS unit
    )
endforeach()

# ─── Phase 13: Overlay tests ──────────────────────────────────────────────────

add_executable(ImFrame_Tests_Overlay_Toast
    Overlay/Toast_test.cpp
)
target_link_libraries(ImFrame_Tests_Overlay_Toast
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Overlay_Toast PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Overlay_Toast PROPERTIES LABELS unit)

# ─── Phase 15: Plot widget tests ──────────────────────────────────────────────

add_executable(ImFrame_Tests_Widgets_LinePlot
    Widgets/LinePlot_test.cpp
)
target_link_libraries(ImFrame_Tests_Widgets_LinePlot
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Widgets_LinePlot PRIVATE cxx_std_23)
# TIMEOUT=30 guards against ImPlot hangs in headless context.
catch_discover_tests(ImFrame_Tests_Widgets_LinePlot PROPERTIES LABELS unit TIMEOUT 30)

# ─── Phase 20: SDL3 Vulkan backend tests ─────────────────────────────────────
# VulkanInput_test: pure InputTranslation unit tests — no GPU required.
# VulkanBackend_test: full backend integration — requires Vulkan-capable GPU.

if(IMF_BACKEND_SDL3_VULKAN)
    add_executable(ImFrame_Tests_Backends_VulkanInput
        Backends/VulkanInput_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_VulkanInput PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_VulkanInput
        PRIVATE
            ImFrame
            ImFrame_SDL3Vulkan
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_VulkanInput PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_VulkanInput PROPERTIES LABELS unit)

    add_executable(ImFrame_Tests_Backends_VulkanBackend
        Backends/VulkanBackend_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_VulkanBackend PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_VulkanBackend
        PRIVATE
            ImFrame
            ImFrame_SDL3Vulkan
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_VulkanBackend PRIVATE cxx_std_23)
    # TIMEOUT guards against hangs if no Vulkan-capable GPU/driver is present in CI.
    catch_discover_tests(ImFrame_Tests_Backends_VulkanBackend PROPERTIES LABELS vulkan TIMEOUT 60)

    add_executable(ImFrame_Tests_Backends_VulkanConformance
        Backends/VulkanConformance_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_VulkanConformance PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_VulkanConformance
        PRIVATE
            ImFrame
            ImFrame_SDL3Vulkan
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_VulkanConformance PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_VulkanConformance PROPERTIES LABELS vulkan TIMEOUT 60)
endif()

# ─── Phase 22: SDL3 DX12 backend tests ───────────────────────────────────────
# DX12Input_test: pure InputTranslation unit tests — no GPU required.
# DX12Backend_test / DX12Conformance_test: full backend integration — requires
# a D3D12-capable GPU (or WARP). DX12Leak_test: debug-only, registers its own
# ID3D12InfoQueue1 callback to assert no D3D12 errors during a full cycle.

if(IMF_BUILD_DX12_BACKEND)
    add_executable(ImFrame_Tests_Backends_DX12Input
        Backends/DX12Input_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_DX12Input PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_DX12Input
        PRIVATE
            ImFrame
            ImFrame_SDL3Vulkan # InputTranslation.hpp/.cpp is reused directly from Phase 20
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_DX12Input PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_DX12Input PROPERTIES LABELS unit)

    add_executable(ImFrame_Tests_Backends_DX12Backend
        Backends/DX12Backend_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_DX12Backend PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_DX12Backend
        PRIVATE
            ImFrame
            ImFrame_SDL3DX12
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_DX12Backend PRIVATE cxx_std_23)
    # TIMEOUT guards against hangs if no D3D12-capable GPU/driver is present in CI.
    catch_discover_tests(ImFrame_Tests_Backends_DX12Backend PROPERTIES LABELS dx12 TIMEOUT 60)

    add_executable(ImFrame_Tests_Backends_DX12Conformance
        Backends/DX12Conformance_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_DX12Conformance PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_DX12Conformance
        PRIVATE
            ImFrame
            ImFrame_SDL3DX12
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_DX12Conformance PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_DX12Conformance PROPERTIES LABELS dx12 TIMEOUT 60)

    add_executable(ImFrame_Tests_Backends_DX12Leak
        Backends/DX12Leak_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_DX12Leak PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_DX12Leak
        PRIVATE
            ImFrame
            ImFrame_SDL3DX12
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_DX12Leak PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_DX12Leak PROPERTIES LABELS dx12 TIMEOUT 60)
endif()

# ─── Phase 23: SDL3 Dawn WebGPU backend tests ────────────────────────────────
# WebGPUInput_test: pure InputTranslation unit tests — no GPU required.
# WebGPUBackend_test / WebGPUConformance_test: full backend integration —
# requires a WebGPU-capable GPU (Dawn translates to D3D12/Metal/Vulkan).

if(IMF_BUILD_WEBGPU_BACKEND)
    add_executable(ImFrame_Tests_Backends_WebGPUInput
        Backends/WebGPUInput_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_WebGPUInput PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_WebGPUInput
        PRIVATE
            ImFrame
            ImFrame_SDL3Vulkan # InputTranslation.hpp/.cpp is reused directly from Phase 20
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_WebGPUInput PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_WebGPUInput PROPERTIES LABELS unit)

    add_executable(ImFrame_Tests_Backends_WebGPUBackend
        Backends/WebGPUBackend_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_WebGPUBackend PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_WebGPUBackend
        PRIVATE
            ImFrame
            ImFrame_DawnWebGPU
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_WebGPUBackend PRIVATE cxx_std_23)
    # TIMEOUT guards against hangs if no WebGPU-capable GPU/driver is present in CI.
    catch_discover_tests(ImFrame_Tests_Backends_WebGPUBackend PROPERTIES LABELS webgpu TIMEOUT 60)

    add_executable(ImFrame_Tests_Backends_WebGPUConformance
        Backends/WebGPUConformance_test.cpp
    )
    target_include_directories(ImFrame_Tests_Backends_WebGPUConformance PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(ImFrame_Tests_Backends_WebGPUConformance
        PRIVATE
            ImFrame
            ImFrame_DawnWebGPU
            imgui::imgui
            Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Backends_WebGPUConformance PRIVATE cxx_std_23)
    catch_discover_tests(ImFrame_Tests_Backends_WebGPUConformance PROPERTIES LABELS webgpu TIMEOUT 60)

    if(WIN32 AND NOT EMSCRIPTEN)
        # Dawn's D3D12 backend probes dxil.dll at device-init time (EnsureDXCLibraries, called
        # before any shader compilation). The DLL ships with the directx-dxc vcpkg package but
        # is NOT in System32, so it must be deployed beside each test executable.
        # dxcompiler.dll is copied too — it is loaded at shader-compile time.
        set(_dxc_dir_debug   "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
        set(_dxc_dir_release "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
        foreach(_tgt
            ImFrame_Tests_Backends_WebGPUBackend
            ImFrame_Tests_Backends_WebGPUConformance)
            add_custom_command(TARGET ${_tgt} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<IF:$<CONFIG:Debug>,${_dxc_dir_debug},${_dxc_dir_release}>/dxil.dll"
                    "$<IF:$<CONFIG:Debug>,${_dxc_dir_debug},${_dxc_dir_release}>/dxcompiler.dll"
                    "$<TARGET_FILE_DIR:${_tgt}>"
                COMMENT "Deploying DXC DLLs beside ${_tgt}"
            )
        endforeach()
    endif()
endif()

# ─── Phase 17: DevTools tests (compiled only when IMF_DEV_TOOLS=ON) ──────────

if(IMF_DEV_TOOLS)
    add_executable(ImFrame_Tests_DevTools_LogViewer
        DevTools/LogViewer_test.cpp
    )
    target_link_libraries(ImFrame_Tests_DevTools_LogViewer
        PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_DevTools_LogViewer PRIVATE cxx_std_23)
    target_compile_definitions(ImFrame_Tests_DevTools_LogViewer PRIVATE IMF_DEV_TOOLS)
    catch_discover_tests(ImFrame_Tests_DevTools_LogViewer PROPERTIES LABELS unit)

    add_executable(ImFrame_Tests_DevTools_ThemeHotReload
        DevTools/ThemeHotReload_test.cpp
    )
    target_link_libraries(ImFrame_Tests_DevTools_ThemeHotReload
        PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_DevTools_ThemeHotReload PRIVATE cxx_std_23)
    target_compile_definitions(ImFrame_Tests_DevTools_ThemeHotReload PRIVATE IMF_DEV_TOOLS)
    catch_discover_tests(ImFrame_Tests_DevTools_ThemeHotReload PROPERTIES LABELS unit TIMEOUT 10)
endif()

# ─── Phase 24: Viewport tests ─────────────────────────────────────────────────
# Both targets use HeadlessBackend (compiled into ImFrame core) via a
# FrameLimitedHeadlessBackend subclass defined inline in each test file.
# ${CMAKE_SOURCE_DIR} is added as PRIVATE include so test TUs can reach
# "Backends/Headless/HeadlessBackend.hpp" directly.

add_executable(ImFrame_Tests_Rendering_Viewport
    Rendering/Viewport_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_Viewport PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Rendering_Viewport
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_Viewport PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_Viewport PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_Viewport PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Rendering_ViewportConformance
    Rendering/ViewportConformance_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_ViewportConformance PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Rendering_ViewportConformance
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_ViewportConformance PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_ViewportConformance PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_ViewportConformance PROPERTIES LABELS unit)

# ─── Phase 25: Canvas2D and Camera2D tests ────────────────────────────────────
# Camera2D_test: pure math — no ImGui context required, no backend needed.
# Canvas2D_test: uses FrameLimitedHeadlessBackend for finite headless runs.
# Both need ${CMAKE_SOURCE_DIR} as PRIVATE include so test TUs can reach
# "Backends/Headless/HeadlessBackend.hpp" directly.

add_executable(ImFrame_Tests_Rendering_Camera2D
    Rendering/Camera2D_test.cpp
)
target_link_libraries(ImFrame_Tests_Rendering_Camera2D
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_Camera2D PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_Camera2D PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_Camera2D PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Rendering_CommandBuffer
    Rendering/CommandBuffer_test.cpp
)
target_link_libraries(ImFrame_Tests_Rendering_CommandBuffer
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_CommandBuffer PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_CommandBuffer PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_CommandBuffer PROPERTIES LABELS unit)

# ─── Phase 32.1: NativeRenderer core types ───────────────────────────────────
# BatchBuilder/TextureAtlas/ClipStack are pure CPU-side data structures with no
# GPU or ImGui dependency — unlike ImGuiCompatRenderer_test below, they need no
# backend and are not gated behind IMF_BACKEND_GLFW_OPENGL3.
add_executable(ImFrame_Tests_Rendering_BatchBuilder
    Rendering/BatchBuilder_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_BatchBuilder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Rendering_BatchBuilder
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_BatchBuilder PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_BatchBuilder PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_BatchBuilder PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Rendering_TextureAtlas
    Rendering/TextureAtlas_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_TextureAtlas PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Rendering_TextureAtlas
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_TextureAtlas PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_TextureAtlas PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_TextureAtlas PROPERTIES LABELS unit)

# ─── Phase 33.2: FontRegistry/FontFace tests ──────────────────────────────────────────────────
if(IMF_BUILD_TEXT_MSDF)
    add_executable(ImFrame_Tests_Rendering_FontRegistry
        Rendering/FontRegistry_test.cpp
    )
    target_include_directories(ImFrame_Tests_Rendering_FontRegistry PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ImFrame_Tests_Rendering_FontRegistry
        PRIVATE ImFrame Freetype::Freetype harfbuzz::harfbuzz Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Rendering_FontRegistry PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Rendering_FontRegistry PRIVATE /utf-8)
    endif()
    # WORKING_DIRECTORY is required here: ctest's own default CWD for a discovered test does not
    # reliably match the copy step's target directory below, so the relative "Assets/Fonts/..."
    # path in FontRegistry_test.cpp resolves only when ctest is told explicitly where to run from.
    catch_discover_tests(ImFrame_Tests_Rendering_FontRegistry
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:ImFrame_Tests_Rendering_FontRegistry>"
        PROPERTIES LABELS unit
    )

    # Reuses the FA6 solid font already vendored for ImFrame_Tests_Icons_IconFont — see that
    # target's own copy step above for why a real font file (not a synthetic one) is needed.
    add_custom_command(TARGET ImFrame_Tests_Rendering_FontRegistry POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:ImFrame_Tests_Rendering_FontRegistry>/Assets/Fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Assets/Fonts/fa-solid-900.ttf"
            "$<TARGET_FILE_DIR:ImFrame_Tests_Rendering_FontRegistry>/Assets/Fonts/fa-solid-900.ttf"
        COMMENT "Copying FA6 solid font to test output directory"
    )
endif()

add_executable(ImFrame_Tests_Rendering_ClipStack
    Rendering/ClipStack_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_ClipStack PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Rendering_ClipStack
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_ClipStack PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_ClipStack PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_ClipStack PROPERTIES LABELS unit)

# ─── Phase 32.2: shader pipeline smoke test (only when the generated headers exist) ──────────
if(IMF_BUILD_NATIVE_RENDERER)
    add_executable(ImFrame_Tests_Rendering_ShaderPipeline
        Rendering/ShaderPipeline_test.cpp
    )
    target_link_libraries(ImFrame_Tests_Rendering_ShaderPipeline
        PRIVATE ImFrame_Shaders Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Rendering_ShaderPipeline PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Rendering_ShaderPipeline PRIVATE /utf-8)
    endif()
    catch_discover_tests(ImFrame_Tests_Rendering_ShaderPipeline PROPERTIES LABELS unit)
endif()

if(IMF_BACKEND_GLFW_OPENGL3)
    add_executable(ImFrame_Tests_Rendering_ImGuiCompatRenderer
        Rendering/ImGuiCompatRenderer_test.cpp
    )
    target_include_directories(ImFrame_Tests_Rendering_ImGuiCompatRenderer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ImFrame_Tests_Rendering_ImGuiCompatRenderer
        PRIVATE ImFrame ImFrame_GLFWOpenGL3 Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Rendering_ImGuiCompatRenderer PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Rendering_ImGuiCompatRenderer PRIVATE /utf-8)
    endif()
    catch_discover_tests(ImFrame_Tests_Rendering_ImGuiCompatRenderer PROPERTIES LABELS unit)
endif()

# ─── Phase 32.4: NativeRendererGL3 real-pixel tests ──────────────────────────────────────────
if(IMF_BACKEND_GLFW_OPENGL3 AND IMF_BUILD_NATIVE_RENDERER)
    add_executable(ImFrame_Tests_Rendering_NativeRendererGL3
        Rendering/NativeRendererGL3_test.cpp
    )
    target_include_directories(ImFrame_Tests_Rendering_NativeRendererGL3 PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ImFrame_Tests_Rendering_NativeRendererGL3
        PRIVATE ImFrame ImFrame_GLFWOpenGL3 glad::glad Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Rendering_NativeRendererGL3 PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Rendering_NativeRendererGL3 PRIVATE /utf-8)
    endif()
    catch_discover_tests(ImFrame_Tests_Rendering_NativeRendererGL3 PROPERTIES LABELS unit)
endif()

add_executable(ImFrame_Tests_Rendering_Canvas2D
    Rendering/Canvas2D_test.cpp
)
target_include_directories(ImFrame_Tests_Rendering_Canvas2D PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Rendering_Canvas2D
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_Canvas2D PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_Canvas2D PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_Canvas2D PROPERTIES LABELS unit)

# ─── Phase 26: Camera3D, Viewport3D, Gizmo tests ─────────────────────────────
# Camera3D_test: pure math — no ImGui context required, no backend needed.
# Gizmo_test: API-level null-guard tests; no ImGui context required.

add_executable(ImFrame_Tests_Rendering_Camera3D
    Rendering/Camera3D_test.cpp
)
target_link_libraries(ImFrame_Tests_Rendering_Camera3D
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_Camera3D PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_Camera3D PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_Camera3D PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Rendering_Gizmo
    Rendering/Gizmo_test.cpp
)
target_link_libraries(ImFrame_Tests_Rendering_Gizmo
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Rendering_Gizmo PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Rendering_Gizmo PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Rendering_Gizmo PROPERTIES LABELS unit)

# ─── Phase 27: Widget tree core tests ────────────────────────────────────────
# Layout_test / Reconciler_test: pure logic, no ImGui context required, but need
# ${CMAKE_SOURCE_DIR}/src as an include dir to reach internal Tree/ headers.
# Primitives_test: drives a real HeadlessBackend Application via SetRoot().

add_executable(ImFrame_Tests_Tree_Layout
    Tree/Layout_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_Layout PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Tree_Layout
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_Layout PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_Layout PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_Layout PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_Reconciler
    Tree/Reconciler_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_Reconciler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Tree_Reconciler
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_Reconciler PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_Reconciler PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_Reconciler PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_ReconcilerPerformance
    Tree/ReconcilerPerformance_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_ReconcilerPerformance PRIVATE
    ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ImFrame_Tests_Tree_ReconcilerPerformance
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_ReconcilerPerformance PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_ReconcilerPerformance PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_ReconcilerPerformance PROPERTIES LABELS unit)

# ─── Phase 32.5: Reconciler + NativeRendererGL3 end-to-end real-pixel test ───────────────────
if(IMF_BACKEND_GLFW_OPENGL3 AND IMF_BUILD_NATIVE_RENDERER)
    add_executable(ImFrame_Tests_Tree_ReconcilerNativeRenderer
        Tree/ReconcilerNativeRenderer_test.cpp
    )
    target_include_directories(ImFrame_Tests_Tree_ReconcilerNativeRenderer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ImFrame_Tests_Tree_ReconcilerNativeRenderer
        PRIVATE ImFrame ImFrame_GLFWOpenGL3 glad::glad Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Tree_ReconcilerNativeRenderer PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Tree_ReconcilerNativeRenderer PRIVATE /utf-8)
    endif()
    catch_discover_tests(ImFrame_Tests_Tree_ReconcilerNativeRenderer PROPERTIES LABELS unit)
endif()

# ─── Phase 32.8: Reconciler + ImageWidget end-to-end real-pixel test (default ImGuiCompatRenderer,
# no NativeRendererGL3 involved — gated on the backend only, not IMF_BUILD_NATIVE_RENDERER) ────────
if(IMF_BACKEND_GLFW_OPENGL3)
    add_executable(ImFrame_Tests_Tree_ReconcilerImage
        Tree/ReconcilerImage_test.cpp
    )
    target_include_directories(ImFrame_Tests_Tree_ReconcilerImage PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ImFrame_Tests_Tree_ReconcilerImage
        PRIVATE ImFrame ImFrame_GLFWOpenGL3 glad::glad Catch2::Catch2WithMain
    )
    target_compile_features(ImFrame_Tests_Tree_ReconcilerImage PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(ImFrame_Tests_Tree_ReconcilerImage PRIVATE /utf-8)
    endif()
    catch_discover_tests(ImFrame_Tests_Tree_ReconcilerImage PROPERTIES LABELS unit)
endif()

add_executable(ImFrame_Tests_Tree_Primitives
    Tree/Primitives_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_Primitives PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_Primitives
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_Primitives PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_Primitives PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_Primitives PROPERTIES LABELS unit)

# ─── Phase 28: Reactive state tests ──────────────────────────────────────────
# State_test: pure logic (State/Signal/Computed), no ImGui context required.
# InheritedWidget_test: mixes pure element-tree tests with HeadlessBackend-driven
# reconciler integration tests; needs ${CMAKE_SOURCE_DIR} to reach
# "Backends/Headless/HeadlessBackend.hpp" directly.

add_executable(ImFrame_Tests_Tree_State
    Tree/State_test.cpp
)
target_link_libraries(ImFrame_Tests_Tree_State
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_State PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_State PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_State PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Tree_InheritedWidget
    Tree/InheritedWidget_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_InheritedWidget PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_InheritedWidget
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_InheritedWidget PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_InheritedWidget PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_InheritedWidget PROPERTIES LABELS unit)

# ─── Phase 29: Portal, VirtualList tests ─────────────────────────────────────
# Both drive a real HeadlessBackend + Application::SetRoot(); need
# ${CMAKE_SOURCE_DIR} to reach "Backends/Headless/HeadlessBackend.hpp" directly.

add_executable(ImFrame_Tests_Tree_Portal
    Tree/Portal_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_Portal PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_Portal
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_Portal PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_Portal PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_Portal PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_VirtualList
    Tree/VirtualList_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_VirtualList PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_VirtualList
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_VirtualList PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_VirtualList PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_VirtualList PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_Widgets
    Tree/Widgets_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_Widgets PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_Widgets
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_Widgets PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_Widgets PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_Widgets PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_DockSpaceWidget
    Tree/DockSpaceWidget_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_DockSpaceWidget PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_DockSpaceWidget
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_DockSpaceWidget PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_DockSpaceWidget PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_DockSpaceWidget PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Tree_WidgetTestDriver
    Tree/WidgetTestDriver_test.cpp
)
target_include_directories(ImFrame_Tests_Tree_WidgetTestDriver PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ImFrame_Tests_Tree_WidgetTestDriver
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Tree_WidgetTestDriver PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(ImFrame_Tests_Tree_WidgetTestDriver PRIVATE /utf-8)
endif()
catch_discover_tests(ImFrame_Tests_Tree_WidgetTestDriver PROPERTIES LABELS unit)
