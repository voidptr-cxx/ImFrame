# Migration Guide: v1.x Imperative Widgets → v2.x Widget Tree

Phase 29 reimplemented the Phase 10–14 widget library as `Tree::Component`/
`Tree::PrimitiveWidget` types built from the Phase 27 primitives (`Box`, `Flex`,
`Text`, `GestureRegion`, `SizedBox`, `Expanded`, `Spacer`, `Stack`) plus two new
ones added that phase (`Portal`, `VirtualList`). Phase 30.1 completed the set
by reimplementing the remaining seven widgets that Phase 29 left deprecated
with no replacement (`Separator`, `Image`, `ProgressBar`, `ColorEdit`, `Radio`,
`PropertyGrid`, `Grid`). **Phase 30.2 removed the old `Show()`-builder API
entirely** — this guide is now a historical record of the migration; there is
no deprecated fallback left to compile against.

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
        return Widgets::ButtonWidget("Save").OnClick([&] { Save(); });
    }
};

// once, at startup:
app.SetRoot(root);
```

Every frame, `root.Build()` is called and reconciled against the previous
tree — there is no `OnUi()` call site to sprinkle `Show()` calls into anymore.

`Tree::Widget`'s converting constructor is deliberately non-`explicit`, so any
primitive or `Component` implicitly converts to `Widget` wherever one is
expected — a `Build()` return value, a `std::vector<Widget>` initializer-list
element (e.g. `Flex::Children({...})`), or a single-`Widget` setter (e.g.
`Box::Child(...)`). None of the examples in this guide wrap values in an
explicit `Widget(...)`/`Tree::Widget(...)` call; write the concrete type
directly.

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
| `ImFrame::Widgets::Separator` | `ImFrame::Widgets::SeparatorWidget` (Phase 30.1) |
| `ImFrame::Widgets::Image` | `ImFrame::Widgets::ImageWidget` (Phase 30.1) |
| `ImFrame::Widgets::ProgressBar` | `ImFrame::Widgets::ProgressBarWidget` (Phase 30.1) |
| `ImFrame::Widgets::ColorEdit` | `ImFrame::Widgets::ColorEditWidget` (Phase 30.1) |
| `ImFrame::Widgets::Radio` | `ImFrame::Widgets::RadioWidget` (Phase 30.1) |
| `ImFrame::Widgets::PropertyGrid` | `ImFrame::Widgets::PropertyGridWidget` (Phase 30.1) |
| `ImFrame::Layout::Panel` | `ImFrame::Tree::Primitives::Box` |
| `ImFrame::Layout::HStack` | `Tree::Primitives::Flex(Flex::Axis::Horizontal)` |
| `ImFrame::Layout::VStack` | `Tree::Primitives::Flex(Flex::Axis::Vertical)` |
| `ImFrame::Layout::ScrollArea` | `Tree::VirtualList` (for large scrolling lists) |
| `ImFrame::Layout::Grid` | `ImFrame::Layout::GridWidget` (Phase 30.1) |

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
Widgets::CheckboxWidget("Wireframe", &wireframe);
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
Widgets::ButtonWidget("Save").OnClick([&] { Save(); });
```

### CheckboxWidget / SliderWidget\<T\> / TextInputWidget\<T\>

Same options as before (`Format`, `Hint`, `Multiline`, `Password`, `Tooltip`,
`Width`, `Disabled`), plus an `OnChange` delegate. `SliderWidget<T>` gained
`OnChange` — the old `Slider<T>` had no callback, only a `bool` return value
from `Show()`, which has no equivalent in a declarative `Build()`.

```cpp
Widgets::SliderWidget<float>("Volume", &volume, 0.0f, 1.0f)
    .Format("%.2f")
    .OnChange([](float v) { ApplyVolume(v); });
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
Widgets::TableWidget(rows.size(), 24.0f)
    .Column("Name",  [&](int r) { return rows[r].name; })
    .Column("Score", [&](int r) { return std::to_string(rows[r].score); })
    .OnRowClick([&](int r) { selectedRow = r; });
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
Widgets::ButtonWidget("Settings").OnClick([&] { showSettings = true; });
Overlay::ModalWidget("Settings", &showSettings)
    .Content(Tree::Primitives::Text("Settings content"));
```

### ContextMenuWidget

Wraps a trigger child `Widget`; right-clicking it opens the menu via
`ImGui::BeginPopupContextItem()` — the same native mechanism `ContextMenu::Show()`
used. `Portal` is not needed for the same reason as `ModalWidget`.

```cpp
Overlay::ContextMenuWidget(Tree::Primitives::Text("file.txt"))
    .Item("Open", [] { OpenFile(); })
    .Item("Delete", [] { DeleteFile(); });
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
        return Tree::Primitives::Flex(Flex::Axis::Vertical).Children({
            MainContent{},
            Overlay::ToastOverlayWidget(Overlay::ToastManager::Instance().Snapshot()),
        });
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
return Rendering::ViewportWidget(&scene);
```

### SeparatorWidget / ProgressBarWidget

Stateless, no behavioral changes. Same `ImGui::SeparatorText()`/`ImGui::Separator()`
and `ImGui::ProgressBar()` calls as the deprecated classes.

```cpp
SeparatorWidget().Label("Advanced");
ProgressBarWidget(loadProgress).Overlay("Loading assets...");
```

### ImageWidget

Collapses the old dual `Show()`/`ShowButton()` API into one widget: calls
`ImGui::Image()` when no `OnClick` is set, or `ImGui::ImageButton()` when one
is — the same OnClick-gated pattern `ButtonWidget`/`CheckboxWidget` already
use instead of a separate call path.

```cpp
ImageWidget(myTex, {256.0f, 256.0f}).Tint({1, 1, 1, 0.8f});
ImageWidget(iconTex, {32.0f, 32.0f}).OnClick([&] { DoAction(); });
```

### ColorEditWidget / RadioWidget

Same pointer-binding pattern as `CheckboxWidget` (`Vec4*` / `int*`). `RadioWidget`
has no `OnChange` — same as the old `Radio`, which only returned a `bool` from
`Show()`; multiple `RadioWidget`s bound to the same `int*` form a group.

```cpp
Vec4 tint{1.0f, 0.5f, 0.0f, 1.0f};
ColorEditWidget("Tint", &tint).Alpha(true);

int mode = 0;
RadioWidget("Linear",  &mode, 0);
RadioWidget("Nearest", &mode, 1);
```

### GridWidget

Unlike the deprecated `Grid` (backed by `ImGui::BeginTable`), `GridWidget` is
pure layout math — no ImGui table involved, the same architectural choice
already made when `HStack`/`VStack` were replaced by `Flex`. Children fill
columns left-to-right, wrapping into a new row every `Columns()` children.

```cpp
Layout::GridWidget(3).Spacing(4.0f).Children({
    ButtonWidget("A"), ButtonWidget("B"),
    ButtonWidget("C"), ButtonWidget("D"), // wraps to row 2
});
```

### PropertyGridWidget

The imperative `Begin()`/`Row()`/`Separator()` scope pattern has no
declarative analogue, so `PropertyGridWidget` takes the full row list
up front via `Rows()` instead of being called into row-by-row. `SplitRatio`
is expressed as a pair of `Flex` factors (`ratio*100` / `(1-ratio)*100`)
rather than a fixed pixel width, since `Build()` runs before any `Layout()`
pass has constraint information — `Flex`'s existing proportional-width math
does the rest. Purely composed from `Flex`/`Expanded`/`Text`/`SeparatorWidget`;
no new primitive or `Element` was written for it.

```cpp
PropertyGridWidget("##props")
    .SplitRatio(0.4f)
    .Rows({
        {"Name", TextInputWidget<std::string>("##name", &name)},
        PropertyGridRow::Separator("Transform"),
        {"Position", SliderWidget<float>("##x", &posX, -10.0f, 10.0f)},
    });
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

## Status: the Phase 10–14 API has been fully removed

Phase 30.1 gave every deprecated Phase 10–14 widget a `Tree::Component`/
`Tree::PrimitiveWidget` equivalent. Phase 30.2 then deleted the deprecated
headers entirely, along with the 20 test files that exercised them directly
and `PropertyGridScope`/`Widgets::Renderable` (both orphaned once
`PropertyGrid`'s `Begin()`/`Row()`/`Separator()` scope pattern was removed —
`PropertyGridWidget` never used either). `Examples/DemoApp/main.cpp` was
migrated to the declarative API in the same phase, via a small local
`TreePanel` helper (see its file comment) that drives one `Tree::Element`'s
Mount/Update/Layout/Paint cycle inside each of the demo's six independently
dockable panels — `Application::SetRoot()` isn't used there because it only
supports a single fixed, non-dockable root window.
