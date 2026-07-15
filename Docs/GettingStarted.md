# Getting Started with ImFrame

ImFrame is a C++23 immediate-mode UI framework built on Dear ImGui. As of
v2.0.0, the entire public surface is the **widget tree** model: you describe
your UI as a `Build()` method returning a `Tree::Widget`, and ImFrame
reconciles that description against the previous frame's tree each time your
state changes.

## Building

```bash
git clone https://github.com/voidptr-cxx/ImFrame.git
cd ImFrame

# Configure with vcpkg (set VCPKG_ROOT to your vcpkg installation)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release
```

The default configuration builds the GLFW+OpenGL3 backend
(`IMF_BACKEND_GLFW_OPENGL3=ON`). See [Backend Guide](BackendGuide.md) for how
to enable Vulkan, Metal, DX12, or WebGPU instead.

## A complete app in ~20 lines

Every ImFrame app has three pieces: a **backend** (which window/graphics API
to use), an **`Application`** (owns the window and the frame loop), and a
**root `Component`** (your UI, described declaratively).

```cpp
#include "GLFWOpenGL3Backend.hpp"
#include "ImFrame/ImFrame.hpp"

using namespace ImFrame;

struct Counter {
    struct S { int count = 0; };
    Tree::State<S> state;

    [[nodiscard]] Tree::Widget Build() const {
        return Tree::Primitives::Flex(Tree::Primitives::Flex::Axis::Vertical).Gap(8.0f).Children({
            Tree::Primitives::Text(std::format("Count: {}", state.Get().count)),
            Widgets::ButtonWidget("Increment").OnClick([s = state] {
                s.Set([](S& st) { ++st.count; });
            }),
        });
    }
};

int main() {
    Counter root;

    auto app = App::Application(
        std::make_unique<Internal::GLFWOpenGL3Backend>(),
        WindowConfig{ .Title = "Counter" });

    app.WithTheme(Themes::Dracula);
    app.SetRoot(root);

    return app.Run().has_value() ? 0 : 1;
}
```

That's it: no `OnUi()` imperative callback, no manual `ImGui::Begin`/`End`
pairs. `Build()` runs every frame; ImFrame reconciles the returned `Widget`
tree against what was there before and only re-renders what actually
changed. Clicking the button calls `State<S>::Set()`, which marks the
`Counter` component dirty — the next frame's `Build()` call picks up the new
count.

## Where to go next

| Guide | Covers |
|---|---|
| [Architecture Overview](Architecture.md) | The three-layer model (backends, tree core, widget library) and how they fit together |
| [Widget Reference](WidgetReference.md) | Every `Component`/`PrimitiveWidget` type, with usage examples |
| [State Management Guide](StateManagement.md) | `State<T>`, `Signal<T>`, `Computed<T>`, `InheritedWidget<T>` |
| [Rendering Surfaces Guide](RenderingSurfaces.md) | `Viewport`, `Canvas2D`, `Viewport3D` for custom-rendered content |
| [Backend Guide](BackendGuide.md) | Selecting and configuring OpenGL3 / Vulkan / Metal / DX12 / WebGPU |
| [Migration Guide](Migration_v1_to_v2.md) | Historical record of the v1.x imperative → v2.x widget-tree migration |

## Project status

See `.claude/PHASE_STATUS.md` for the current implementation progress and
`.claude/DECISIONS.md` for the architectural decision log.
