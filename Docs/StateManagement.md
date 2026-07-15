# State Management Guide

ImFrame's widget tree is reactive: rather than manually calling a "rebuild"
function, you mutate one of four reactive-data types and the affected part
of the tree rebuilds automatically on the next frame. All four share the
same underlying mechanism — see [How it works](#how-it-works) below — but
each is suited to a different situation.

| Type | Use for | Thread safety |
|---|---|---|
| `Tree::State<T>` | Component-local mutable state (a counter, a text field's value, a toggle) | Render thread only |
| `Tree::Signal<T>` | A value that can be written from a background thread | Any thread |
| `Tree::Computed<T>` | A derived value that depends on one or more `Signal<T>`s | Any thread (read); recomputed lazily |
| `Tree::InheritedWidget<T>` | Data provided once, read by any number of descendants at any depth | Render thread only |

## `State<T>`

The everyday case: state owned by exactly one component, mutated in
response to user interaction.

```cpp
struct Counter {
    struct S { int count = 0; };
    Tree::State<S> state;

    [[nodiscard]] Tree::Widget Build() const {
        return Tree::Primitives::GestureRegion()
            .OnClick([s = state] { s.Set([](S& st) { ++st.count; }); })
            .Child(Tree::Primitives::Text(std::format("Count: {}", state.Get().count)));
    }
};
```

- `Get()` reads the current value. If called during a `Build()` pass, it also
  registers this component's "mark dirty" callback — so a later `Set()` knows
  which subtree to invalidate.
- `Set(mutator)` applies `mutator` synchronously, then fires the dirty
  callback (if one was registered).
- `T` must be a default-constructible aggregate — prefer a small nested
  struct (`Counter::S` above) over a bare scalar for anything that will grow.
- **Coalescing is automatic.** The dirty flag is a single `atomic<bool>`; any
  number of `Set()` calls between one frame's `Update()` and the next
  collapse into exactly one rebuild — you never need to batch calls
  yourself. (Verified directly in `Tests/Tree/ReconcilerPerformance_test.cpp`:
  1000 `Set()` calls in one frame produce exactly 1 additional `Build()`
  call.)
- `State<T>` is a `shared_ptr`-backed handle — copies (e.g. capturing `s =
  state` in a lambda) all reference the same underlying data. The block is
  freed when the last handle (including any captured-by-value lambda) goes
  out of scope.

**Pitfall:** `ComponentElement<T>::Update()` unconditionally overwrites its
stored component from the incoming `Widget` every frame. If your *parent*
component rebuilds and constructs a brand-new value of your component type
(rather than reusing a persistent instance), any `State<T>` inside that new
value starts over from its default — the old state is gone. This is why
every widget in the standard library either has no internal state, or binds
to state the *caller* owns via a pointer (see the
[Widget Reference](WidgetReference.md)).

## `Signal<T>`

For state that a background thread needs to write. `Tree::Signal<T>` is
distinct from `Utility::Signal<void(Args...)>` (a plain event observable,
unrelated) — this one wraps a value behind a mutex and is safe to assign
from any thread.

```cpp
struct Worker {
    Tree::Signal<float> progress;

    void RunOnBackground() {
        for (int i = 0; i <= 100; ++i) {
            progress = static_cast<float>(i) / 100.0f;   // any thread
        }
    }

    [[nodiscard]] Tree::Widget Build() const {
        return Widgets::ProgressBarWidget(progress.Get());
    }
};
```

- `Get()` reads the current value (thread-safe) and, if called during a
  `Build()` pass, registers the dirty callback — same as `State<T>::Get()`.
- `operator=(T)` writes a new value from any thread, fires the dirty
  callback, and notifies the permanent `OnChange` subscription used
  internally by `Computed<T>`.
- `OnChange(fn)` subscribes a permanent callback (returns a
  `Utility::Connection`) — mostly used internally, but available if you need
  to react to every write rather than polling `Get()`.

## `Computed<T>`

A lazily-evaluated value derived from one or more `Signal<T>` dependencies.
Recomputes only when read *after* a dependency has changed — not on every
dependency write.

```cpp
Tree::Signal<int>    width{800};
Tree::Signal<int>    height{600};
Tree::Computed<long> area{[&] { return static_cast<long>(width.Get()) * height.Get(); }, width, height};

// area.Value() == 480000
width = 1024;
// area.Value() == 614400 — recomputed on this read, not at the point width changed
```

`Value()` registers the calling component's dirty callback the same way
`Get()` does elsewhere — reading a `Computed<T>` inside `Build()` means your
component rebuilds whenever any of its dependencies change.

## `InheritedWidget<T>`

Scoped data provided once at an ancestor, read by `Context::Of<T>()` at any
descendant depth — the widget-tree equivalent of React Context or Flutter's
`InheritedWidget`.

```cpp
struct AppTheme { std::string fontName; };

struct ThemeConsumer {
    [[nodiscard]] Tree::Widget Build() const {
        if (const AppTheme* theme = Tree::Context::Of<AppTheme>()) {
            return Tree::Primitives::Text(theme->fontName);
        }
        return Tree::Primitives::Text("(no theme)");
    }
};

// Somewhere up the tree:
Tree::Widget root = Tree::InheritedWidget<AppTheme>(AppTheme{"Roboto"}, ThemeConsumer{});
```

- `Context::Of<T>()` returns `nullptr` if there's no ancestor providing `T`,
  or if called outside a `Build()` pass.
- Invalidation compares old vs. new value via `std::equality_comparable<T>`
  when `T` supports it (falls back to always-notify otherwise) — descendants
  that read an unchanged value don't rebuild.
- Like `State`/`Signal`, dependents are registered fresh on every `Build()`
  pass — there's no manual subscribe/unsubscribe to manage.

## How it works

All four types share one mechanism, declared in `Tree/Context.hpp`:

- A `thread_local` "current dirty registrar" (`Internal::g_stateRegistrar`),
  installed by `Internal::StateRegistrarScope` around every
  `ComponentElement<T>::Rebuild()` call. Calling `Get()`/`Value()` during
  `Build()` captures whatever registrar is currently installed.
- Each `ComponentElement<T>` owns a `shared_ptr<atomic<bool>>` dirty flag.
  `Update()` checks and clears it (`exchange(false)`); if it was set,
  `Rebuild()` runs — otherwise the previous `Build()` output is reused
  unchanged.

Because the flag is a single atomic bool rather than a counter or queue,
rebuild coalescing falls out of the design for free — see
`Tests/Tree/ReconcilerPerformance_test.cpp` for the test that verifies this
under stress (1000 `Set()` calls, 1 rebuild).

## Testing stateful components

`Tree::WidgetTestDriver<T>` (`include/ImFrame/Tree/WidgetTestDriver.hpp`)
drives a component's real `Mount()`/`Update()` lifecycle headlessly, so
`State<T>`/dirty-flag semantics work exactly as they do in a running app —
useful for asserting the kind of behaviour described above without spinning
up a backend:

```cpp
Tree::WidgetTestDriver driver{Counter{}};
driver.Build();
REQUIRE(driver.Component().state.Get().count == 0);

std::optional<Widgets::ButtonWidget> button =
    Tree::FindDescendant<Widgets::ButtonWidget>(driver.Root());
Tree::SimulateClick(*button);

driver.Build(); // re-render after the click
REQUIRE(driver.Component().state.Get().count == 1);
```

`FindDescendant<Target>(widget)` walks the built `Widget` tree looking for
the first value of type `Target`; `SimulateClick(widget)` fires a
`ButtonWidget`/`CheckboxWidget`/`RadioWidget`/`GestureRegion`'s click/toggle
behaviour directly, without going through ImGui input at all. This is
**logic-mode testing only** — there is no pixel/rendering assertion mode yet
(deferred until real headless rasterization exists; see
`.claude/DECISIONS.md`, Phase 30.4), and structural navigation is
compile-time/type-directed rather than a runtime string path, since
`Tree::Element` exposes no child-traversal API to build one on top of.
