# Architecture Overview

ImFrame is built in three layers. Each layer only depends on the one below
it; nothing in a lower layer ever depends on ImGui types or on the layer
above it.

```
┌─────────────────────────────────────────────────────────────────┐
│  (c) Public widget/layout/theme library                         │
│      Widgets::ButtonWidget, CheckboxWidget, TableWidget, ...     │
│      Layout::GridWidget · Theme::Theme · Themes::Dracula, ...    │
│      — Components composed from the 8 core primitives below      │
├─────────────────────────────────────────────────────────────────┤
│  (b) Tree core                                                  │
│      Tree::Widget (immutable description) · Tree::Element        │
│      (live, stateful node) · Internal::Reconciler · State/Signal │
│      /Computed/InheritedWidget (reactive data) · Rendering::      │
│      CommandBuffer + Internal::IRenderer/ImGuiCompatRenderer      │
│      (Box/Text only — everything else still calls ImGui directly)│
├─────────────────────────────────────────────────────────────────┤
│  (a) Backend abstraction                                        │
│      Internal::IBackend · WindowConfig · NativeGraphicsContext   │
│      One concrete implementation per platform: GLFWOpenGL3,      │
│      SDL3Vulkan, SDL3Metal, SDL3DX12, DawnWebGPU, Headless        │
└─────────────────────────────────────────────────────────────────┘
```

## (a) Backend abstraction

`include/ImFrame/Backends/BackendInfo.hpp` declares `Internal::IBackend` — a
pure interface with one required method set (`Init`/`Poll`/`BeginFrame`/
`EndFrame`/`Shutdown`/`NativeHandle`/`CancelClose`) and several optional
overrides with stub defaults (DPI scale, window size, multi-window,
input-event draining, native graphics context, viewport framebuffers).

Every platform gets exactly one concrete `IBackend` implementation, built as
its own CMake static library target (`Backends/<Name>/`):

| Backend | Target |
|---|---|
| GLFW + OpenGL 3.3 | `ImFrame_GLFWOpenGL3` |
| SDL3 + Vulkan | `ImFrame_SDL3Vulkan` |
| SDL3 + Metal | `ImFrame_SDL3Metal` |
| SDL3 + DX12 | `ImFrame_SDL3DX12` |
| Dawn WebGPU | `ImFrame_DawnWebGPU` |
| Headless (no window) | built into `ImFrame` core, for tests |

No ImGui or platform header (GLFW/SDL/Vulkan/etc.) ever appears in a public
`include/ImFrame/` header — `Backends/<Name>/` is the only place those
includes are permitted (see [Backend Guide](BackendGuide.md) for how a
consumer application selects one).

## (b) Tree core

This is the layer that gives ImFrame its declarative model.

- **`Tree::Widget`** (`Tree/Widget.hpp`) — an immutable, cheaply-copyable,
  type-erased *description* of one node. Wraps a `PrimitiveWidget` or
  `Component` value in a `shared_ptr<const Internal::WidgetConcept>`: one
  allocation when it's built, copies are just refcount bumps. Key operations:
  `CreateElement()`, `CanUpdate(other)`, `As<T>()`.
- **`Tree::Element`** (`Tree/Element.hpp`) — the abstract, stateful,
  *persists-across-frames* counterpart. `Mount()`/`Update()`/`Unmount()` drive
  its lifecycle; `Layout(BoxConstraints) -> Vec2` and `Paint(Rendering::
  CommandBuffer&, position)` drive measurement and drawing, using the same
  single-pass "parent narrows, child returns size" protocol Flutter uses.
- **`Component` concept** — any type with `Build() const` returning something
  convertible to `Widget`. Not a base class; no virtual dispatch, no
  allocation beyond the one `Widget` the call produces.
- **`PrimitiveWidget` concept** — any type with `CreateElement() const ->
  unique_ptr<Element>`. Satisfied by the eight `Tree::Primitives` types
  (`Box`, `Expanded`, `Flex`, `GestureRegion`, `SizedBox`, `Spacer`, `Stack`,
  `Text`) plus a handful of others that implement it directly (`Tree::Portal`,
  `Tree::VirtualList`, `Tree::InheritedWidget<T>`, `App::DockSpaceWidget`,
  `Layout::GridWidget`, the `Rendering::*Widget` wrappers).
- **`Internal::Reconciler`** (`src/Tree/Reconciler.hpp`, not a public header)
  — owns the single root `Element` plus a reused `Rendering::CommandBuffer`
  and a fixed `Internal::ImGuiCompatRenderer`. Its one method, `Show(rootWidget)`,
  reconciles the freshly-built root against the existing element (update in
  place if `CanUpdate()`, otherwise destroy and recreate), runs one `Layout()`
  pass, resets the buffer, runs `Paint()` (and drains/paints any `Portal`s
  into the same buffer), then replays the whole frame's recorded commands
  through the renderer once — all inside a dedicated host window.
- **`Rendering::CommandBuffer` / `Internal::IRenderer`** (Phase 31) — an
  abstract drawing language sitting between `Element::Paint()` and ImGui.
  `Box`/`Text` are the only two primitives whose `Paint()` records `Command`
  values (`DrawRect`/`DrawText`/...) instead of calling ImGui directly;
  `Internal::ImGuiCompatRenderer` (the only `IRenderer` implementation today)
  translates the buffer to real `ImDrawList` calls once per frame. Every
  other `Element` — including `GestureRegion`, `VirtualList`, and every
  `Widgets::*`/`Overlay::*` interactive widget's own `Element` — still calls
  ImGui directly, because their drawing is inseparable from ImGui's own
  hit-testing/window/scroll state (e.g. `ImGui::Button()` handles click
  detection and drawing as one opaque call). This is a deliberate, narrow
  migration, not a partial one left incomplete — see `.claude/DECISIONS.md`
  (Phase 31.1–31.3) for the full survey findings.
- **`RenderObjects` layer** (`src/Tree/RenderObjects/*.cpp`, plus every
  `Element` in `src/Widgets/*.cpp`/`src/Overlay/*.cpp`/`src/Rendering/*.cpp`)
  — every concrete `Element` subclass lives here, in `ImFrame::Internal::`.
  A primitive's public header never includes `<imgui.h>`, it only declares
  `CreateElement()` and documents "defined in `XxxRO.cpp`". Since Phase 31,
  `src/Rendering/Renderers/ImGuiCompatRenderer.cpp` is a second, equally
  legitimate ImGui call site — it exists specifically to be where `Box`/
  `Text`'s recorded commands turn into real draw calls.
- **Reactive state** — `State<T>`, `Signal<T>`, `Computed<T>`, and
  `InheritedWidget<T>` all share one underlying mechanism: a
  `thread_local` "current dirty-registrar" installed around each
  `ComponentElement<T>::Rebuild()` call, and a `shared_ptr<atomic<bool>>`
  dirty flag per component instance. See the
  [State Management Guide](StateManagement.md) for the full picture.

## (c) Public widget/layout/theme library

Everything under `include/ImFrame/Widgets/`, `include/ImFrame/Layout/`, and
the theme engine sits on top of layer (b), but the two take different shapes:
`TableWidget` genuinely is a `Component` composed from `VirtualList` — it has
no `Element` of its own. Most interactive widgets (`ButtonWidget`,
`CheckboxWidget`, `SliderWidget<T>`, and the rest) are **not** compositions —
each defines its own `PrimitiveWidget`/`Element` pair in `src/Widgets/*.cpp`
that calls the matching native ImGui widget function directly (e.g.
`ButtonElement::Paint()` calls `ImGui::Button()`). This is deliberate: a
native ImGui widget call handles hit-testing, styling, and drawing as one
opaque unit, and `GestureRegion` has no way to express that without losing
the interaction logic — see `.claude/DECISIONS.md` (Phase 31.3) for why this
also means these widgets are not candidates for `CommandBuffer` migration.
See the [Widget Reference](WidgetReference.md) for the full catalog.

## Why this shape

The three-layer split exists so that:

1. **ImGui is a pure implementation detail.** A consumer application never
   needs `#include <imgui.h>` — the only ImGui dependency is in whichever
   backend library it links.
2. **Backends are interchangeable.** Since `IBackend` is the only contract
   between an `Application` and the platform, the same widget tree renders
   identically (module the [known conformance gap](../.claude/PHASE_STATUS.md))
   across OpenGL3, Vulkan, Metal, DX12, and WebGPU without any
   backend-specific code in your `Component`s.
3. **The reconciler stays simple.** Because `RenderObjects/` is the only code
   that touches ImGui, `Internal::Reconciler` and `Internal::ElementInternal`'s
   `ReconcileChildren`/`ReconcileChild` helpers are pure C++ — no ImGui state
   machine to reason about when auditing reconciliation correctness (see the
   Phase 30.5 reconciler performance audit in `.claude/DECISIONS.md`).
