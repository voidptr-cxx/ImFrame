# Widget Reference

Every type below satisfies either the `Tree::Component` concept (has
`Build() const`) or `Tree::PrimitiveWidget` concept (has `CreateElement()
const`) — see [Architecture Overview](Architecture.md) for what
distinguishes them. All are usable anywhere a `Tree::Widget` is expected:
`Tree::Widget`'s converting constructor is non-`explicit`, so you never wrap
a value in `Widget(...)` — pass the concrete type directly.

## Core layout primitives (`Tree::Primitives`)

The eight types every other widget in this document is ultimately composed
from.

### `Text`

Leaf text with color, size, wrap, and alignment.

```cpp
Text("Hello, ImFrame!").Color({1, 1, 1, 1}).Align(Widgets::TextAlign::Center);
```

`.Content(sv)`, `.Color(Vec4)`, `.FontSize(float)` (`0` = current ImGui font's
native size), `.Wrap(bool = true)`, `.Align(TextAlign)`.

### `Box`

Sized, padded, optionally decorated container holding zero or one child —
the fundamental building block every other container composes with.

```cpp
Box()
    .Padding(Widgets::EdgeInsets::All(8.0f))
    .Background({0.15f, 0.15f, 0.18f, 1.0f})
    .Radius(4.0f)
    .Child(Text("Hello"));
```

`.Width/.Height/.MinWidth/.MinHeight/.MaxWidth/.MaxHeight(float)`,
`.Padding(EdgeInsets)`, `.Background(Vec4)`, `.BorderColor(Vec4)`,
`.BorderWidth(float)`, `.Radius(float)`, `.Child(Widget)`.

### `Flex`

Row or column layout — the replacement for the deprecated `HStack`/`VStack`.

```cpp
Flex(Flex::Axis::Horizontal).Gap(4.0f).Children({
    Text("Left"), Spacer(), Text("Right"),
});
```

`Flex(Axis = Horizontal)`, `.MainAlign(MainAlignment)`,
`.CrossAlign(CrossAlignment)`, `.Gap(float)`, `.Children(vector<Widget>)`.

### `Expanded`

Wraps a child so it claims a proportional share of its parent `Flex`'s
remaining space (like Flutter's `Expanded`).

```cpp
Flex(Flex::Axis::Horizontal).Children({
    ButtonWidget("Fixed"),
    Expanded(SliderWidget<float>("Fill remaining", &v, 0.0f, 1.0f)),
});
```

`Expanded(Widget child)`, `.Factor(int)` (relative share among sibling
`Expanded`s).

### `SizedBox`

Reserves an exact `Width × Height`, draws nothing.

```cpp
SizedBox().Width(16.0f).Height(16.0f);
```

### `Spacer`

Flex-only invisible filler that pushes siblings apart — see the `Flex`
example above.

### `GestureRegion`

Input detection with no visuals of its own — the composition primitive
every interactive widget below is built on.

```cpp
GestureRegion()
    .OnClick([] { /* ... */ })
    .OnHover([](bool hovering) { /* ... */ })
    .Child(Text("Click me"));
```

`.OnClick`, `.OnDoubleClick`, `.OnHover(Delegate<void(bool)>)`,
`.OnDragStart/OnDragMove(Delegate<void(Vec2)>)/OnDragEnd`,
`.OnScroll(Delegate<void(Vec2)>)`, `.Child(Widget)`.

### `Stack`

Z-axis layering — each child gets the `Stack`'s own constraints, positioned
at top-left; later children paint on top of earlier ones. No layout
negotiation between children.

```cpp
Stack().Children({ Box().Background({0,0,0,1}), Text("Overlaid") });
```

## Other core primitives (`Tree::`, not `Primitives`-namespaced)

### `Portal`

Renders its child at the element-tree root, escaping the parent's clip
rect — used for popups/toasts/anything that must draw outside its
structural position. Occupies zero space where it's placed; a per-frame
registry drains and paints portals last (always on top).

```cpp
Portal(Box().Background({0, 0, 0, 0.5f}).Child(Text("On top!")));
```

### `VirtualList`

Virtualized fixed-row-height scrolling list — only visible rows are built,
computed from live scroll position. `TableWidget` is built on this.

```cpp
VirtualList(10000, 24.0f, [](int index) -> Widget {
    return Text(std::format("Row {}", index));
});
```

### `InheritedWidget<T>`

Scoped data provider — see the
[State Management Guide](StateManagement.md#inheritedwidgett) for the full
explanation and example.

## Interactive widgets (`Widgets::`)

Every interactive widget binds to caller-owned state via a **raw pointer**,
not a reference — `ComponentElement<T>`/every primitive `Element` reconciles
by copy-assigning the incoming widget config, which requires the type to be
copy-assignable. `value` (and, for `ComboWidget<T>`, the `items` span) must
outlive every `Element` mounted from the widget.

### `ButtonWidget`

```cpp
ButtonWidget("Save").OnClick([&] { Save(); });
```
`explicit ButtonWidget(std::string label)`, `.Icon(string)`, `.Size(Vec2)`,
`.OnClick(Delegate<void()>)`, `.Disabled(bool = true)`, `.Tooltip(string)`,
`.Width(float)`.

### `CheckboxWidget`

```cpp
CheckboxWidget("Wireframe", &wireframe).OnChange([](bool v) { /* ... */ });
```
`CheckboxWidget(string label, bool* value)`, `.OnChange(Delegate<void(bool)>)`.

### `SliderWidget<T>`

```cpp
SliderWidget<float>("Volume", &volume, 0.0f, 1.0f)
    .Format("%.2f")
    .OnChange([](float v) { ApplyVolume(v); });
```
`SliderWidget(string label, T* value, T min, T max)` — `T` must satisfy
`std::is_arithmetic_v`. `.Format(string)`, `.OnChange(Delegate<void(T)>)`.

### `TextInputWidget<T>`

```cpp
TextInputWidget<std::string>("Name", &name).Hint("Enter your name");
```
`TextInputWidget(string label, T* value)` — `T` is `std::string` or
`std::u8string`. `.Hint(string)`, `.Multiline(bool = true)`,
`.Password(bool = true)`, `.OnChange(Delegate<void(const T&)>)`.

### `ComboWidget<T>`

```cpp
static constexpr std::array<int, 3> modes{0, 1, 2};
ComboWidget<int>("Mode", &mode, std::span<const int>(modes));
```
`ComboWidget(string label, T* selected, std::span<const T> items)` —
`items` is caller-owned and must outlive the widget. `.ItemLabel(Delegate<
string(const T&)>)` (optional custom formatting), `.OnChange(Delegate<
void(const T&)>)`.

### `ProgressBarWidget`

Stateless.

```cpp
ProgressBarWidget(loadProgress).Overlay("Loading assets...");
```
`explicit ProgressBarWidget(float fraction) noexcept`, `.Size(Vec2)`,
`.Overlay(string)`.

### `ColorEditWidget`

```cpp
ColorEditWidget("Tint", &tint).Alpha(true);
```
`ColorEditWidget(string label, Vec4* value)`, `.Alpha(bool = true)`,
`.OnChange(Delegate<void(Vec4)>)`.

### `RadioWidget`

No `OnChange` — multiple `RadioWidget`s bound to the same `int*` form a
group; the widget sets `*value = option` on click (matching
`ImGui::RadioButton`'s own semantics).

```cpp
int mode = 0;
RadioWidget("Linear",  &mode, 0);
RadioWidget("Nearest", &mode, 1);
```
`RadioWidget(string label, int* value, int option)`.

### `SeparatorWidget`

Stateless.

```cpp
SeparatorWidget().Label("Advanced");
```

### `ImageWidget`

Calls `ImGui::Image()` when no `OnClick` is set, `ImGui::ImageButton()` when
one is.

```cpp
ImageWidget(myTex, {256.0f, 256.0f}).Tint({1, 1, 1, 0.8f});
ImageWidget(iconTex, {32.0f, 32.0f}).OnClick([&] { DoAction(); });
```

## Composite widgets

### `TableWidget`

Built on `VirtualList` — column definitions with a per-cell text callback.

```cpp
TableWidget(rows.size(), 24.0f)
    .Column("Name",  [&](int r) { return rows[r].name; })
    .Column("Score", [&](int r) { return std::to_string(rows[r].score); })
    .OnRowClick([&](int r) { selectedRow = r; });
```

Not yet covered: sort-state tracking, per-row context menus, striped rows,
Fixed/Stretch/Auto column width modes.

### `PropertyGridWidget`

Takes the full row list up front (no imperative `Begin()`/`Row()` scope —
`Build()` runs before `Layout()`, so there's no way to interleave calls).

```cpp
PropertyGridWidget("##props")
    .SplitRatio(0.4f)
    .Rows({
        {"Name", TextInputWidget<std::string>("##name", &name)},
        PropertyGridRow::Separator("Transform"),
        {"Position", SliderWidget<float>("##x", &posX, -10.0f, 10.0f)},
    });
```

### `Layout::GridWidget`

Pure layout math — no ImGui table involved (unlike the deprecated `Grid`).
Children fill columns left-to-right, wrapping into a new row every
`Columns()` children. Distinct from `Widgets::TableWidget`/`PropertyGridWidget`
— easy to confuse by name, very different purpose (fixed N-column grid vs.
data table vs. label/value rows).

```cpp
Layout::GridWidget(3).Spacing(4.0f).Children({
    ButtonWidget("A"), ButtonWidget("B"),
    ButtonWidget("C"), ButtonWidget("D"), // wraps to row 2
});
```

## Overlays (`Overlay::`)

### `ModalWidget`

Binds to a caller-owned `bool* open`; the widget clears it on any close path
(× button, Escape, click-outside).

```cpp
bool showSettings = false;
ButtonWidget("Settings").OnClick([&] { showSettings = true; });
Overlay::ModalWidget("Settings", &showSettings)
    .Content(Text("Settings content"));
```

### `ContextMenuWidget`

Wraps a trigger child; right-click opens the menu.

```cpp
Overlay::ContextMenuWidget(Text("file.txt"))
    .Item("Open", [] { OpenFile(); })
    .Item("Delete", [] { DeleteFile(); });
```

### `ToastOverlayWidget`

Stateless — takes a snapshot of pending toasts and composes them into a
`Portal`-wrapped stack. Use either this **or** the automatic
`Application::RunOneFrame()` toast-render path, never both (toasts would
render twice).

```cpp
Flex(Flex::Axis::Vertical).Children({
    MainContent{},
    Overlay::ToastOverlayWidget(Overlay::ToastManager::Instance().Snapshot()),
});
```

## Rendering surfaces

`Rendering::ViewportWidget`, `Canvas2DWidget`, and `Viewport3DWidget` wrap
`Viewport`/`Canvas2D`/`Viewport3D` by raw pointer for custom-rendered
content — see the dedicated [Rendering Surfaces Guide](RenderingSurfaces.md).

## App-level

### `App::DockSpaceWidget`

Opt-in, stateless dockspace host window, for placing directly inside a
`Build()` tree (typically via `SetRoot()`) rather than relying on
`Application`'s automatic dockspace. No content slot — `ImGui::DockSpace()`
consumes the whole host window, so pair this with separately-opened
dockable windows. No named-layout persistence (use `App::DockSpace` via
`Application::GetDockSpace()` for that).

```cpp
struct AppRoot {
    [[nodiscard]] Tree::Widget Build() const { return App::DockSpaceWidget(); }
};
```
