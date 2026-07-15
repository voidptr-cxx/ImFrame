# Rendering Surfaces Guide

Most of an ImFrame UI is built from `Tree::Primitives` and the standard
widget library (see the [Widget Reference](WidgetReference.md)). When you
need to draw content ImFrame doesn't have a primitive for — a 2D scene, a 3D
viewport, arbitrary custom-rendered pixels — use one of the three rendering
surfaces below.

All three follow the same shape: an **imperative owner object** (`Viewport`,
`Canvas2D`, `Viewport3D`) that you construct once and keep alive for as long
as the surface exists, plus a **declarative wrapper widget**
(`ViewportWidget`, `Canvas2DWidget`, `Viewport3DWidget`) that places it into
your `Build()` tree. The owner objects are non-copyable — the wrappers bind
to them by raw pointer, the same pattern every stateful widget in the
[Widget Reference](WidgetReference.md) uses.

## `Viewport`

An offscreen framebuffer composited into the frame as a texture — the
general-purpose building block. `Canvas2D` and `Viewport3D` are both built
on top of it.

```cpp
struct Scene {
    Rendering::Viewport viewport{"main_scene"};

    Scene() {
        viewport.Size({640.0f, 480.0f});
        viewport.OnRender([](const Rendering::RenderContext& ctx) {
            // All GPU work here — this fires before BeginFrame() each frame.
        });
    }

    [[nodiscard]] Tree::Widget Build() const {
        return Rendering::ViewportWidget(&viewport);
    }
};
```

Key API:

```cpp
Viewport& Size(Vec2 size);
Viewport& OnRender(Delegate<void(const RenderContext&)> callback);
Viewport& OnResize(Delegate<void(Vec2)> callback);
Viewport& OnInput(Delegate<void(const InputEvent&)> callback);
Viewport& Borderless(bool = true);
void      Show();   // composites via ImGui::Image() — call from OnUi if not using ViewportWidget
```

Two `Viewport`s constructed with the same `id` string share one underlying
framebuffer — this is intentional (documented), not a bug, and is how you'd
show the same rendered content in two places without rendering it twice.

For automated tests, `HeadlessViewport` (a `Viewport` subclass) adds
`ReadPixels() -> std::expected<std::vector<std::byte>, Core::Error>` so a
headless backend can assert on rendered output.

## `Canvas2D`

A 2D scene canvas with layers, a built-in `Camera2D`, hit-testing, and
static-subtree caching — for diagrams, node graphs, level editors, and
similar 2D-scene UIs. Renders via `ImDrawList` into an ImGui child window.

```cpp
struct DiagramEditor {
    Rendering::Canvas2D canvas{"diagram"};

    DiagramEditor() {
        canvas.Size({800.0f, 600.0f});
        canvas.InteractionMode(Rendering::CanvasInteractionMode::PanZoom);
        canvas.OnDraw([](Rendering::DrawContext& dc) {
            // Issue draw calls in world space; the camera transform is applied for you.
        });
        canvas.OnHit([](std::uint32_t id, Vec2 worldPos) {
            // Fired when a hit-testable element is clicked.
        });
    }

    [[nodiscard]] Tree::Widget Build() const {
        return Rendering::Canvas2DWidget(&canvas);
    }
};
```

Key API:

```cpp
Canvas2D& Size(Vec2 size);
Canvas2D& Background(Vec4 color);
Canvas2D& InteractionMode(CanvasInteractionMode mode);   // None | Pan | PanZoom
Canvas2D& OnDraw(Delegate<void(DrawContext&)> callback);
Canvas2D& OnHit(Delegate<void(std::uint32_t, Vec2)> callback);
Canvas2D& MinZoom(float) / MaxZoom(float);
void      ResetCamera();
void      FitToRect(Vec2 min, Vec2 max);
[[nodiscard]] const Camera2D& GetCamera() const noexcept;
[[nodiscard]] std::optional<std::uint32_t> HitTest(Vec2 screenPos) const;
void SetLayerVisible(int layerIndex, bool visible);
void SetLayerOpacity(int layerIndex, float opacity);
```

## `Viewport3D`

A higher-level wrapper over `Viewport` for 3D content — owns a `Camera3D`,
drives it from mouse/keyboard input automatically (orbit, fly, or
orthographic), and delivers view/projection matrices to your render
callback.

```cpp
struct SceneEditor {
    Rendering::Viewport3D viewport3d{"scene_3d"};

    SceneEditor() {
        viewport3d.Size({800.0f, 600.0f});
        viewport3d.InteractionMode(Rendering::CameraMode::Orbit);
        viewport3d.OnRender([](const Rendering::Viewport3DRenderInfo& info) {
            // info.View, info.Projection, info.ViewProjection, info.CameraPosition, ...
        });
    }

    [[nodiscard]] Tree::Widget Build() const {
        return Rendering::Viewport3DWidget(&viewport3d);
    }
};
```

Key API:

```cpp
Viewport3D& Size(Vec2 size);
Viewport3D& CameraControl(bool enabled);              // default true
Viewport3D& InteractionMode(CameraMode mode);          // Orbit | Fly | Orthographic
Viewport3D& OnRender(Delegate<void(const Viewport3DRenderInfo&)> callback);
[[nodiscard]] Camera3D& GetCamera() noexcept;
[[nodiscard]] Ray3D     Unproject(Vec2 screenPos) const noexcept;   // valid after Show() in the same frame
[[nodiscard]] std::optional<Vec2> Project(Vec3 worldPos) const noexcept;
[[nodiscard]] const float* GetViewMatrix() / GetProjectionMatrix() / GetViewProjectionMatrix() const noexcept;
```

`Unproject()`/`Project()` are for picking and gizmo placement — `Gizmo`
(`include/ImFrame/Rendering/Gizmo.hpp`) is built on exactly this API.

## Choosing between them

- Need custom-rendered pixels with no scene semantics (a live camera feed, a
  procedurally generated texture)? Use **`Viewport`** directly.
- Building a 2D diagram/graph/map editor with pan, zoom, and clickable
  elements? Use **`Canvas2D`**.
- Building a 3D scene view, model preview, or level editor? Use
  **`Viewport3D`**.
