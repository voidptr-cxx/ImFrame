---
name: imframe-git-commit
description: Use whenever preparing a git commit message, branch name, pull request, or release for the ImFrame project, or whenever asked to "follow GIT_RULES.md", "commit this", "prepare a PR", or "tag a release". Always apply this skill before generating any commit message or branch name for ImFrame work — generic commit conventions are not sufficient since ImFrame uses a specific scope table and branch/release process documented in the project's own GIT_RULES.md.
---

# ImFrame Git Workflow

ImFrame follows a strict, documented Git workflow adapted from the author's `GIT_RULES.md` (originally written for the Img project, adapted for ImFrame in Phase 49.4). This skill encodes the parts needed to generate correct commits, branches, and PRs without re-deriving the rules each time.

## Branch structure

```
main          ← release branch — tagged commits only, always stable
develop       ← integration branch — all phase work merges here first
feature/*     ← individual phase or feature branches
fix/*         ← bug fix branches
docs/*        ← documentation, wiki, proposal changes
chore/*       ← build system, CI, tooling, dependency changes
```

Never commit directly to `main`. The only path to `main` is a PR from `develop` with all CI gates green.

## Branch naming

```
feature/phase-19-backend-hardening
feature/phase-27-widget-tree-core
fix/phase-20-vulkan-headless-surface
fix/swapchain-resize-crash
docs/phase-49-wiki-book-1
docs/update-contributing-guide
chore/pin-dawn-vcpkg-version
chore/add-tsan-ci-job
```

Pattern: `<type>/<short-kebab-case-description>`, optionally prefixed with `phase-NN-` when the work is tied to a specific phase.

## Commit message format (Conventional Commits)

```
<type>(<scope>): <subject>

<optional body>
```

- Subject line ≤ 72 characters, imperative mood ("add", not "added" or "adds").
- Types: `feat`, `fix`, `refactor`, `docs`, `chore`, `test`, `perf`.
- One commit = one logical change. Never combine unrelated changes in a single commit.

## Scope table

Use the most specific scope that applies. Never nest scopes.

| Scope | Covers |
|---|---|
| `core` | `include/ImFrame/Core/` — RAII guards, types, error |
| `utility` | `include/ImFrame/Utility/` — File, Thread, EventBus, Timer, Logger, Config |
| `app` | `include/ImFrame/App/` — Application, DockSpace, WindowManager |
| `theme` | `include/ImFrame/Theme/` — Theme, ThemeBuilder, built-in themes |
| `icons` | `include/ImFrame/Icons/` — FA6 constants, atlas merge |
| `anim` | `include/ImFrame/Anim/` — Tween, AnimatedValue, Easing |
| `widgets` | `include/ImFrame/Widgets/` — all widget types |
| `layout` | `include/ImFrame/Layout/` — Panel, HStack, VStack, Grid, ScrollArea |
| `overlay` | `include/ImFrame/Overlay/` — Toast, Modal, ContextMenu |
| `rendering` | `include/ImFrame/Rendering/` — Viewport, Canvas2D, Viewport3D |
| `tree` | `include/ImFrame/Tree/` — Widget, Element, Component, State, Signal |
| `backends` | `Backends/` — all IBackend implementations |
| `platform` | `include/ImFrame/Platform/` — Clipboard, dialogs, tray, notifications |
| `audio` | `include/ImFrame/Audio/` — AudioEngine, Sound, AudioPlayer |
| `compute` | `include/ImFrame/Compute/` — ComputePass, ComputeBuffer |
| `plugin` | `include/ImFrame/Plugin/` — PluginManager, Lua scripting |
| `mobile` | `Backends/iOSMetal/`, `Backends/AndroidVulkan/`, mobile widgets |
| `i18n` | `include/ImFrame/i18n/` — Locale, LocaleManager, Tr() |
| `a11y` | `include/ImFrame/Accessibility/` — AccessibilityTree, AccessibilityProps |
| `network` | `HttpClient.hpp`, `WebSocket.hpp` |
| `document` | `include/ImFrame/Document/` — PdfExporter, PrintDialog |
| `demo` | `Examples/DemoApp/` |
| `cmake` | `CMakeLists.txt`, `vcpkg.json`, `vcpkg-configuration.json` |
| `ci` | `.github/workflows/` |
| `docs` | `Docs/`, `ImFrame.wiki/` |
| `phase-N` | Changes spanning multiple components, tied to a specific phase number |

If a future component doesn't map cleanly to this table, propose a new scope row rather than forcing it into an unrelated one, and flag it to the user for confirmation.

## Example commits

```
feat(tree): add Portal primitive for overlay rendering

Renders child widget at element tree root regardless of structural
position. Used for ToastManager, Modal backdrop, and ContextMenu
popups so they escape parent clipping.

fix(backends): correct row de-striding in DX12 ReadPixels

256-byte row alignment was not accounted for, producing diagonal
tearing in headless conformance tests on non-power-of-two widths.

chore(cmake): pin dawn vcpkg port to 2026-04-12 baseline

docs(phase-49): add Book 3 feature guides to ImFrame.wiki
```

## Pre-commit checklist

Before generating a commit or telling the user it's ready to push, confirm (or remind the user to confirm):

- [ ] `scripts/check_api_leaks.sh` passes
- [ ] `grep -r "imgui" include/ImFrame/Utility/` returns nothing
- [ ] `grep -r "imgui" include/ImFrame/Tree/` returns nothing
- [ ] All tests pass locally (`ctest --output-on-failure`)
- [ ] No debug `printf` / `std::cout` left in source
- [ ] No commented-out code
- [ ] No `TODO` without an owner tag
- [ ] Commit messages follow the format above
- [ ] Subject line ≤ 72 characters
- [ ] `clang-format` produces no diff

## Merge strategy

- PRs into `develop`: standard merge with `--no-ff`, never squash — phase work history is preserved intentionally.
- PRs into `main`: also `--no-ff`, only after all `develop`-tier and `main`-tier CI gates are green (see the project's CI gate tables in `GIT_RULES.md` / Phase 49.4 proposal if unsure which gates apply).
- Feature branches are deleted immediately after merge — same day, both locally and on origin.

## Release tagging

```bash
git checkout main
git merge --no-ff develop -m "release: vX.Y.Z"
git push origin main
git tag -a vX.Y.Z -m "ImFrame vX.Y.Z — <one-line summary>

<longer summary of what's included>

Next: <what's planned next>"
git push origin vX.Y.Z
```

Every commit on `main` must correspond to a tag. An untagged commit on `main` is a process violation, not a style nit — flag it if you see one.

## CHANGELOG.md discipline

Every PR into `main` must include a corresponding entry added to `CHANGELOG.md` under `[Unreleased]` (moved to a dated version section at release time), following Keep a Changelog format. A PR to `main` with no changelog update should be flagged before proceeding.
