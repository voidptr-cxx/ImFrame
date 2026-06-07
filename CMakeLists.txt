cmake_minimum_required(VERSION 3.25)

project(ImFrame
    VERSION 1.0.0
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
find_package(glfw3     CONFIG REQUIRED)
find_package(glad      CONFIG REQUIRED)
find_package(freetype  CONFIG REQUIRED)

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
    src/Widgets/Spacer.cpp
    src/Widgets/Text.cpp
    src/Widgets/WidgetImpl.cpp
)

target_include_directories(ImFrame
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ImFrame
    PRIVATE
        imframe_warnings
        Threads::Threads
        imgui::imgui        # App layer (Phase 7+) calls ImGui directly
)

# macOS: Shell integration framework required for SHGetKnownFolderPath equivalent
# (kqueue needs no extra framework; Path::FromUserHome uses getenv)
if(WIN32)
    target_link_libraries(ImFrame PRIVATE shell32)   # SHGetKnownFolderPath
endif()

target_compile_features(ImFrame PUBLIC cxx_std_23)

# ─── Backends ─────────────────────────────────────────────────────────────────
if(IMF_BACKEND_GLFW_OPENGL3)
    add_subdirectory(Backends/GLFWOpenGL3)
endif()

# ─── Tests and Examples ───────────────────────────────────────────────────────
if(IMF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

add_subdirectory(Examples/DemoApp)
