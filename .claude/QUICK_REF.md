# ImFrame — Quick Reference (DOC_STANDARDS + GIT_RULES)

> **Load this file on demand only** — not on session start.
> Read when: writing new `.hpp`/`.cpp` files (DOC_STANDARDS) or preparing a commit (GIT_RULES).
> Condensed from DOC_STANDARDS v1.0.0 and GIT_RULES v1.0.0.
> Source documents: kept locally outside the repo (machine-specific path — never commit it, see CLAUDE.md's "Public Repo / Git Hygiene Rules")

---

## DOC_STANDARDS v1.0.0

### File Header — mandatory on every .hpp and .cpp, BEFORE #pragma once

```cpp
/**
 * @file     FileName.hpp          ← must match filename exactly, including extension
 * @brief    One sentence summary  ← no trailing period
 *
 * Optional 3–5 sentence description. Explain design decisions, constraints,
 * or anything a consumer needs before using the types declared here.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     YYYY-MM-DD            ← creation date only; git tracks changes
 * @version  0.8.0                 ← current library version
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once   ← public headers only; comes immediately after file block
```

Internal `.cpp` files: add `@internal` tag, omit `#pragma once`.

### Comment Style

| Style | When to use |
|-------|-------------|
| `/** */` | Multi-tag or multi-sentence blocks — classes, methods, enums |
| `///` | Single-line member variable or enumerator comments only |
| ~~`/*! */`~~ | Never — Qt style is banned |

### Per-Symbol Requirements

| Symbol | Required tags |
|--------|--------------|
| Public class | `@brief`, `@tparam` (if templated), `@since`, `@example` |
| Public struct | `@brief`, `@since`, `///` on each member |
| Public enum | `@brief`, `@since`, `///` on each enumerator |
| Public method / free function | `@brief`, `@param[in/out]` per param, `@return` (non-void), `@throws` |
| Constructor | `@brief`, `@param[in]` per param, `@throws` |
| Destructor | Only if non-obvious behaviour |

### Do NOT Document

- Obvious getters — `Width()`, `Name()`, `Id()`
- `= default` / `= delete` functions
- Private helper functions with clear names
- Copy/move ctors with standard behaviour
- Test functions (`TEST_CASE` blocks)
- Overriding methods where the base is fully documented — use `@copydoc` if needed

### @param Direction — always specify

| Tag | Meaning |
|-----|---------|
| `@param[in]` | Read-only input — function does not modify it |
| `@param[out]` | Function writes to it — initial value ignored |
| `@param[in,out]` | Function both reads and writes |

### @return Rules

- Always document non-void return values
- For `bool`: state what `true` and `false` mean specifically
- For `Result<T, E>`: describe both the success (T) and failure (E) case
- For pointers: state whether `nullptr` is valid and what it means

### @throws Rules

- Document every exception that can propagate out
- If `noexcept`, add `@throws Nothing — noexcept` inline via `///`

### Namespace — C++17 Nested Syntax Only

```cpp
// ✅ Correct
namespace ImFrame::Utility {
class Thread { };
} // namespace ImFrame::Utility

// ❌ Wrong — stacked style is banned
namespace ImFrame {
namespace Utility {
} // namespace Utility
} // namespace ImFrame
```

When types must live in parent and sub-namespace in the same file, close and reopen:

```cpp
namespace ImFrame {
struct WindowConfig { };
} // namespace ImFrame

namespace ImFrame::Internal {
class IBackend { };
} // namespace ImFrame::Internal
```

### Section Dividers (human readers — not Doxygen)

```cpp
// ─── Public constructors ──────────────────────────────────────────────────────
// ─── Platform: Windows ───────────────────────────────────────────────────────
// ─── Private helpers ──────────────────────────────────────────────────────────
```

### Inline Comments

- Explain **why**, not what (the code shows what)
- TODOs must have an owner: `// TODO(voidptr-cxx): Replace with SIMD in Phase 12.`
- No commented-out code — delete it; git history preserves it

---

## GIT_RULES v1.0.0

### Branch Naming

```
feature/phase-N-short-description   ← all lowercase, hyphens, 2–5 words
fix/short-description
docs/short-description
chore/short-description
```

Branches are short-lived — deleted the same day they merge.

### Commit Format (Conventional Commits)

```
type(scope): subject

[optional body — wrap at 72 chars]

[optional footer: Closes #N, Refs: Phase-N, BREAKING CHANGE: ...]
```

- Subject: **imperative mood**, **lowercase first letter**, **no trailing period**, **≤ 72 chars total**
- Body: required when the change is not self-evident from the subject
- Must complete: *"If applied, this commit will…"*

### Commit Types

| Type | Use for |
|------|---------|
| `feat` | New feature or public API addition |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `test` | Adding or fixing tests — no production code |
| `refactor` | Restructure with no behaviour change |
| `perf` | Performance improvement |
| `chore` | Build system, CI, dependencies |
| `style` | Formatting, naming — no behaviour change |
| `revert` | Reverting a previous commit |

### ImFrame Scopes

`utility` · `core` · `backends` · `app` · `theme` · `cmake` · `ci` · `phase-N`

### Commit Examples

```
✅  feat(utility): add Thread RAII wrapper with platform naming
✅  refactor(utility): migrate FileWatcher from std::thread to std::jthread
✅  test(utility): add BackgroundWorker unit and tsan tests
✅  chore(cmake): bump version to 0.8.0 and register Phase 7 sources
✅  docs(utility): update PHASE_STATUS and DECISIONS for Phase 7

❌  Fixed thread naming bug          ← past tense, no type
❌  feat(utility): Add Thread.       ← capital first letter, trailing period
❌  WIP                              ← never commit this
```

### Merge Strategy

| Target | Strategy |
|--------|----------|
| Feature → develop | `--no-ff` for large phase completions; squash for < 200 lines |
| develop → main | Always `--no-ff -m "release: vX.Y.Z"` |

Never fast-forward merge to `develop`.

### Pre-commit Checklist

- [ ] All tests pass locally
- [ ] No `printf` / `std::cout` debug output left in source
- [ ] No commented-out code
- [ ] No `std::thread` (Phase 4+ invariant — use `std::jthread`)
- [ ] All new public code has full Doxygen documentation
- [ ] `DECISIONS.md` updated if an architectural decision was made
- [ ] `PHASE_STATUS.md` Resumption Snapshot updated

### PR Rules

- Title = commit subject format: `feat(app): add Application, Window, DockSpace`
- Requires all CI checks green before merge
- Self-review using the checklist deliberately (not as a formality)

### Tagging and Versioning

```
vMAJOR.MINOR.PATCH   ← annotated tags only, on main only
```

- MAJOR: breaking public API change
- MINOR: new planned milestone (each phase)
- PATCH: bug fix

```bash
git tag -a v0.8.0 -m "ImFrame v0.8.0 — Application and Window"
git push origin main v0.8.0
```

### What Never Goes in Git

- Build artefacts: `build/`, `*.o`, `*.lib`, `*.dll`
- Secrets: `.env`, `*.pem`, `*.key`
- Generated docs: `docs/api/html/`
- Commented-out code
- WIP commits to `develop`

---

*Quick Reference version: Phase 7 · 2026-06-03*
