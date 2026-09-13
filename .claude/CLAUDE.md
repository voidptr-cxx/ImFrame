# ImFrame — Project Context for Claude Code

> Authoritative reference for every coding decision in ImFrame.
> All rules here override defaults.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).

---

## ⚡ Session Boot Sequence — Execute on Every Session Start

When I type **`start`**, or at the beginning of any session, execute these steps immediately:

1. Read `.claude/PHASE_STATUS.md` — find the **Resumption Snapshot** at the top
2. Run `git log --oneline -10` — orient on recent commits
3. Run `git status` — check for uncommitted work
4. If `.claude/SCRATCH.md` exists and is non-empty, read it
5. Confirm readiness with one line: `"Ready: Phase [N] — [current task]"`

**Do not re-derive state from source files. The Resumption Snapshot is authoritative.**

---

## 🏁 Session End — When I Say "wrap up"

1. Update the **Resumption Snapshot** in `.claude/PHASE_STATUS.md` with what was done and the next concrete action
2. If any architectural decisions were made, append them to `.claude/DECISIONS.md`
3. Summarize `.claude/SCRATCH.md` into `PHASE_STATUS.md` if relevant, then clear `SCRATCH.md`
4. Commit all changes with a message following GIT_RULES (type/scope/subject format)
5. Update graphify graphs

---

## 📝 During Work

- After completing each logical unit (function done, test passing), append a one-liner to `.claude/SCRATCH.md`
- If context feels large, warn me so I can `/compact`
- **Never re-litigate decisions in `DECISIONS.md`** — they are settled. Implement them.
- Load `.claude/QUICK_REF.md` only when writing new code (DOC_STANDARDS) or preparing a commit (GIT_RULES)

---

## Project Overview

**ImFrame** is a modern C++23 immediate-mode UI framework built on top of
[Dear ImGui](https://github.com/ocornut/imgui). It provides:

- A backend abstraction layer (GLFW+OpenGL3 in Phase 1; Vulkan/Metal/DX12/WebGPU later)
- A rich set of RAII wrappers around raw ImGui calls
- A theme engine, icon font integration, and layout primitives
- A reactive widget tree (Flutter-inspired) in later phases

The library is built phase-by-phase. Each phase has a `PHASE_NN_PROPOSAL.md`
file kept locally outside the repo (path is machine-specific — see
`.claude/settings.local.json` or ask the maintainer). Always read the
proposal before implementing a phase.

---

## Namespaces

| Namespace | Purpose |
|-----------|---------|
| `ImFrame::` | All public API types and functions |
| `ImFrame::Core::` | Context, error types, fundamental value types |
| `ImFrame::Backends::` | Backend abstraction (IBackend, WindowConfig, FrameInfo) |
| `ImFrame::Utility::` | File I/O, threading, events, logging, config |
| `ImFrame::App::` | Application, Window, DockSpace |
| `ImFrame::Theme::` | Theme, ColorToken, ThemeBuilder |
| `ImFrame::Themes::` | Named theme constants (Dracula, Nord, CatppuccinMocha, Light) |
| `ImFrame::Icons::` | Icon font constants and loader |
| `ImFrame::Widgets::` | Individual UI widgets |
| `ImFrame::Layout::` | Layout containers |
| `ImFrame::Anim::` | Tweening and animation |
| `ImFrame::Overlay::` | Toast, Modal, ContextMenu |
| `ImFrame::Rendering::` | RenderContext, Canvas2D, 3D viewport |
| `ImFrame::Tree::` | Reactive widget tree (Phase 27+) |
| `ImFrame::Internal::` | Private implementation details — never expose in public headers |

---

## Build Commands

```bash
# Configure (with vcpkg)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure

# clang-format check
find include src Examples -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

---

## Coding Standards

### Naming

| Symbol | Convention | Example |
|--------|------------|---------|
| Class / Struct / Enum | PascalCase | `WindowConfig`, `BackendError` |
| Function / Method | PascalCase | `Load()`, `GetWidth()` |
| Variable / Parameter | camelCase | `windowWidth`, `deltaTime` |
| Private member | `_camelCase` | `_impl`, `_width` |
| Constant | `UPPER_CASE` | `MAX_FRAMES_IN_FLIGHT` |
| Macro | `IMF_UPPER_CASE` | `IMF_ASSERT()` |
| Template parameter | PascalCase | `TPixel`, `TValue` |
| Namespace | PascalCase | `ImFrame`, `Core` |

### Error Handling

- **Never throw** across the public API. All fallible operations return
  `std::expected<T, ImFrame::Error>`.
- **Never use** `assert()` for user-facing argument validation. Use
  `ImFrame::Error` result types.
- Internal invariants use `IMF_ASSERT(condition)` (added Phase 1).

### Headers

- Public headers live in `include/ImFrame/`. They must **never** include ImGui
  headers directly (ImGui types must not appear in the public API).
- Backend-specific headers live in `Backends/<Name>/`. They may include ImGui
  and platform headers.
- Concrete backend implementations (`Backends/`) use `#include <glad/glad.h>`
  **before** `#include <GLFW/glfw3.h>` in every `.cpp` file.
- `#pragma once` on every header — always after the Doxygen file header block.

### Documentation (DOC_STANDARDS summary)

- Every `.hpp` and `.cpp` file starts with a Doxygen `/** @file ... */` block.
- `@brief` is one sentence, no trailing period. `@date` is creation date (not
  last-modified). `@version` matches the library version.
- `/** */` for anything with more than one tag. `///` for single-line member
  comments only.
- `@internal` on all `src/` and `Backends/` files — excluded from public docs.
- Every public class needs `@brief`, `@tparam` (if templated), `@since`,
  `@example`.
- Every public method needs `@brief`, `@param[in/out]`, `@return`, `@throws`.

### Namespaces

Always use C++17 nested namespace syntax — **never** the old stacked style:

```cpp
// ✅ Correct
namespace ImFrame::Internal {
} // namespace ImFrame::Internal

// ❌ Wrong — stacked style is not used
namespace ImFrame {
namespace Internal {
} // namespace Internal
} // namespace ImFrame
```

When a type must live in a parent namespace and a sub-namespace in the same
file, close the parent block and open a new block:

```cpp
namespace ImFrame {
struct WindowConfig { ... };
} // namespace ImFrame

namespace ImFrame::Internal {
class IBackend { ... };
} // namespace ImFrame::Internal
```

### Style

- 4-space indentation. 120-column limit.
- `BreakBeforeBraces: Attach` — opening brace on same line.
- `PointerAlignment: Left` — `int* ptr`, not `int *ptr`.
- No commented-out code in committed files — delete it; git preserves history.
- No `printf` / `std::cout` debug output left in committed code.

---

## Architecture Invariants

1. **No ImGui headers in `include/ImFrame/`** — consumers must not need to know
   ImGui exists. ImGui types stay behind the Pimpl boundary or in Backends/.
2. **No `ImGui_Impl*` calls outside `Backends/`** — all platform/renderer
   integration code lives in the backend layer.
3. **Backends/ is not part of the public API** — it is conditionally compiled.
   Consumers link to one backend target, not to all of them.
4. **GLAD before GLFW** — every `.cpp` in the OpenGL3 backend includes
   `<glad/glad.h>` before `<GLFW/glfw3.h>`.
5. **Static library only** — ImFrame is always `STATIC`. No shared library
   until version 2.0 (if ever).
6. **C++23 everywhere** — std::expected, std::format, std::ranges. No polyfills.

---

## Dependency Direction

```
Examples/DemoApp  →  ImFrame (public API only)
Tests/            →  ImFrame (public API) + Catch2
Backends/         →  ImFrame (internal headers) + platform libs (GLFW, glad)
src/              →  include/ImFrame/ (own public headers only)
include/ImFrame/  →  nothing (no dependencies in public headers in Phase 0)
```

---

## Phase Reference

| Phase | Content |
|-------|---------|
| 0 | Project scaffold, CMake, vcpkg, CI |
| 1 | Error types, IBackend abstraction, GLFW+OpenGL3 backend |
| 2 | Core context, Types (Vec2/Rect/Color) — skipped |
| 3 | Utility: Path, File, Directory, FileWatcher |
| 4 | Utility: Thread, ThreadPool, BackgroundWorker |
| 5 | Utility: EventBus, Signal, Delegate |
| 6 | Utility: Timer, Logger, Config |
| 7 | App: Application, Window, DockSpace |
| 8 | Theme engine + 4 built-in themes |
| 9 | Icon font integration |
| 10 | Core widget set (14 widgets) |
| 11 | Layout containers (Panel, HStack, VStack, Grid, ScrollArea) |
| 12 | Animation: Tween, AnimatedValue, Easing, Sequence |
| 13 | Overlays: Toast, Modal, ContextMenu |
| 14 | Table + PropertyGrid |
| 15–18 | TBD (data binding, scripting, hot-reload, API leak tooling) |
| 19 | FrameInfo, InputEvent |
| 20–23 | Vulkan / Metal / DX12 / WebGPU backends |
| 24 | RenderContext, Viewport |
| 25 | Canvas2D, Camera2D, DrawContext |
| 26 | Viewport3D, Camera3D, Gizmo |
| 27 | Widget tree core |
| 28 | Reactive state (State, Signal, Computed, InheritedWidget) |
| 29 | VirtualList, Portal |

---

## Key Files

| File | Purpose |
|------|---------|
| `include/ImFrame/ImFrame.hpp` | Umbrella include |
| `src/Placeholder.cpp` | Phase 0 stub TU — replaced Phase 1+ |
| `CMakeLists.txt` | Root build file |
| `vcpkg.json` | Dependency manifest |
| `.clang-format` | Code formatting rules |
| `.clang-tidy` | Static analysis rules |
| `.github/workflows/ci.yml` | Main CI matrix |
| `.claude/PHASE_STATUS.md` | Resumption snapshot + phase progress |
| `.claude/DECISIONS.md` | Architectural decision log |
| `.claude/QUICK_REF.md` | DOC_STANDARDS + GIT_RULES (load on demand) |
| `.claude/SCRATCH.md` | Intra-session work log (cleared each session) |

---

## Per-Phase Guidance

Before starting any phase:
1. Read that phase's `PHASE_NN_PROPOSAL.md` from its local (non-repo) proposals folder
2. Check `PHASE_STATUS.md` Resumption Snapshot for open items
3. Create a `feature/phase-N-short-description` branch from `develop`
4. Follow DOC_STANDARDS for every new file
5. After completing the phase, update `PHASE_STATUS.md` and `DECISIONS.md`

---

## 🔒 Public Repo / Git Hygiene Rules

**ImFrame is a public, open-source repository.** These rules exist because on
2026-09-13 the entire `.claude/` folder was found tracked and pushed to the
public GitHub remote, leaking a local Windows username and personal folder
paths (`C:\Users\<name>\OneDrive\...`) across ~200 historical commits and 29
branches. The full history was rewritten (`git filter-repo`) to strip
`.claude/` from every commit, force-pushed over the old public history, and
every already-merged feature branch was deleted (both locally and on
origin) — see `.claude/DECISIONS.md`'s "Public Repo Hardening" entry for the
full incident record. **Do not let this happen again:**

1. **`.claude/` is never tracked.** It is fully `.gitignore`d (not just
   `settings.json`). It holds local session state, scratch notes, and
   personal machine paths — none of it belongs in a public repo, however
   useful it is to a Claude Code session running locally. If a future need
   arises to share project-decision history publicly, create a separate,
   deliberately-sanitized file for that (e.g. a public `ROADMAP.md` or
   `CHANGELOG.md`), never by re-tracking `.claude/` itself.
2. **Never write an absolute local filesystem path into any file that will
   be committed** — no `C:\Users\<name>\...`, no `/home/<name>/...`, no
   `/Users/<name>/...`. Reference local-only resources (proposal documents,
   personal notes, machine-specific tool paths) by a relative description or
   an environment variable, never a literal path containing a username.
3. **Before staging any commit**, mentally re-scan the diff for: personal
   file paths, real names/emails not already public in commit authorship,
   and anything matching this repo's own `.gitignore` "Secrets" section
   patterns (`*_token*`, `*_secret*`, `*_password*`, `*.pem`, `*.key`) that
   may have been added anyway via an explicit `git add -f`.
4. **Never `git add -f` a `.gitignore`d path** without first checking *why*
   it's ignored — every ignore rule in this repo's `.gitignore` exists for a
   specific, documented reason (see its section comments).
5. A force-push or history rewrite on this **public** repo is a last-resort
   incident response, not a routine operation — it invalidates every
   existing clone/fork and requires the same care taken on 2026-09-13
   (full backup mirror before touching anything, verify the rewrite removed
   exactly the intended paths and nothing else, confirm via a *fresh*
   independent clone from GitHub — not a local working copy — before
   declaring it clean).
