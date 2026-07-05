# Migration Guide: v1.x Imperative Widgets → v2.x Widget Tree

Phase 29 reimplements the Phase 10–14 widget library as `Tree::Component`/
`Tree::PrimitiveWidget` types built from the Phase 27 primitives (`Box`, `Flex`,
`Text`, `GestureRegion`, `SizedBox`, `Expanded`, `Spacer`, `Stack`) plus two new
ones added this phase (`Portal`, `VirtualList`). The old `Show()`-builder API
is marked `[[deprecated]]` — it still compiles and works, but new code should
use the declarative API described here. **The old API is removed in Phase 30.**

The visual output is identical: the reimplemented widgets call the same
underlying ImGui functions, just reached through a `Build()` method instead of
an imperative `Show()` call site.

---

## Core shape of the migration

**Old (imperative, v1.x):**

```cpp
void OnUi() {
    Widgets::Button("Save").OnClick([&] { Save(); }).Show();
}
```

**New (declarative, v2.x):**

```cpp
struct MyRoot {
    [[nodiscard]] Tree::Widget Build() const {
        return Tree::Widget(Widgets::ButtonWidget("Save").OnClick([&] { Save(); }));
    }
};

// once, at startup:
app.SetRoot(root);
```

Every frame, `root.Build()` is called and reconciled against the previous
tree — there is no `OnUi()` call site to sprinkle `Show()` calls into anymore.

---

## Naming convention

Every reimplemented widget keeps its old name plus a **`Widget`** suffix, in
the same header and namespace as its deprecated predecessor:

| Old (deprecated) | New |
|---|---|
| `ImFrame::Widgets::Button` | `ImFrame::Widgets::ButtonWidget` |
| `ImFrame::Widgets::Checkbox` | `ImFrame::Widgets::CheckboxWidget` |
| `ImFrame::Widgets::Slider<T>` | `ImFrame::Widgets::SliderWidget<T>` |
| `ImFrame::Widgets::TextInput<T>` | `ImFrame::Widgets::TextInputWidget<T>` |
| `ImFrame::Widgets::Combo<T>` | `ImFrame::Widgets::ComboWidget<T>` |
| `ImFrame::Widgets::Table` | `ImFrame::Widgets::TableWidget` |
| `ImFrame::Overlay::Modal` / `ConfirmModal` | `ImFrame::Overlay::ModalWidget` |
| `ImFrame::Overlay::ContextMenu` | `ImFrame::Overlay::ContextMenuWidget` |
| `ImFrame::Overlay::ToastManager::Render()` | `ImFrame::Overlay::ToastOverlayWidget` (opt-in, coexists with the old automatic render path) |
| `ImFrame::Rendering::Viewport` | wrap in `ImFrame::Rendering::ViewportWidget` |
| `ImFrame::Rendering::Canvas2D` | wrap in `ImFrame::Rendering::Canvas2DWidget` |
| `ImFrame::Rendering::Viewport3D` | wrap in `ImFrame::Rendering::Viewport3DWidget` |
| `ImFrame::Widgets::Text` | `ImFrame::Tree::Primitives::Text` (Phase 27, no suffix — different namespace) |
| `ImFrame::Widgets::Spacer` | `ImFrame::Tree::Primitives::SizedBox` (fixed) or `Tree::Primitives::Spacer` (flexible) |
| `ImFrame::Widgets::Separator` | `Tree::Primitives::Box` with a thin `Height`/`Width` and `Background` |
| `ImFrame::Layout::Panel` | `ImFrame::Tree::Primitives::Box` |
| `ImFrame::Layout::HStack` | `Tree::Primitives::Flex(Flex::Axis::Horizontal)` |
| `ImFrame::Layout::VStack` | `Tree::Primitives::Flex(Flex::Axis::Vertical)` |
| `ImFrame::Layout::ScrollArea` | `Tree::VirtualList` (for large scrolling lists) |
| `ImFrame::Widgets::Radio`, `ColorEdit`, `Image`, `ProgressBar`, `PropertyGrid`, `Grid` | **not yet reimplemented** — remain deprecated with no Component equivalent this phase; keep using the old API for these until a future phase adds one |

---

## Binding pattern: pointer, not reference

Every old `Show()`-style widget bound to caller-owned state via a **reference**
(`bool& value`). The new widgets bind via a **raw pointer** (`bool* value`)
instead:

```cpp
// Old
bool wireframe = false;
Widgets::Checkbox("Wireframe", wireframe).Show();

// New
bool wireframe = false;
Widget(Widgets::CheckboxWidget("Wireframe", &wireframe));
```

This is not a style preference — it's required. `ComponentElement<T>` and
every primitive `Element` reconcile by doing `_config = newWidget.As<T>();`
(a copy-assignment). A type with a reference member has an implicitly deleted
copy-assignment operator, so it could never satisfy that reconciliation step.
`value` (and, for `ComboWidget<T>`, the `items` span) must outlive every
`Element` mounted from the widget — typically the lifetime of the enclosing
component or application.

---

## Per-widget notes

### ButtonWidget

No behavioral changes. ImGui's own style system (`ImGuiCol_Button` /
`ButtonHovered` / `ButtonActive`) already handles the hover/press visual
inside the single `ImGui::Button()` call — there is no separate hover-state
Component to manage.

```cpp
Widget(Widgets::ButtonWidget("Save").OnClick([&] { Save(); }));
```

### CheckboxWidget / SliderWidget\<T\> / TextInputWidget\<T\>

Same options as before (`Format`, `Hint`, `Multiline`, `Password`, `Tooltip`,
`Width`, `Disabled`), plus an `OnChange` delegate. `SliderWidget<T>` gained
`OnChange` — the old `Slider<T>` had no callback, only a `bool` return value
from `Show()`, which has no equivalent in a declarative `Build()`.

```cpp
Widget(Widgets::SliderWidget<float>("Volume", &volume, 0.0f, 1.0f)
           .Format("%.2f")
           .OnChange([](float v) { ApplyVolume(v); }));
```

### ComboWidget\<T\>

Unchanged in spirit — still opens ImGui's native combo popup
(`BeginCombo`/`Selectable`/`EndCombo`), which already renders on its own
overlay layer independent of the parent window's clip rect. `Portal` is not
needed here.

### TableWidget

Reimplemented on `Tree::VirtualList` instead of `ImGuiListClipper` — the
proposal's headline capability for this phase. Current scope: column
definitions with a per-cell text callback, and `OnRowClick` for selection.

**Not yet covered** (use the deprecated `Table` for these): sort-state
tracking, right-click context menus per row, striped rows, and
Fixed/Stretch/Auto column width modes.

```cpp
Widget(Widgets::TableWidget(rows.size(), 24.0f)
           .Column("Name",  [&](int r) { return rows[r].name; })
           .Column("Score", [&](int r) { return std::to_string(rows[r].score); })
           .OnRowClick([&](int r) { selectedRow = r; }));
```

### ModalWidget

Binds to a caller-owned `bool* open` instead of an internal `_pendingOpen`
flag toggled by `Open()`/`Close()`. Set `*open = true` (e.g. from a
`ButtonWidget::OnClick`) to trigger opening; the widget clears it back to
`false` on any close path (× button, Escape, click-outside). Wraps the same
`ImGui::BeginPopupModal()`/`EndPopupModal()` pair as `Modal::Begin()` — native
ImGui modals already escape parent clipping, so `Portal` is not used here.
`ConfirmModal`'s two-button pattern isn't reimplemented separately; compose it
from `ModalWidget::Content()` plus two `ButtonWidget`s.

```cpp
bool showSettings = false;
// ...
Widget(Widgets::ButtonWidget("Settings").OnClick([&] { showSettings = true; }));
Widget(Overlay::ModalWidget("Settings", &showSettings)
           .Content(Widget(Tree::Primitives::Text("Settings content"))));
```

### ContextMenuWidget

Wraps a trigger child `Widget`; right-clicking it opens the menu via
`ImGui::BeginPopupContextItem()` — the same native mechanism `ContextMenu::Show()`
used. `Portal` is not needed for the same reason as `ModalWidget`.

```cpp
Widget(Overlay::ContextMenuWidget(Widget(Tree::Primitives::Text("file.txt")))
           .Item("Open", [] { OpenFile(); })
           .Item("Delete", [] { DeleteFile(); }));
```

### ToastOverlayWidget

The one overlay that genuinely needed `Portal` — `ToastManager::Render()`
draws directly via `ImGui::GetForegroundDrawList()`, with no native popup
mechanism to lean on. `ToastOverlayWidget` is **stateless**: it takes a
snapshot of the toasts to render (`ToastManager::Instance().Snapshot()`,
reusing the existing queue/fade logic unchanged) and composes them into a
`Portal`-wrapped stack. Being stateless sidesteps the "parent rebuild resets
embedded `State<T>`" pitfall entirely (see below) — there is no persisted
state inside the widget to lose.

`ToastManager::Add()` and the `ToastInfo`/`ToastSuccess`/`ToastWarning`/`ToastError`
free functions are **not deprecated** — they only enqueue a toast and are
orthogonal to how it renders. Use either the automatic
`Application::RunOneFrame()` → `ToastManager::Instance().Render(dt)` path
(unchanged, still the default) **or** place a `ToastOverlayWidget` in your
tree — not both, or toasts render twice.

```cpp
struct MyRoot {
    [[nodiscard]] Widget Build() const {
        return Widget(Tree::Primitives::Flex(Flex::Axis::Vertical).Children({
            Widget(MainContent{}),
            Widget(Overlay::ToastOverlayWidget(Overlay::ToastManager::Instance().Snapshot())),
        }));
    }
};
```

### ViewportWidget / Canvas2DWidget / Viewport3DWidget

Thin declarative wrappers — `Viewport`/`Canvas2D`/`Viewport3D` still own their
framebuffer and register with `Internal::ViewportRegistry` exactly as before.
Bind via a raw pointer (these types are non-copyable, same reasoning as
`CheckboxWidget`'s pointer binding above).

```cpp
Rendering::Viewport scene("3d_view");
scene.OnRender([](const RenderContext& ctx) { /* ... */ });
// In Build():
return Widget(Rendering::ViewportWidget(&scene));
```

---

## A note on internal component state

If you write your own stateful `Component` (using `Tree::State<T>` as a
member), be aware: `ComponentElement<T>::Update()` unconditionally overwrites
its stored component from the incoming `Widget` every frame. If your
**parent** component rebuilds (for any reason) and constructs a **fresh**
value of your component type — rather than reusing a persistent instance it
already held — any `State<T>` inside that fresh value starts over from its
default. This is why every reimplemented interactive widget in this phase
(`ButtonWidget`, `CheckboxWidget`, etc.) either has no internal state at all,
or binds to state the *caller* owns (a pointer), rather than owning transient
interaction state itself.

---

## What's still deprecated with no replacement (yet)

`Radio`, `ColorEdit`, `Image`, `ProgressBar`, `PropertyGrid`, and `Grid` are
marked `[[deprecated]]` per the Phase 29 blanket policy but have no
`Tree::Component` equivalent in this phase. Keep using them as-is; a future
phase will provide replacements before Phase 30 removes the old API entirely.
