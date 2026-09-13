---
name: imframe-coding-standards
description: Use whenever writing, reviewing, generating, or reasoning about ImFrame C++ source code, or when explicitly asked to check code against project conventions. Always apply these conventions automatically when producing any ImFrame C++ code snippet, header, or implementation file — do not wait to be asked about style, since consistency with the existing 50+ phases of code is a hard project requirement, not a preference.
---

# ImFrame C++ Coding Standards

ImFrame is C++20/23, built with strict naming, safety, and error-handling discipline enforced by `.clang-tidy`, `.clang-format`, and CI grep checks. This skill is the condensed quick-reference; the authoritative source is the project's `CLAUDE.md`.

## Naming conventions

| Element | Convention | Example |
|---|---|---|
| Files / directories | `PascalCase` | `ThreadPool.hpp`, `Rendering/` |
| Classes / structs | `PascalCase` | `class ThreadPool`, `struct WindowConfig` |
| Methods / free functions | `PascalCase` | `void Submit()`, `Path FromCwd()` |
| Private/protected members | `_camelCase` | `_workerThreads`, `_isRunning` |
| Local variables | `camelCase` | `auto windowConfig = ...` |
| Constants | `SCREAMING_SNAKE_CASE` | `MAX_ATLAS_SIZE` |
| Template parameters | `PascalCase` | `template<typename T>` |
| Namespaces | `PascalCase`, max 2 levels | `ImFrame::`, `ImFrame::Internal::` |

`ImFrame::Internal::` is reserved for implementation details that must never appear in a public header — this is enforced by `scripts/check_api_leaks.sh` in CI, not just convention.

## `[[nodiscard]]` — mandatory on

- Every RAII scope guard (`WindowScope`, `ChildScope`, `PopupScope`, etc.)
- Every `std::expected<T, Error>` return
- Every `std::future<T>` return
- `SubscriptionToken`, `TimerHandle`, `WatchHandle`, and similar handle types
- Any return value whose silent discard is almost certainly a bug

When in doubt, default to adding it — a missing `[[nodiscard]]` is a more common defect than a spurious one in this codebase.

## `noexcept` — mandatory on

- Every destructor
- Every move constructor and move assignment operator
- Any method provably free of throwing operations (no allocation, no throwing calls, value-type operations only)

## Error handling

- **Never throw from library code.** All failable operations return `std::expected<T, Error>` (or `std::expected<void, Error>` for failable actions with no return value).
- No raw `bool` return for success/failure where the failure reason matters — convert to `std::expected`.
- Exceptions are reserved for truly exceptional, unrecoverable situations only, and even then are rare in this codebase — prefer `Logger::Fatal` + abort for programming errors, `std::expected` for everything recoverable.

## Callbacks

- **`Delegate<Signature>` is the default**, not `std::function`. `Delegate` is a zero-heap-allocation, single-target callable wrapper used throughout widget and event APIs.
- `std::function` is acceptable only when the callable genuinely needs to be a large capturing closure that won't fit `Delegate`'s small buffer, or when multiple simultaneous targets are needed (in which case prefer `Signal<Sig>` instead, which is designed for that).
- `Signal<Sig>` / `ThreadSafeSignal<Sig>` (Phase 5) is for one-to-many notification; `Delegate<Sig>` is for one-to-one.

## RAII pattern for ImGui scopes

Every paired ImGui `Begin*/End*` or `Push*/Pop*` call is wrapped in a `[[nodiscard]]` RAII guard — never a raw paired call. Reference pattern (`PushPopScope<PushFn, PopFn>` template from Phase 2):

```cpp
[[nodiscard]] auto scope = BeginWindow({.Title = "Inspector"});
if (scope) {
    // contents
}
// destructor calls ImGui::End() automatically, unconditionally
```

After Phase 27 (widget tree), raw ImGui calls are restricted entirely to `src/Tree/RenderObjects/` — if you're writing anything outside that directory and reaching for a raw `ImGui::` call, stop and use the command buffer (`CommandBuffer::DrawRect`, etc., from Phase 31) or an existing primitive instead.

## Builder pattern (widgets, layouts, components)

Constructor takes only the minimum required arguments. Every optional configuration is a setter returning `*this` by reference for chaining. The terminal method is `Show()` (legacy Phase 10–14 imperative widgets, deprecated post-Phase 30) or `Build()` (Phase 27+ declarative components).

```cpp
Button("Save")
    .Icon(Icons::Fa::FloppyDisk)
    .OnClick(handler)
    .Disabled(isSaving)
    .Show();   // or composed inside another widget's Build()
```

Never deviate from this shape for a new widget or component — no setters returning `void`, no required arguments hidden behind setters, no alternate terminal method names.

## Layering / dependency direction

Lower layers never include higher ones. In particular:

- `include/ImFrame/Utility/` has **zero** ImGui dependency — verified by `grep -r "imgui" include/ImFrame/Utility/` returning nothing.
- `include/ImFrame/Tree/` (Phase 27+) also has zero ImGui dependency — the widget tree is pure data; ImGui calls live only in `RenderObject` implementations.
- If you're adding an `#include <imgui.h>` (or similar) to either of those directories, that's a layering violation — stop and reconsider where the code belongs.

## Memory / threading

- `std::jthread` everywhere, never raw `std::thread` — cooperative cancellation via `std::stop_token` is part of the pattern.
- Threads are always named (`pthread_setname_np` / `SetThreadDescription`) — an unnamed thread is a defect.
- Any callback that must run on the render thread but might be triggered from a background thread is dispatched via `EventBus::DispatchAsync()` or an equivalent `BackgroundWorker`-mediated path — never call into render-thread-only state (e.g. `Build()`, `State<T>::Set()`) directly from another thread.

## When reviewing existing code

Flag (don't silently fix unless asked) any of: raw `std::thread`, `std::function` where `Delegate` would fit, missing `[[nodiscard]]` on an `std::expected`/RAII-guard return, a thrown exception in non-Lua, non-test code, or an ImGui include inside `Utility/` or `Tree/`.
