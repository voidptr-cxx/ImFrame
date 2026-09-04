cmake_minimum_required(VERSION 3.25)

# IMF_BACKEND_SDL3_VULKAN must be read before project() so the corresponding
# vcpkg manifest feature can be requested before the toolchain file resolves
# dependencies (during the first project() call).
option(IMF_BACKEND_SDL3_VULKAN "Build the SDL3 + Vulkan 1.3 backend" OFF)
if(IMF_BACKEND_SDL3_VULKAN)
    list(APPEND VCPKG_MANIFEST_FEATURES "vulkan-backend")
endif()

# IMF_BUILD_METAL_BACKEND must be read before project() for the same reason —
# see the IMF_BACKEND_SDL3_VULKAN block above and DECISIONS.md 2026-06-17.
# Apple-only at build time: Backends/SDL3Metal's own CMakeLists.txt is added
# only under `if(APPLE)` below, so enabling this option on a non-Apple host
# requests the vcpkg feature but never compiles any Objective-C++ source.
option(IMF_BUILD_METAL_BACKEND "Build the SDL3 + Metal backend (macOS; iOS scaffold only)" OFF)
if(IMF_BUILD_METAL_BACKEND)
    list(APPEND VCPKG_MANIFEST_FEATURES "metal-backend")
endif()

# IMF_BUILD_DX12_BACKEND must be read before project() for the same reason —
# see the IMF_BACKEND_SDL3_VULKAN block above. Windows-only at build time:
# Backends/SDL3DX12's own CMakeLists.txt is added only under `if(WIN32)` below.
option(IMF_BUILD_DX12_BACKEND "Build the SDL3 + DirectX 12 backend (Windows only)" OFF)
if(IMF_BUILD_DX12_BACKEND)
    list(APPEND VCPKG_MANIFEST_FEATURES "dx12-backend")
endif()

# IMF_BUILD_WEBGPU_BACKEND must be read before project() for the same reason —
# see the IMF_BACKEND_SDL3_VULKAN block above. Unlike Metal/DX12, this backend
# is genuinely cross-platform: Dawn translates to D3D12/Metal/Vulkan per host
# platform automatically, so Backends/DawnWebGPU is added on every platform.
option(IMF_BUILD_WEBGPU_BACKEND "Build the SDL3 + Dawn WebGPU backend" OFF)
if(IMF_BUILD_WEBGPU_BACKEND)
    list(APPEND VCPKG_MANIFEST_FEATURES "webgpu-backend")
endif()

# IMF_BUILD_NATIVE_RENDERER must be read before project() for the same reason as the backend
# options above. Gates Phase 32's NativeRenderer shader build pipeline (glslang[tools] is a real
# build-time toolchain dependency, not a lightweight header — see .claude/DECISIONS.md, Phase 32.2)
# behind an explicit opt-in, matching how the GPU backends themselves are opt-in. NativeRenderer is
# entirely optional today (Application::UseNativeRenderer() has no callers yet outside its own
# tests) — a default `cmake -B build` should not silently grow a new toolchain requirement for it.
option(IMF_BUILD_NATIVE_RENDERER "Build Phase 32's NativeRenderer shader pipeline (requires glslang)" OFF)
if(IMF_BUILD_NATIVE_RENDERER)
    list(APPEND VCPKG_MANIFEST_FEATURES "native-renderer")
endif()

# IMF_BUILD_TEXT_MSDF must be read before project() for the same reason as the options above.
# Gates Phase 33's MSDF text pipeline (harfbuzz + msdfgen are real build-time toolchain
# dependencies, not lightweight headers) behind an explicit opt-in, matching every other
# optional-subsystem option in this file. Still experimental as of Phase 33.1 — a default
# `cmake -B build` should not silently grow two new toolchain requirements for it.
option(IMF_BUILD_TEXT_MSDF "Build Phase 33's MSDF text pipeline (requires harfbuzz, msdfgen)" OFF)
if(IMF_BUILD_TEXT_MSDF)
    list(APPEND VCPKG_MANIFEST_FEATURES "text-msdf")
endif()

project(ImFrame
    VERSION 2.0.0
    DESCRIPTION "Immediate-mode UI framework wrapping Dear ImGui"
    LANGUAGES CXX
)

# ─── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit a compile_commands.json for IDE tooling and clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Options ──────────────────────────────────────────────────────────────────
option(IMF_BUILD_TESTS            "Build the ImFrame test suite"           ON)
option(IMF_BACKEND_GLFW_OPENGL3   "Build the GLFW + OpenGL3 backend"       ON)

# IMF_DEV_TOOLS defaults ON in Debug, OFF in Release/RelWithDebInfo/MinSizeRel.
if(NOT DEFINED IMF_DEV_TOOLS)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
        set(IMF_DEV_TOOLS ON CACHE BOOL "Enable developer tooling")
    else()
        set(IMF_DEV_TOOLS OFF CACHE BOOL "Enable developer tooling")
    endif()
endif()

# ─── vcpkg dependencies ───────────────────────────────────────────────────────
find_package(imgui     CONFIG REQUIRED)
find_package(implot    CONFIG REQUIRED)
find_package(glfw3     CONFIG REQUIRED)
find_package(glad      CONFIG REQUIRED)
find_package(freetype  CONFIG REQUIRED)

if(IMF_BUILD_TEXT_MSDF)
    find_package(harfbuzz CONFIG REQUIRED)
    find_package(msdfgen  CONFIG REQUIRED)
endif()

# SDL3 is not used on Emscripten at all (Backends/DawnWebGPU/CMakeLists.txt's
# EMSCRIPTEN branch uses DOM events directly — see the Phase 23 proposal's
# Input section), so it is skipped there even when IMF_BUILD_WEBGPU_BACKEND=ON.
if((IMF_BACKEND_SDL3_VULKAN OR IMF_BUILD_METAL_BACKEND OR IMF_BUILD_DX12_BACKEND OR IMF_BUILD_WEBGPU_BACKEND)
   AND NOT EMSCRIPTEN)
    find_package(SDL3 CONFIG REQUIRED)
endif()

if(IMF_BACKEND_SDL3_VULKAN)
    find_package(Vulkan                REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
endif()

# vcpkg's dawn port is the native-only path. On Emscripten, WebGPU comes from
# emscripten's own --use-port=emdawnwebgpu instead (wired directly in
# Backends/DawnWebGPU/CMakeLists.txt) — no vcpkg dawn package is involved.
# NOTE: this whole IMF_BUILD_WEBGPU_BACKEND=ON path has not been exercised
# under an actual Emscripten toolchain invocation — see DECISIONS.md/
# PHASE_STATUS.md. The other vcpkg dependencies below (imgui, implot, glfw3,
# glad, freetype) would also need a working wasm32-emscripten vcpkg triplet
# for a real Emscripten build to configure at all, which has not been
# attempted either.
if(IMF_BUILD_WEBGPU_BACKEND AND NOT EMSCRIPTEN)
    find_package(dawn CONFIG REQUIRED)
endif()

# Threads is required by Thread, ThreadPool, BackgroundWorker, FileWatcher (std::jthread).
find_package(Threads REQUIRED)

# ─── Compiler warnings (interface target so sub-targets can link it) ──────────
add_library(imframe_warnings INTERFACE)
if(MSVC)
    target_compile_options(imframe_warnings INTERFACE
        /W4
        /WX
        /wd4251     # DLL-interface warning — not applicable to a static lib
        /utf-8
    )
else()
    target_compile_options(imframe_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wno-unknown-pragmas
    )
endif()

# ─── ImFrame core static library ──────────────────────────────────────────────
add_library(ImFrame STATIC
    src/Placeholder.cpp

    # ── Phase 3: Filesystem utilities ─────────────────────────────────────────
    src/Utility/Path.cpp
    src/Utility/File.cpp
    src/Utility/Directory.cpp
    src/Utility/FileWatcher.cpp

    # ── Phase 4: Concurrency utilities ────────────────────────────────────────
    src/Utility/Thread.cpp
    src/Utility/ThreadPool.cpp
    src/Utility/BackgroundWorker.cpp

    # ── Phase 5: Event system utilities ───────────────────────────────────────
    src/Utility/Signal.cpp
    src/Utility/EventBus.cpp

    # ── Phase 6: Supporting utilities ─────────────────────────────────────────
    src/Utility/Timer.cpp
    src/Utility/Logger.cpp
    src/Utility/Sinks/ConsoleSink.cpp
    src/Utility/Sinks/FileSink.cpp
    src/Utility/Sinks/UiSink.cpp
    src/Utility/Config.cpp

    # ── Phase 7: Application layer ─────────────────────────────────────────────
    src/App/Application.cpp
    src/App/WindowManager.cpp
    src/App/DockSpace.cpp

    # ── Phase 8: Theme engine ──────────────────────────────────────────────────
    src/Theme/Theme.cpp

    # ── Phase 9: Icon font integration ────────────────────────────────────────
    src/Icons/IconFont.cpp

    # ── Phase 10: Core widget set ──────────────────────────────────────────────
    src/Widgets/Button.cpp
    src/Widgets/Checkbox.cpp
    src/Widgets/Radio.cpp
    src/Widgets/ColorEdit.cpp
    src/Widgets/Image.cpp
    src/Widgets/ProgressBar.cpp
    src/Widgets/Separator.cpp
    src/Widgets/WidgetImpl.cpp

    # ── Phase 11: Layout containers ───────────────────────────────────────────
    src/Layout/ChildScope.cpp

    # ── Phase 13: Overlay system ──────────────────────────────────────────────
    src/Overlay/Toast.cpp
    src/Overlay/Modal.cpp
    src/Overlay/ContextMenu.cpp

    # ── Phase 14: Table and PropertyGrid ──────────────────────────────────────
    src/Widgets/Table.cpp
    src/Widgets/PropertyGrid.cpp

    # ── Phase 15: Plot integration ────────────────────────────────────────────
    src/Widgets/PlotContext.cpp

    # ── Phase 19: Headless backend (compiled into core for Application::CreateHeadless) ──
    Backends/Headless/HeadlessBackend.cpp
    Backends/Headless/ViewportHeadless.cpp

    # ── Phase 24: Viewport foundation ────────────────────────────────────────────
    src/App/ApplicationContext.cpp
    src/Rendering/Viewport.cpp
    src/Rendering/ViewportRegistry.cpp

    # ── Phase 25: Canvas2D, Camera2D, DrawContext ─────────────────────────────
    src/Rendering/Camera2D.cpp
    src/Rendering/DrawContext.cpp
    src/Rendering/Canvas2D.cpp

    # ── Phase 26: Viewport3D, Camera3D, Gizmo ─────────────────────────────────
    src/Rendering/Camera3D.cpp
    src/Rendering/Viewport3D.cpp
    src/Rendering/Gizmo.cpp

    # ── Phase 27: Widget tree core ────────────────────────────────────────────
    src/Tree/ElementInternal.cpp
    src/Tree/Layout.cpp
    src/Tree/Reconciler.cpp
    src/Tree/RenderObjects/RootBridge.cpp
    src/Tree/RenderObjects/BoxRO.cpp
    src/Tree/RenderObjects/TextRO.cpp
    src/Tree/RenderObjects/FlexRO.cpp
    src/Tree/RenderObjects/StackRO.cpp
    src/Tree/RenderObjects/GestureRegionRO.cpp
    src/Tree/RenderObjects/SizedBoxRO.cpp
    src/Tree/RenderObjects/ExpandedRO.cpp
    src/Tree/RenderObjects/SpacerRO.cpp

    # ── Phase 29: Portal, VirtualList ─────────────────────────────────────────
    src/Tree/PortalRegistry.cpp
    src/Tree/RenderObjects/PortalRO.cpp
    src/Tree/RenderObjects/VirtualListRO.cpp

    # ── Phase 30.1: Widget replacement gap closure (GridWidget) ───────────────
    src/Tree/RenderObjects/GridRO.cpp

    # ── Phase 30.3: DockSpace reimplementation (DockSpaceWidget) ──────────────
    src/Tree/RenderObjects/DockSpaceRO.cpp
    src/Tree/RenderObjects/DockSpaceWidgetRO.cpp

    # ── Phase 31.2: IRenderer / ImGuiCompatRenderer ───────────────────────────
    src/Rendering/Renderers/ImGuiCompatRenderer.cpp

    # ── Phase 32.1: NativeRenderer core types (BatchBuilder/TextureAtlas/ClipStack) ──
    src/Rendering/Renderers/BatchBuilder.cpp
    src/Rendering/Renderers/TextureAtlas.cpp
    src/Rendering/Renderers/ClipStack.cpp
)

# ── Phase 17: Developer tooling (conditional) ─────────────────────────────────
if(IMF_DEV_TOOLS)
    target_sources(ImFrame PRIVATE
        src/DevTools/LogViewer.cpp
        src/DevTools/PerfOverlay.cpp
        src/DevTools/ThemeHotReload.cpp
    )
    target_compile_definitions(ImFrame PUBLIC IMF_DEV_TOOLS)
endif()

# ── Phase 33.2: MSDF text pipeline — FontFace/FontRegistry (opt-in) ───────────
# freetype is already found unconditionally above (ImGui's own atlas already uses it via
# its "freetype" feature), but ImFrame's own src/ code has never linked/called it directly
# before this — gated behind IMF_BUILD_TEXT_MSDF so it stays optional, matching every other
# optional subsystem in this file.
if(IMF_BUILD_TEXT_MSDF)
    target_sources(ImFrame PRIVATE
        src/Rendering/Text/FontFace.cpp
        src/Rendering/Text/FontRegistry.cpp
        src/Rendering/Text/TextShaper.cpp
        src/Rendering/Text/GlyphAtlas.cpp
    )
    # msdfgen-ext for its FreeType-integrated adoptFreetypeFont()/loadGlyph() (GlyphAtlas, Phase 33.4,
    # reuses FontRegistry's own FT_Face directly, no separate FreeType load) -- msdfgen-core is also
    # required directly: Shape/Projection/generateMTSDF/etc. live there, not in msdfgen-ext itself.
    target_link_libraries(ImFrame PRIVATE Freetype::Freetype harfbuzz::harfbuzz
        msdfgen::msdfgen-ext msdfgen::msdfgen-core)
endif()

target_include_directories(ImFrame
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}        # allows #include "Backends/Headless/HeadlessBackend.hpp"
        ${CMAKE_SOURCE_DIR}/src    # allows cross-directory src/ includes e.g. "App/ApplicationContext.hpp"
)

target_link_libraries(ImFrame
    PRIVATE
        imframe_warnings
        Threads::Threads
        imgui::imgui        # App layer (Phase 7+) calls ImGui directly
        implot::implot      # Phase 15+ plot widgets
)

# macOS: Shell integration framework required for SHGetKnownFolderPath equivalent
# (kqueue needs no extra framework; Path::FromUserHome uses getenv)
if(WIN32)
    target_link_libraries(ImFrame PRIVATE shell32)   # SHGetKnownFolderPath
endif()

target_compile_features(ImFrame PUBLIC cxx_std_23)

# ─── Phase 32.2: NativeRenderer shader pipeline (opt-in) ──────────────────────
# Compiles Shaders/*.glsl to SPIR-V at build time via glslang and embeds the result as
# generated C++ headers — see cmake/CompileShader.cmake. No target links against
# ImFrame_Shaders yet (NativeRenderer itself doesn't exist until a later 32.x sub-phase);
# this section exists purely to prove the pipeline compiles real, valid shaders.
if(IMF_BUILD_NATIVE_RENDERER)
    include(cmake/CompileShader.cmake)

    set(IMF_SHADER_HEADERS "")
    foreach(IMF_SHADER_NAME SDFRect Image Path MSDFText)
        compile_shader("${CMAKE_SOURCE_DIR}/Shaders/${IMF_SHADER_NAME}.glsl" vert ${IMF_SHADER_NAME}
            OUT_HEADER IMF_SHADER_VERT_HEADER)
        compile_shader("${CMAKE_SOURCE_DIR}/Shaders/${IMF_SHADER_NAME}.glsl" frag ${IMF_SHADER_NAME}
            OUT_HEADER IMF_SHADER_FRAG_HEADER)
        list(APPEND IMF_SHADER_HEADERS "${IMF_SHADER_VERT_HEADER}" "${IMF_SHADER_FRAG_HEADER}")
    endforeach()

    add_library(ImFrame_Shaders INTERFACE ${IMF_SHADER_HEADERS})
    target_include_directories(ImFrame_Shaders INTERFACE ${IMF_SHADER_GENERATED_DIR}/..)
endif()

# ─── Backends ─────────────────────────────────────────────────────────────────
if(IMF_BACKEND_GLFW_OPENGL3)
    add_subdirectory(Backends/GLFWOpenGL3)
endif()

if(IMF_BACKEND_SDL3_VULKAN)
    add_subdirectory(Backends/SDL3Vulkan)
endif()

if(IMF_BUILD_METAL_BACKEND AND NOT APPLE)
    # No silent no-op: a platform limitation must be an explicit, visible
    # signal (see .claude/skills/imframe-cross-platform-checklist), not a
    # quietly-skipped add_subdirectory().
    message(WARNING "IMF_BUILD_METAL_BACKEND=ON has no effect on this platform — "
                     "the Metal backend is Apple-only. Backends/SDL3Metal will not be built.")
elseif(APPLE AND IMF_BUILD_METAL_BACKEND)
    add_subdirectory(Backends/SDL3Metal)
endif()

# iOSMetal's own CMakeLists.txt is a structural no-op (immediate return())
# unless CMAKE_SYSTEM_NAME is "iOS" — always added so the scaffold exists
# whenever an iOS toolchain is actually used, independent of IMF_BUILD_METAL_BACKEND.
add_subdirectory(Backends/iOSMetal)

if(IMF_BUILD_DX12_BACKEND AND NOT WIN32)
    # No silent no-op — same rationale as the Metal block above.
    message(WARNING "IMF_BUILD_DX12_BACKEND=ON has no effect on this platform — "
                     "the DX12 backend is Windows-only. Backends/SDL3DX12 will not be built.")
elseif(WIN32 AND IMF_BUILD_DX12_BACKEND)
    add_subdirectory(Backends/SDL3DX12)
endif()

if(IMF_BUILD_WEBGPU_BACKEND)
    add_subdirectory(Backends/DawnWebGPU)
endif()

# ─── Tests and Examples ───────────────────────────────────────────────────────
if(IMF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

add_subdirectory(Examples/DemoApp)
