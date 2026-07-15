# Backend Guide

ImFrame supports five rendering backends. Which one your application uses is
a **compile-time / link-time choice** — there is no runtime backend-switching
API. Each backend is its own CMake static library target; you enable the
CMake option, link the matching target, and construct that backend's
concrete `IBackend` implementation directly.

## Available backends

| Backend | CMake option | Library target | Platform |
|---|---|---|---|
| GLFW + OpenGL 3.3 | `IMF_BACKEND_GLFW_OPENGL3` (default **ON**) | `ImFrame_GLFWOpenGL3` | Windows, Linux, macOS |
| SDL3 + Vulkan | `IMF_BACKEND_SDL3_VULKAN` (default OFF) | `ImFrame_SDL3Vulkan` | Windows, Linux |
| SDL3 + Metal | `IMF_BUILD_METAL_BACKEND` (default OFF) | `ImFrame_SDL3Metal` | macOS only |
| SDL3 + DX12 | `IMF_BUILD_DX12_BACKEND` (default OFF) | `ImFrame_SDL3DX12` | Windows only |
| Dawn WebGPU | `IMF_BUILD_WEBGPU_BACKEND` (default OFF) | `ImFrame_DawnWebGPU` | Windows, Linux, macOS, Emscripten |

Metal and DX12 CMake options only take effect on their respective platforms
(`elseif(APPLE AND IMF_BUILD_METAL_BACKEND)` / `elseif(WIN32 AND
IMF_BUILD_DX12_BACKEND)`) — enabling them elsewhere is a no-op.

## Selecting a backend

**1. Enable the CMake option** when configuring:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DIMF_BACKEND_SDL3_VULKAN=ON
```

**2. Link the matching target** in your `CMakeLists.txt` (this is exactly
what `Examples/DemoApp/CMakeLists.txt` does for OpenGL3):

```cmake
add_executable(MyApp main.cpp)
target_link_libraries(MyApp
    PRIVATE
        ImFrame
        ImFrame_SDL3Vulkan
        imgui::imgui
)
target_compile_features(MyApp PRIVATE cxx_std_23)
```

**3. Include that backend's header and construct it** when building your
`Application`:

```cpp
#include "SDL3VulkanBackend.hpp"
#include "ImFrame/ImFrame.hpp"

auto app = ImFrame::App::Application(
    std::make_unique<ImFrame::Internal::SDL3VulkanBackend>(),
    ImFrame::WindowConfig{ .Title = "My App" });
```

Swapping backends for an existing app is exactly these three steps in
reverse for the old backend and forward for the new one — your
`Component`/`Tree::Widget` code never changes, since it never touches
backend types directly.

## `WindowConfig`

Every backend's `Init()` takes the same `WindowConfig`:

```cpp
struct WindowConfig {
    std::string  Title     = "ImFrame Application";
    int          Width     = 1280;
    int          Height    = 720;
    VSyncMode    VSync     = VSyncMode::On;
    bool         Docking   = true;
    bool         Viewports = false;   // multi-viewport (OS-level floating windows)
    // ...
};
```

## Headless / testing

`Internal::HeadlessBackend` (built into `ImFrame` core, no separate target)
implements `IBackend` without opening a window — used by the test suite and
by `Application::CreateHeadless()`. It does **not** perform real rasterization
today (`ReadPixels()` returns an unwritten, zero-filled buffer) — this is a
known, tracked limitation; see `.claude/DECISIONS.md` for the Phase 36
`SoftwareRenderer` plan that will close it. For pixel-level backend testing
today, the Vulkan/DX12/WebGPU/OpenGL3 backends each expose their own
backend-specific `ReadPixels()` (not part of `IBackend`, added incrementally
per backend as conformance tests needed them) — see
`Tests/Backends/*Conformance_test.cpp` for usage.

## Conformance across backends

`Tests/Backends/*Conformance_test.cpp` renders a representative widget tree
(`Tests/Backends/ConformanceApp.hpp`) across every theme and several layout
sizes on each buildable backend, asserting each backend renders *something*
non-trivial without crashing. **This is not yet a true cross-backend
pixel-diff** — that requires `HeadlessBackend` to actually rasterize, which
is Phase 36 work — so today each backend is only verified for
self-consistency, not verified to produce byte-identical output to the
others. Track this gap in `.claude/PHASE_STATUS.md`.
