# ImFrame — Phase Status

---

## ⚡ Resumption Snapshot

**Active Phase:** 35 (All Five Backend Renderers — Vulkan/Metal/DX12/WebGPU `NativeRenderer`s, `NativeRenderer` as default, cross-backend conformance). **Phase 34 (Effects Pipeline, all sub-phases 34.1–34.6) is fully complete, committed, and merged into `develop`** — `PHASE_34_PROPOSAL.md`'s literal scope is entirely implemented and tested on `NativeRendererGL3`. **Phase 33 (all sub-phases, 33.1–33.8) is fully complete, committed, and merged.** **Phase 32 (all sub-phases, 32.1–32.9) is fully complete, committed, and merged** (32.1: `3b4649b`/`1ce067a`; 32.2: `e863130`/`0c1ef33`; 32.3: `ed3dc4d`/`fcedd80`; 32.4: `41f341c`/`4a60514`; 32.5: `6408507`/`eaafd48`; 32.6: `8366f43`/`963cb94`; 32.7: `c3112c8`/`dd0c6a7`; 32.8: `3801ef6`/`0f2bf40`; 32.9: `7d099a1`/`64d9e0b`). Phase 31 (all sub-phases) remains complete and merged. v2.0.0 remains untagged. `develop` is now ahead of `origin/develop` — **not pushed** this session (never asked to). **Note (unchanged since Phase 32.5):** two merge commits (32.1's `1ce067a`, reused for 32.4's `4a60514` and 32.5's `eaafd48`) have a copy-pasted subject line reading "NativeRenderer core types" — cosmetic, not touched. Also unchanged: `origin/develop` includes the 32.1 merge despite no `git push` this session — flagged previously, not investigated further, not blocking.

**Phase 35 backend order (user's own choice, 2026-09-12):** Vulkan first (cross-platform, buildable/testable here, most fully-specified of the four in `PHASE_35_PROPOSAL.md`), then presumably DX12/WebGPU in some order. **Metal cannot be built or tested from this Windows session at all** — deferred indefinitely, flagged explicitly rather than silently skipped.

**Phase 35 priority call (user's own choice, 2026-09-13):** given Vulkan's Shadow/Layer/BackdropBlur parity work (35.4-35.6) is done, the user chose to defer Vulkan's live frame-loop integration (it needs a real architectural redesign — see Phase 35.6's own remaining-work note below, superseded by this one) in favour of breadth: get DX12 and WebGPU each to their own "first SDF rect" starting point first, matching where Vulkan itself began at Phase 35.1.

**Next Action:** Phase 35.8 (complete, this session) is WebGPU's own "first SDF rect" sub-phase — a new `NativeRendererWebGPU` (`Backends/DawnWebGPU/`), mirroring `NativeRendererDX12`'s Phase 35.7 scope and structure exactly (`BatchKind::Rect` only, lazy WebGPU resource init, streaming vertex/index/uniform buffers re-uploaded per `Render()` call via `wgpuQueueWriteBuffer()`). Unlike Vulkan/DX12, **no fence or CPU stall of any kind is needed** — `ViewportWebGPU.cpp`'s own documented finding ("WebGPU's sequential submit model guarantees the above work completes before the next submit on this queue") applies identically here. Unlike SPIR-V/DXIL, WGSL is compiled **at runtime** by Dawn, so the shader source is inlined directly as a C++ raw string in `NativeRendererWebGPU.cpp` (mirroring `NativeRendererGL3`'s own hand-written-inline-shader convention) rather than routed through any offline `Shaders/*.glsl`/`*.hlsl`-style CMake pipeline — no new shader-embed library target was needed. **The inline WGSL `VSMain` negates Y, matching `SDFRect.hlsl`'s D3D convention** — WebGPU's NDC is Y-up (same as D3D/Metal), unlike Vulkan's Y-down NDC; derived independently and verified via a deliberately off-center asymmetric-placement test present from this sub-phase's first commit, which passed on the first real attempt once the one real bug below was fixed. `RoundedBoxSdf()`'s port uses `select()` in place of GLSL/HLSL's ternary/swizzle-assignment (WGSL has neither). **Found and fixed one real, WGSL-specific bug**, caught by Dawn's own shader validator at load time (not a runtime pixel mismatch): the fragment shader's stroke-blending code called `fwidth()` inside a `strokeWidth > 0.0` branch — WGSL requires derivative built-ins to run only from *uniform* control flow (a stricter rule than GLSL/HLSL enforce), so `SDFRect.glsl`/`SDFRect.hlsl`'s identical structure never surfaced this. Fixed by hoisting the `fwidth()` call and its dependents out of the branch, leaving only the final blend gated behind it. Also extended `DawnWebGPUBackend::WebGPURendererHandles` with a third field, `Instance` (alongside `Device`/`Queue`), needed only by the test's own synchronous `wgpuInstanceWaitAny()`-based readback — mirroring `DawnWebGPUBackend::ReadPixels()`'s own existing internal pattern. **Landed clean after that one fix** — all 3 new tests (filled two-tone rect, off-center asymmetric, rounded corners) passing. Full suite **556/556** (excluding `ThreadPool stress` and `DX12Conformance`, the latter newly discovered broken by an unrelated, pre-existing `NOMINMAX` bug — see below), `check_api_leaks.sh` clean, both Debug and Release built and passing for `NativeRendererWebGPU`'s own targets. See `.claude/DECISIONS.md`'s "Phase 35.8" section for full reasoning. Committed on `feature/phase-35.8-webgpu-sdf-rect` as `ca744a6`, merged into `develop` as `4596611` (clean, no conflicts).

**Discovered, not fixed (out of scope for 35.8, flagged separately):** running this sub-phase's own "full Debug build must be clean" verification bar as an actual non-targeted full build (rather than building only the WebGPU-specific targets) surfaced a pre-existing, unrelated compile failure in `Tests/Backends/DX12Conformance_test.cpp` — `std::numeric_limits<float>::max()` mangled by `<windows.h>`'s own `max` macro, surfacing inside the public `include/ImFrame/Tree/Element.hpp`/`Tree/Primitives/Box.hpp` headers, almost certainly a missing `NOMINMAX` somewhere in that test's own DX12-backend include chain. Lives entirely in Phase 22-era code, unrelated to WebGPU rendering — flagged as a background follow-up task rather than fixed here. `ctest` excluding just that one (already-unbuilt) target still ran 556/556 clean.

**Remaining Phase 35 work, roughly in order (all three of Vulkan/DX12/WebGPU now have their own "first SDF rect" milestone):**
- DX12/WebGPU's own `BatchKind::Image`/`Shadow`/`Layer`/`BackdropBlur` parity work, mirroring Vulkan's own Phase 35.2/35.4-35.6 sequence — not yet started for either backend.
- Vulkan: real integration into `SDL3VulkanBackend`'s live frame loop (replacing the current per-`Render()`-call CPU stall with proper semaphore sync) and `IRenderer` batching/text — explicitly deferred (2026-09-13) in favour of the breadth work above; still open.
- Metal deferred indefinitely (untestable from this session).
- `Application::WithRenderer(RendererConfig)` / `NativeRenderer`-as-default, and the `CrossBackendConformance_test.cpp`/`BackendDiff` tool, come after every backend has at least parity with GL3's Phase 34 feature set.

**Loose ends — status as of 2026-09-06: ALL THREE fixed, root-caused, not just re-flagged. Full suite is 497/497 clean with zero exclusions — the first fully clean run all session.**
- `task_9779c419` (the `ThreadPool` stress hang) — a lost-semaphore-permit bug in the worker loop. Empirically verified: the stress test alone, 40/40 clean runs post-fix (was ~53% hang rate before). `ctest` invocations no longer need `-E "ThreadPool stress"`.
- `task_122ea9a9` (`IconFont_test.cpp`'s two runtime tests, silently SKIPPED every `ctest` run) — fixed the missing `WORKING_DIRECTORY` and simplified `FindSolidFont()`'s dead duplicate-candidate array down to one path. This uncovered a **second, previously entirely-hidden bug**: merging an explicitly-sized font onto an implicitly-sized `AddFontDefault()` destination trips a real ImGui assertion — fixed by giving the test fixture's base font an explicit `SizePixels`. Both previously-skipped tests now pass.
- The `Config Set and Save round-trip` / `Config Set coalesces...` flakes — an initial fix (widening `Save()`'s lock to serialise concurrent writers' content) looked sufficient against isolated single-test reruns but was **incomplete**: repeating the check as full-suite `ctest` runs (this project's actual standing verification bar) surfaced a different, near-100%-reproducible failure — `fs::remove()` throwing a Windows sharing-violation exception because a lingering coalesced background save could still be mid-write (file handle open) when a test's cleanup ran. Real fix: a process-wide `FileIoMutex()` serialising all `Save()`/`Load()`/`PollWatcher()` file access (a per-instance lock can never stop `Load()`, which is `static`, from racing a different instance's save), plus a new public `Config::FlushPendingSave()` so a caller can deterministically wait for an in-flight coalesced save before relying on the file being settled. **18 consecutive full-suite runs, 497/497 every time**, closes this out for real — see `.claude/DECISIONS.md`'s corrected/superseding rows for the full sequence of what was tried and why the first attempt wasn't enough.

See `.claude/DECISIONS.md`'s three "Loose-End Fix" sections for full detail on each.

**Project-wide, not phase-specific (2026-09-07):** at the user's request, every one of this codebase's own source files (not vendored third-party headers) had its copyright header changed from "Proprietary and confidential... All rights reserved" to a two-line MIT reference; a root `LICENSE` file with the full MIT text was added; `.claude/QUICK_REF.md`'s own file-header template was updated so new files default to the new license; `vcpkg.json` gained `"license": "MIT"`. See `.claude/DECISIONS.md`'s "Project-Wide Change — MIT License Adoption" section for full detail, including how this 316-file, comment-only change was kept on its own branch, separate from the unrelated `Config` file-race fix discovered during the same session even though both incidentally touched the same 3 files. Committed on `chore/mit-license` as `b6a5544`, merged into `develop` as `ca1888d` (a merge conflict in `.claude/DECISIONS.md` — both branches had appended distinct, non-overlapping sections since the branch point — resolved trivially by keeping both).

**Project-wide, not phase-specific (2026-09-13) — Public Repo Hardening:** the user discovered `.claude/` was tracked and pushed to the public GitHub remote, leaking a local Windows username/personal folder path across ~200 commits and 29 branches (no actual secrets/credentials were found — verified by scanning full history for API-key/token/private-key patterns and secret-style filenames, both clean). Full history rewritten with `git-filter-repo` (run on a disposable clone, after taking a full mirror backup and a plain-file backup of `.claude/`'s contents), force-pushed over every branch, then all 29 non-`develop` branches (confirmed fully merged, none unique) deleted both locally and on `origin` — `develop` is now the sole remaining branch, verified clean via a fresh independent clone from GitHub (152 commits, zero `.claude/` hits). `.claude/` is now fully `.gitignore`d (was previously just `settings.json`) and restored locally as untracked content so session continuity keeps working; `CLAUDE.md`/`QUICK_REF.md` had their personal-path references genericized, and `CLAUDE.md` gained a new "Public Repo / Git Hygiene Rules" section. The repo stays public (deliberately open-source going forward) rather than being made private. See `.claude/DECISIONS.md`'s "Public Repo Hardening — `.claude/` History Purge" section for the full incident record. Backups kept, not auto-deleted: `../ImFrame-FULL-BACKUP-<timestamp>.git` (full mirror) and `../ImFrame-claude-backup-<timestamp>/` (`.claude/` file copy) alongside the repo — delete once confirmed no longer needed.

### Phase 33.1 status ✅ Complete — new dependencies (`harfbuzz`, `msdfgen`) added and verified

**Delivered:** `vcpkg.json` gained a new opt-in feature `"text-msdf"` (`harfbuzz` + `msdfgen`); `CMakeLists.txt` gained `IMF_BUILD_TEXT_MSDF` (default `OFF`), read before `project()` and appending `"text-msdf"` to `VCPKG_MANIFEST_FEATURES` when ON — the exact same shape as `IMF_BUILD_NATIVE_RENDERER`/`"native-renderer"` (Phase 32.2).

**Verified:** `cmake -B build-text -DIMF_BUILD_TEXT_MSDF=ON` succeeds end-to-end — **26 minutes** for vcpkg to resolve and build both dependencies from source on this machine (real measurement, the actual risk gate for this sub-phase, cleared). CMake targets confirmed available: `harfbuzz::harfbuzz` (+ `harfbuzz::harfbuzz-subset`), `msdfgen::msdfgen-core`, `msdfgen::msdfgen-ext` (the FreeType-integrated one `FontFace` needs), `msdfgen::msdfgen-full`. Headers confirmed present (`hb-*.h`, `msdfgen.h`/`msdfgen-ext.h`). `tinyxml2` pulled in transitively by msdfgen's default feature set — not a concern. Default (`IMF_BUILD_TEXT_MSDF` unset) `build/` config rebuilt successfully after the `CMakeLists.txt` edit — confirmed unaffected. `check_api_leaks.sh` clean (no source files touched yet, dependency scaffolding only).

**Not done, and why:** no C++ code was written this sub-phase — deliberately scoped to "prove the dependencies actually resolve and build" before writing any code against their APIs, since a from-source vcpkg build of two new libraries was the real unknown here, not the C++ itself.

**Session note:** `build-text/` (scratch, not part of the repo) is being **kept** across this session's remaining Phase 33 sub-phases, unlike Phase 32's disposable `build-native/` — re-paying the 26-minute dependency build on every sub-phase's verification pass would be wasteful. Delete it once Phase 33 work concludes for the session, or reconfigure it if `vcpkg.json`/`CMakeLists.txt` changes again.

**Branch:** none (on `develop`) — merged `3773413`/`c074857`, local feature branch deleted.

### Phase 33.2 status ✅ Complete — `FontFace`/`FontRegistry` (FreeType + HarfBuzz loading)

**Delivered:** `src/Rendering/Text/{FontFace,FontRegistry}.{hpp,cpp}` (both `ImFrame::Internal`, matching `BatchBuilder`/`IRenderer`'s precedent of living under `src/Rendering/` without being public API).

- `FontFace::Load(FT_Library, Path, sizePixels) -> Result<FontFace>` — opens the file via `FT_New_Face()`, sets the initial size via `FT_Set_Pixel_Sizes()`, binds a `hb_font_t*` via `hb_ft_font_create()` (non-`_referenced` — `FontFace` owns the `FT_Face`/`hb_font_t` pair together, destroyed in a fixed order). Move-only.
- `FontRegistry::Load(Path, sizePixels) -> Result<Rendering::FontId>` — lazily initialises one shared `FT_Library` on first use (matching `NativeRendererGL3::EnsureInitialized()`'s lazy-init convention), issues sequential `FontId` values starting at 1 (0 stays reserved/invalid, per `FontId`'s own existing doc contract), stores each `FontFace` in a map keyed by the id. `FontRegistry::Get(FontId) -> const FontFace*` for later lookup by `TextShaper`/`GlyphAtlas`.
- Reuses the *existing* `Rendering::FontId` type (Phase 31) — no new ID type needed.
- New `Error::FontLoadFailed` enum value in the public `Error.hpp` — the first time ImFrame's own `src/` code calls FreeType directly (previously only ImGui's own bundled FreeType usage was ever exercised, via `ImFontAtlas::AddFontFromFileTTF()`).
- `CMakeLists.txt`: `IMF_BUILD_TEXT_MSDF`-gated `target_sources`/`target_link_libraries(ImFrame PRIVATE Freetype::Freetype harfbuzz::harfbuzz)`, plus a conditional `find_package(harfbuzz CONFIG REQUIRED)` (missed in 33.1's dependency-only scaffolding, added now that something actually links it).

**New test:** `Tests/Rendering/FontRegistry_test.cpp` — 6 test cases against the real vendored `Assets/Fonts/fa-solid-900.ttf` (same asset `IconFont_test.cpp` already uses): successful load, `FileNotFound` on a missing path, `Get()` on invalid/unknown/valid ids, distinct ids across multiple loads. **Discovered and fixed a real ctest working-directory bug** while writing this: `catch_discover_tests()`'s default CWD doesn't match where the `POST_BUILD` copy step places the font, so 3 of 6 tests failed under `ctest` despite passing when run directly — fixed by passing `WORKING_DIRECTORY "$<TARGET_FILE_DIR:...>"` explicitly. The *identical*, still-unfixed bug was found in `IconFont_test.cpp` (its two runtime tests have silently `Skipped` on every `ctest` run this entire session) — flagged as background task `task_122ea9a9` rather than fixed here (out of scope for this sub-phase's own new files).

**Verified:** New tests: 16 assertions / 6 test cases, all passing under both direct execution and `ctest`. Full suite `IMF_BUILD_TEXT_MSDF=ON` (`build-text/`): **438/438** (excluding the unrelated `ThreadPool stress` hang). Full suite `IMF_BUILD_TEXT_MSDF=OFF` (`build/`): **488/488** (same exclusion) — `Error::FontLoadFailed` is a pure additive enum value, no behaviour change for the default config. `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `4d8e4bb`/`72c10b6`, local feature branch deleted.

### Phase 33.3 status ✅ Complete — `TextShaper` (HarfBuzz shaping + LRU cache)

**Delivered:** `src/Rendering/Text/TextShaper.{hpp,cpp}` (`ImFrame::Internal`).

- `GlyphRun{GlyphId, Advance, Offset, Cluster}` and `ShapedText{Runs, TotalAdvance}` — new public-to-`Internal::` structs, converted from HarfBuzz's 26.6-fixed-point `hb_glyph_position_t` to float pixels (`/64.0f`, matching the FreeType-derived scale `hb_ft_font_create()` set up in Phase 33.2).
- `TextShaper::Shape(FontRegistry&, FontId, string_view) -> const ShapedText*` — reuses one `hb_buffer_t` across calls (HarfBuzz's own recommended pattern; reset, not recreated, each call), calls `hb_buffer_guess_segment_properties()` to auto-detect script/direction/language before `hb_shape()`. Returns `nullptr` for an unknown `FontId` (mirrors `FontRegistry::Get()`'s convention) rather than `Result<>`, since `std::expected` would force a full-vector copy on every call including cache hits.
- Hand-written LRU cache (`std::list<CacheKey>` recency order + `std::unordered_map<CacheKey, pair<ShapedText, iterator>>`), default capacity 256, evicting the tail of the list once exceeded. `CacheKey{FontId, string Text}` with a local `CacheKeyHash` functor (no global `std::hash<FontId>` added — matches `FontRegistry`'s own reasoning for keying by `FontId::Value()` instead).
- `CMakeLists.txt`: `TextShaper.cpp` added to the same `IMF_BUILD_TEXT_MSDF`-gated block as Phase 33.2's files.

**New test:** `Tests/Rendering/TextShaper_test.cpp` — 7 cases. Five use the vendored FA6 icon font (always run everywhere): invalid-id → `nullptr`, empty text → empty non-null `ShapedText`, one-glyph-per-codepoint with positive advances, cache-hit pointer identity, and a rigorous LRU-eviction test (touch-then-evict-then-confirm-survivor, verified via guaranteed pointer identity on a genuine cache hit — not a pointer-inequality heuristic). Two reference `C:/Windows/Fonts/calibri.ttf` by absolute path (`SKIP()` if absent, matching `IconFont_test.cpp`'s established pattern — no font was vendored into the repo, per this project's "don't fetch/vendor unreviewed third-party source" policy): confirmed **real** HarfBuzz behaviour — "fi" merges into 1 glyph vs. "fx" staying 2 glyphs, and "AV"'s combined advance measurably differs from "A"+"V" summed independently (kerning).

**Verified:** New tests: 32 assertions / 7 test cases, all passing under both direct execution and `ctest` (including the two Calibri-dependent tests actually executing, not skipping, on this machine). Full suite `IMF_BUILD_TEXT_MSDF=ON`: **445/445** (excluding the unrelated `ThreadPool stress` hang). Full suite `IMF_BUILD_TEXT_MSDF=OFF`: **488/488** (unaffected — no OFF-config file changed). `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `051b6a5`/`a060f5d`, local feature branch deleted.

### Phase 34.2 status ✅ Complete — GL3 fwidth-based AA port (`NativeRendererGL3` + `Image.glsl`)

**Delivered:** Completes the fwidth-AA fix Phase 34.1 started, closing both gaps that sub-phase deliberately deferred.

- `Backends/GLFWOpenGL3/NativeRendererGL3.cpp`: `kRectFragmentSource` (the real GL3 renderer's Rect shader) ported to the same `aa = max(fwidth(dist) * 0.5, 1e-4)` technique as `SDFRect.glsl`, for both fill and stroke edges.
- `kImageFragmentSource` (GL3 Image shader) got the identical fix — found directly adjacent to the Rect fix in the same file, sharing the same `RoundedBoxSdf` + fixed-band pattern; fixing one and not the other would have left rects and images visibly disagreeing on AA quality inside the same renderer.
- `Shaders/Image.glsl`'s own `#version 450` rounding mask (the gap Phase 34.1 explicitly flagged but didn't fix) got the same treatment.
- **No new pixel-precision test added** — `fwidth()`'s exact per-fragment value is GPU/driver-dependent (quad-based derivative implementations vary), so pinning an exact sub-pixel alpha value would couple a test to hardware specifics rather than this codebase's logic (matches `MSDFText.glsl`'s own precedent — never precision-tested beyond "real coverage was drawn"). Verified instead via the existing `NativeRendererGL3_test.cpp` corner/stroke/fill tests passing unchanged — they sample far enough from the edge that a ~2x-narrower band cannot flip their pass/fail outcome, and didn't.

**Verified:** `build-native/` full suite **450/451** (the one failure, `Config Set and Save round-trip`, reconfirmed the same pre-existing unrelated flake from 34.1 — passes standalone). `build-text/` full suite **475/475**, clean. Default `build/` not rebuilt — provably unaffected (only `Shaders/*.glsl` and `NativeRendererGL3.cpp` touched, both gated off in that config). `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `be02eb0`/`6a4bc07`, local feature branch deleted.

### Phase 34.1 status ✅ Complete — `SDFRect.glsl` (renamed from `Rect.glsl`, fwidth-based AA)

**Delivered:** `Shaders/Rect.glsl` renamed to `Shaders/SDFRect.glsl` (per `PHASE_34_PROPOSAL.md`'s "New Files" list) and deleted (`git rm`), not kept as a fallback — matching the proposal's own stated invariant.

- The only functional change: the anti-aliasing band changed from a hardcoded `smoothstep(-1.0, 1.0, dist)` to `smoothstep(-aa, aa, dist)` with `aa = max(fwidth(dist) * 0.5, 1e-4)`, applied to both fill and stroke edges. The `RoundedBoxSdf` math itself, and single-pass fill+stroke handling, are unchanged from Phase 32.2 — see `.claude/DECISIONS.md` for why the proposal's "replaces tessellation" framing didn't match what actually needed changing.
- `BatchKind::Rect`/`RectVertex`/`DrawRect`/`AppendRect()` are all unchanged — only the shader **file** was renamed.
- `CMakeLists.txt`'s shader-compile `foreach` list updated (`Rect` → `SDFRect`); `ShaderPipeline_test.cpp`'s included headers/symbols renamed to match.
- Every comment referencing the old filename (`Image.glsl`, `NativeRendererGL3.hpp`/`.cpp`, `TextRendererGL3.cpp`, `BatchBuilder.hpp`) updated to `SDFRect.glsl`.
- **Not done, deliberately:** `NativeRendererGL3.cpp`'s hand-written GL3 port of the Rect shader still has the old fixed-band AA (mirrors the Phase 32.2→32.4 / 33.5→33.7 shader-then-GL3-port split — see next action above). `Image.glsl`'s own rounded-corner mask has the identical un-fixed AA-band inconsistency, flagged but not addressed (not in the proposal's literal scope).

**Verified:** `build-native/` (`IMF_BUILD_NATIVE_RENDERER=ON` alone) full suite **451/451**. `build-text/` (both flags `ON`) full suite **475/476** — the one failure (`Config Set and Save round-trip`) confirmed a pre-existing, unrelated flake (passes standalone). Default `build/` (both flags `OFF`, doesn't even compile `Shaders/*.glsl`) **496/497** with the analogous `Config Set coalesces...` flake, also confirmed passing standalone. `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `13d253c`/`bca5230`, local feature branch deleted.

### Phase 33.8 status ✅ Complete — `Application::WithFont()` routes through `IRenderer::LoadFont()`

**Delivered:** Closes out `PHASE_33_PROPOSAL.md`'s final section, "ImGui Font Atlas Replacement." A design fork on `Application`'s **public** API (unlike every other Phase 33 sub-phase, which stayed internal-only) was presented to the user as an explicit choice before implementation — the generic-virtual approach was chosen.

- `src/Rendering/Renderers/IRenderer.hpp`: new pure-virtual `LoadFont(const Utility::Path&, float sizePixels) -> Result<Rendering::FontId>`. `Application::WithFont()` calls it on whichever renderer is active — never checks which concrete type.
- `src/Tree/Reconciler.hpp`: new `GetRenderer() -> IRenderer*` accessor (never null — defaults to `ImGuiCompatRenderer`).
- `ImGuiCompatRenderer::LoadFont()`: does the exact `ImFontAtlas::AddFontFromFileTTF()` load `Application.cpp` used to do inline, now also recording a `Rendering::FontId -> ImFont*` map; `Translate(DrawText)` was fixed to actually resolve `cmd.Font` against it (falling back to `ImGui::GetFont()` for invalid/unknown ids — the exact pre-33.8 behaviour, so every existing `DrawText` producer, e.g. `TextRO.cpp`, is unaffected).
- `NativeRendererGL3::LoadFont()`: forwards to its attached `ITextRenderer::LoadFont()` (new method on that interface too), or `Error::FontLoadFailed` if none attached. `TextRendererGL3::LoadFont()` already existed (Phase 33.7) — just marked `override`.
- `Application.hpp`/`.cpp`: `WithFont()`'s `Run()`-time loop now routes non-icon fonts through `_reconciler->GetRenderer()->LoadFont()`; new `LoadedFontId(std::size_t index) const noexcept` accessor exposes the resolved id (parallel-indexed to `_pendingFonts`, `FontId{}` for icon/empty-path/failed slots). `WithFont()`'s own signature is unchanged (still `Application&`, fluent chain preserved) — the id isn't known until `Run()`-time.
- **Deliberate scope boundaries** (documented, not oversights): icon fonts (`FontConfig::isIconFont`) stay ImGui-atlas-only regardless of active renderer — no MSDF glyph-range-merge equivalent exists or was requested. `TextRO.cpp` is untouched — no widget anywhere sets `DrawText::Font` yet; that needs a real design answer for "how does a widget discover which `FontId` to use," out of this proposal's literal scope.
- **Real bug found and fixed along the way**: the first test combining "load a real font at `Run()` time" with `TestHeadlessBackend` hit a genuine, previously-latent ImGui assert (`atlas->TexIsBuilt`) — adding a font after the atlas is built requires an explicit rebuild before the next frame, which `GLFWOpenGL3Backend`'s real backend tolerates (dynamic-texture support) but `TestHeadlessBackend` doesn't. This bug predates 33.8 (the old inline loading code had the identical gap); fixed in `Application::Run()` by rebuilding the atlas (`GetTexDataAsRGBA32()`, the same call `TestHeadlessBackend::Init()` itself already makes) whenever `!IsBuilt()` after the font loop.

**New tests:** `ImGuiCompatRenderer_test.cpp` (+2: `LoadFont` FileNotFound without needing a context, and a real-font-load-then-render test proving `ResolveFont()` is actually consulted — uses `Icons::Fa::House`, not a hand-typed PUA byte sequence, a mistake caught and fixed mid-writing this same sub-phase). `Application_test.cpp` (+1: `WithFont` with a real path resolves a valid `LoadedFontId` via the default renderer — this is the test that surfaced the `TestHeadlessBackend` atlas-rebuild bug above). `TextRendererGL3_test.cpp` (+2: `NativeRendererGL3::LoadFont` forwards when a text renderer is attached, fails when none is). `Application_test.cpp`'s `SpyRenderer` test double gained a `LoadFont()` override (required — `IRenderer` is no longer satisfiable without one).

**Verified:** All three build configs, full suites: default `build/` **496/496**, `build-native/` **451/451**, `build-text/` **475/475** (each excluding the pre-existing `ThreadPool stress` hang) — confirmed both before and after the `TestHeadlessBackend` bugfix (failure reproduced directly by running the test binary standalone, not assumed from the assert message). `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `edbcd1e`/`b3d22e0`, local feature branch deleted.

### Phase 33.7 status ✅ Complete — `TextRendererGL3` (real GL3 MSDF text rendering, end-to-end)

**Delivered:** `Backends/GLFWOpenGL3/TextRendererGL3.{hpp,cpp}` (new) — the concrete `ITextRenderer`/`ITextLayoutProvider` adapter, closing out the proposal's "DrawText Command Processing" section (split from 33.6, mirroring Phase 32's Rect precedent).

- `NativeRendererGL3.hpp` gained a small new abstract seam, `ITextRenderer` (`LayoutProvider()` + `RenderTextBatch(const Batch&)`), and `AttachTextRenderer(ITextRenderer*)` / a `_textRenderer` pointer member — nothing else. `NativeRendererGL3.cpp`/`.hpp` still have zero `FontRegistry`/`TextShaper`/`GlyphAtlas` dependency and still build/pass fully under `IMF_BUILD_NATIVE_RENDERER` alone (verified directly).
- `TextRendererGL3` owns `FontRegistry`+`TextShaper`+`GlyphAtlas`, a GL texture mirroring `GlyphAtlas`'s CPU pixel buffer, and a hand-written `#version 330 core` port of `Shaders/MSDFText.glsl` (median-of-three + `fwidth`-based screen-px-range AA, matching the GLSL source exactly).
- `LoadFont(Path, sizePixels) -> Result<FontId>` — the entry point a caller now has to obtain a real `FontId`.
- `LayoutText()` (the `ITextLayoutProvider` implementation): resolves the font, shapes via `TextShaper`, rescales HarfBuzz's advances/offsets from the font's *loaded* size to the requested `DrawText::FontSize` (a real correctness subtlety — the two can differ), converts `DrawText::Position` from its documented top-left-corner convention (matching `ImGuiCompatRenderer::Translate(DrawText)`'s existing behaviour) to a baseline via `FT_Face::size->metrics.ascender`, and emits one `GlyphQuad` per non-blank glyph in msdfgen's Y-up-to-screen-Y-down-converted space.
- `RenderTextBatch()`: `SyncAtlasTexture()` re-uploads the full glyph-atlas pixel buffer to a real `GL_TEXTURE_2D` whenever `TextureAtlas::Generation()` (new, this sub-phase — increments on `Upload()`/`Grow()`) has advanced since the last upload; then a standard bind-VBO/EBO-and-`glDrawElements()` call, matching `RenderRectBatch()`/`RenderImageBatch()`'s existing shape.
- `src/Rendering/Renderers/TextureAtlas.{hpp,cpp}`: added `Generation()` — the minimum mechanism needed to answer "has the CPU buffer changed since I last mirrored it to the GPU," not full dirty-rect tracking (a documented, deliberate simplification for a first working version).
- `Backends/GLFWOpenGL3/CMakeLists.txt`: `TextRendererGL3.cpp` compiled (and `Freetype`/`harfbuzz`/`msdfgen-ext`/`msdfgen-core` linked, for the same "PRIVATE link doesn't propagate through a static lib" reason as `GlyphAtlas`'s own test targets) only when **both** `IMF_BUILD_NATIVE_RENDERER AND IMF_BUILD_TEXT_MSDF` are `ON` — deliberately not auto-enabling one from the other (see `DECISIONS.md`).

**New test:** `Tests/Rendering/TextRendererGL3_test.cpp` — 3 cases: `LoadFont` on a missing path (`Error::FileNotFound`), `LayoutText` on an unknown `FontId` (invalid texture, no quads, no GL context needed), and the big one — a full end-to-end real-pixel render (`Icons::Fa::House` from the vendored FA6 font, through `NativeRendererGL3::AttachTextRenderer()`, read back via the same offscreen-FBO harness `NativeRendererGL3_test.cpp` established) proving real, opaque, correctly-colored MSDF coverage was drawn, with the far corner still fully transparent background. **Passed on the first attempt** against every design decision made across Phase 33.1–33.7. `TextureAtlas_test.cpp` gained one new case for `Generation()`.

**Verified:** New tests: 3/3 passing. `build-text/` reconfigured with **both** `IMF_BUILD_NATIVE_RENDERER=ON` and `IMF_BUILD_TEXT_MSDF=ON` for the first time this session — full suite **470/470**. `build-native/` (native-renderer alone, confirmed zero `TextRendererGL3.*` object files produced) **448/448**. Default `build/` (neither) **493/493**. All exclude the pre-existing, unrelated `ThreadPool stress` hang. `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `1103f20`/`f3e6318`, local feature branch deleted.

### Phase 33.6 status ✅ Complete — `BatchKind::Text`/`TextVertex` in `BatchBuilder`

**Delivered:** `src/Rendering/Renderers/BatchBuilder.{hpp,cpp}` gains text batching, split out from the proposal's single "DrawText Command Processing" section (see below) to mirror Phase 32's own Rect precedent (32.2 shader / 32.3 batch types / 32.4 GL3 rendering, three separate sub-phases).

- `TextVertex{Position, Uv, Color}` — matches `Shaders/MSDFText.glsl`'s `STAGE_VERTEX` inputs field-for-field (Phase 33.5).
- `BatchKind::Text` added alongside `Rect`/`Image`; `BatchVertices` variant gains `std::vector<TextVertex>`. No new `BatchFlushReason` needed — the existing reasons (`CommandTypeChange`/`TextureChange`/`NonBatchable`/`BufferFull`) already cover text.
- New abstract seam `ITextLayoutProvider` (`LayoutText(const DrawText&, vector<GlyphQuad>&) -> TextureId`) — keeps `BatchBuilder` free of any `FontRegistry`/`TextShaper`/`GlyphAtlas` #include. This matters concretely: `BatchBuilder` compiles unconditionally (no `IMF_BUILD_NATIVE_RENDERER`/`IMF_BUILD_TEXT_MSDF` gate at all), while the real text pipeline is gated behind `IMF_BUILD_TEXT_MSDF` alone — a hard #include would have forced two currently-independent build options into lockstep for every `BatchBuilder` consumer.
- `BatchBuilder::SetTextLayoutProvider(ITextLayoutProvider*)` — optional, non-owning. Unset (`nullptr`, the default) preserves the exact pre-33.6 behaviour: `DrawText` flushes as `BatchFlushReason::NonBatchable`, no batch produced. A provider that resolves `cmd.Font` to an invalid `TextureId` gets the identical treatment — matches `FontRegistry::Get()`/`GlyphAtlas::GetOrCreate()`'s existing "unknown id → null, caller degrades gracefully" idiom.
- `AppendText()`'s `kMaxBatchVertices` overflow check runs per-glyph-quad (not per-command, unlike `AppendRect`/`AppendImage`), since one `DrawText` can produce an unbounded number of quads for a long string.
- `DrawText::MaxWidth` (wrapping) is explicitly **not** implemented by any provider yet — documented in `ITextLayoutProvider`'s own doc comment; every `DrawText` lays out as one line regardless.
- `NativeRendererGL3::DrawBatches()`'s dispatch was extended from `if (Rect) ... else RenderImage(...)` to an explicit `switch` over the now-3-valued `BatchKind`, with `BatchKind::Text` a documented no-op — the old shape would have mis-dispatched a stray `Text` batch into `RenderImageBatch()` and crashed via `std::get`. Not reachable today (no provider is ever set on `NativeRendererGL3`'s own `_batchBuilder`), fixed proactively since it sits directly beside this sub-phase's own change.

**New test:** `Tests/Rendering/BatchBuilder_test.cpp` — added a test-only `FakeTextLayoutProvider` (no real font pipeline, no GL context; `cmd.Font.Value()==0` simulates "unknown font", otherwise returns one synthetic quad and treats `cmd.Font.Value()` itself as the resolved `TextureId`) and 4 new cases: valid layout produces a correct `Text` batch (vertex/index counts, UV corners, per-vertex `Color` propagation), an invalid-texture result skips the command exactly like no-provider-set, a successful `DrawText` between two `DrawRect`s flushes on command-type change on both sides, and two `DrawText`s with different resolved textures flush on texture change. The pre-existing "`DrawText` flushes as non-batchable" test (no provider set) is untouched and still passes.

**Verified:** New tests: 4 new cases, all passing. Full suite in all three of this session's configs: `build-native/` (`IMF_BUILD_NATIVE_RENDERER=ON`, `IMF_BUILD_TEXT_MSDF=OFF`) **447/447**; default `build/` (`IMF_BUILD_NATIVE_RENDERER=OFF`) **492/492**; `build-text/` (`IMF_BUILD_TEXT_MSDF=ON`) **455/455** — all excluding the pre-existing, unrelated `ThreadPool stress` hang. `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `8945751`/`d8d072d`, local feature branch deleted.

### Phase 33.5 status ✅ Complete — `MSDFText.glsl` shader (median-of-three + fwidth AA)

**Delivered:** `Shaders/MSDFText.glsl` — a `#version 450` `STAGE_VERTEX`/`STAGE_FRAGMENT` source, same single-file compile technique as `Rect.glsl`/`Image.glsl`/`Path.glsl`.

- Vertex stage: `inPosition` (pixel-space, transformed via the existing `PerFrame.ViewportSize` uniform block convention), `inUv` (MSDF atlas texcoord), `inTextColor` (per-vertex, not a uniform — see below).
- Fragment stage: samples `uAtlas`'s RGB channels, recovers the signed distance via `Median()` (max(min(r,g), min(max(r,g),b))), converts it to a screen-pixel distance via `ScreenPxRange()` — `fwidth(vUv)` against `uMsdf.PxRange`/`textureSize(uAtlas,0)`, Chlumsky's own reference MSDF shader technique, matching the proposal's specified "`fwidth()`-based" approach exactly — then `smoothstep(-0.5, 0.5, screenPxDistance)` for opacity, multiplied into `vTextColor.a`.
- `uMsdf.PxRange` (a small `binding=2` uniform block) must equal `Internal::GlyphAtlas::kPxRange` (4.0) whenever this shader is actually driven by `GlyphAtlas`-generated bitmaps — not yet wired up this sub-phase.
- Color is a per-vertex attribute (`inTextColor`), not a shader color uniform — a deliberate deviation from the proposal's literal wording, matching `Rect.glsl`/`Image.glsl`'s own per-vertex `inFillColor`/`inTintColor` convention so differently-colored glyphs can batch into one draw call.
- **Scope boundary (intentional):** no `BatchKind::Text`, no `TextVertex`, no `NativeRendererGL3` GL3 port, no `DrawText` processing this sub-phase — mirrors `Path.glsl`'s own Phase 32.2 precedent (a shader authored and compile-verified with no live consumer yet, because no batch kind/vertex format/renderer exists to feed it). That wiring is the proposal's separate "DrawText Command Processing" section, deferred to a later sub-phase.
- `CMakeLists.txt`: added `MSDFText` to the existing `IMF_BUILD_NATIVE_RENDERER`-gated `foreach(IMF_SHADER_NAME Rect Image Path MSDFText)` loop — independent of `IMF_BUILD_TEXT_MSDF` (the shader has zero harfbuzz/msdfgen/FontRegistry dependency, pure GLSL).
- `Tests/Rendering/ShaderPipeline_test.cpp`: added `MSDFText` vertex/fragment includes and assertions to both existing smoke tests (non-empty source/SPIR-V, correct SPIR-V magic number), alongside `Rect`/`Image`/`Path`.

**Verified:** Configured a fresh `build-native/` (`IMF_BUILD_NATIVE_RENDERER=ON`, `IMF_BUILD_TEXT_MSDF=OFF` — this shader needs neither harfbuzz nor msdfgen). Both `MSDFText.glsl` stages compiled and embedded via `glslangValidator` with zero errors on the first attempt. `ShaderPipeline_test.cpp`'s 2 tests pass with the new assertions included. Full suite in that config: **443/443** (excluding the pre-existing, unrelated `ThreadPool stress` hang). Default `build/` (`IMF_BUILD_NATIVE_RENDERER=OFF`) rebuilt successfully — unaffected, since every change is inside the existing `IMF_BUILD_NATIVE_RENDERER` gate. `check_api_leaks.sh` clean (no `src/`/`include/` files touched).

**Branch:** none (on `develop`) — merged `37c6f3e`/`f4126e7`, local feature branch deleted.

### Phase 33.4 status ✅ Complete — `GlyphAtlas` (real MSDF bitmap generation via msdfgen)

**Delivered:** `src/Rendering/Text/GlyphAtlas.{hpp,cpp}` (`ImFrame::Internal`) — `TextureAtlas` (Phase 32.1) gets its first live producer.

- `GlyphAtlas::GetOrCreate(FontRegistry&, FontId, glyphId) -> const GlyphAtlasEntry*` — on cache miss, adopts the exact `FT_Face` `FontRegistry` already owns via msdfgen's `adoptFreetypeFont()` (no second file load), loads the glyph EM-normalized, colors edges (`edgeColoringSimple`), computes bounds with a `kPxRange`-derived border, and generates a 4-channel MTSDF (`generateMTSDF`) at a fixed `kPixelsPerEm = 32.0` raster density — independent of whatever pixel size any `FontRegistry::Load()` call used, which is the entire point of MSDF (one bitmap serves any final on-screen size). Converts float→byte (`msdfgen::pixelFloatToByte`) and uploads via `TextureAtlas::Upload()`.
- `GlyphAtlasEntry{Texture, Region, UvMin, UvMax, SizeEm, BearingEm, IsBlank}` — `SizeEm`/`BearingEm` are in em units using msdfgen's Y-up convention; a future renderer placing the quad in ImFrame's top-down screen space must negate Y. `IsBlank` covers both a truly empty outline (space) and a degenerate (zero-area) bounding box.
- Cached by `(FontId, glyphId)` — glyph ids sourced from `TextShaper`'s shaping output (font-internal glyph indices, not Unicode codepoints), matching how the two are meant to be used together.
- **API surface fully validated by reading the real installed msdfgen headers directly** (`msdfgen.h`/`msdfgen-ext.h`/`Shape.h`/`Projection.h`/etc. under `build-text/vcpkg_installed/`), not from possibly-stale training memory — graphify doesn't index vcpkg-vendored third-party headers, so this was necessarily raw-file reading. First compile attempt succeeded against every API assumption made this way.
- `CMakeLists.txt`: `GlyphAtlas.cpp` added to the `IMF_BUILD_TEXT_MSDF` block; discovered `msdfgen::msdfgen-ext` alone left `Shape`/`Projection`/`generateMTSDF`/etc. unresolved (those live in `msdfgen::msdfgen-core`, confirmed via a real link failure) — both are now linked, matching vcpkg's own usage hint. Every test target touching these symbols (even transitively) also needs both re-linked directly, same as `glad::glad` already is for `NativeRendererGL3`'s tests — `ImFrame`'s own `PRIVATE` link doesn't propagate through the static `ImFrame.lib`.

**New test:** `Tests/Rendering/GlyphAtlas_test.cpp` — 6 cases, using real `Icons::Fa::*` constants (not hand-typed PUA bytes — an earlier draft used `U+F000` directly, which isn't an *assigned* FA6 icon and silently fell back to `.notdef`, a visible box outline in this font, causing two confusing failures until switched to `Icons::Fa::House`/`Star`). Covers invalid-id, sane dimensions + valid UV rect, cache-hit identity, distinct non-overlapping regions for distinct glyphs, and — the strongest test — **reads back the real uploaded bitmap** via `TextureAtlas::ReadRegion()` and reconstructs it (median-of-three, the same operation the eventual shader performs), confirming a real icon's center reconstructs as fully inside (255) and the bitmap's corner as fully outside (0). The blank-glyph test needed `C:/Windows/Fonts/calibri.ttf` (`SKIP()` if absent) rather than FA6, since FA6's own `.notdef` fallback for `" "` isn't actually blank.

**Verified:** New tests: 44 assertions / 6 test cases, all passing under both direct execution and `ctest` (including the Calibri-dependent test executing on this machine). Full suite `IMF_BUILD_TEXT_MSDF=ON`: **451/451** (excluding the unrelated `ThreadPool stress` hang). Full suite `IMF_BUILD_TEXT_MSDF=OFF`: **488/488** (unaffected). `check_api_leaks.sh` clean.

**Branch:** none (on `develop`) — merged `d915d40`/`f81712c`, local feature branch deleted.

### Phase 32.9 status ✅ Complete — `NativeRendererGL3` real GL texture support for `BatchKind::Image`

**Delivered:** `NativeRendererGL3` now actually renders `BatchKind::Image` batches instead of `IMF_ASSERT`-ing on them — the last piece of Phase 32's "GPU Renderer Foundation." A hand-written `#version 330 core` shader pair (`RenderImageBatch()`), ported from `Shaders/Image.glsl` the same way Phase 32.4 ported `Rect.glsl` (same `layout(binding=N)`-unavailable-in-330-core reasoning), samples the bound texture, applies `TintColor`, and reuses the same `RoundedBoxSdf` corner-rounding mask as the Rect shader.

- **Key design decision:** `Rendering::TextureId::Value()` is treated as a **raw GL texture name** (`GLuint`), not a registry index — `glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(batch.Texture.Value()))` directly. This matches how `Widgets::Image` (the only live `DrawImage` producer, Phase 32.8) already constructs its `TextureId` — a reinterpreted raw handle, not a registry-issued one — and how `ImGuiCompatRenderer`/ImGui's own OpenGL3 backend already interpret the identical bit pattern. Verified `TextureAtlas` (the only thing that *would* need registry semantics) has no live producer/consumer wiring anywhere, so a registry layer would be pure speculation. `TextureId`'s public doc comment (`include/ImFrame/Rendering/CommandBuffer.hpp`) was corrected to state this as the real, current contract — it previously said "never construct one directly — obtain it from `ImageLoader::Load()` or `Viewport::TextureId()`," neither of which exists.
- `RenderRectBatch()`/`RenderImageBatch()` now each bind their own program/VAO/viewport uniform at the start of the call (previously `DrawBatches()` bound the Rect program/VAO once, valid only when Rect was the sole kind) — `BatchBuilder` can legitimately interleave Rect and Image batches in one `CommandBuffer`, so `DrawBatches()` can no longer assume a single kind for the whole list.
- `DrawBatches()`'s GL state save/restore gained `GL_ACTIVE_TEXTURE`/`GL_TEXTURE_BINDING_2D` (texture unit 0) alongside the existing program/VAO/blend save-restore.
- The `BatchKind::Image` `IMF_ASSERT(false)` was removed entirely (not softened) — `BatchKind` has exactly two enumerators and both are now real; guarding against a value that can't exist would be defensive code this project's conventions avoid.

**New tests:** `Tests/Rendering/NativeRendererGL3_test.cpp` gained 4 new real-pixel test cases (real `glGenTextures`/`glTexImage2D` solid-blue texture, `ScratchFramebuffer` + direct `Render()`, `glReadPixels()` assertions): basic textured-quad + `TintColor` multiply, rounded-corner masking, and one interleaved-Rect-and-Image-batch test proving each batch renders with its own kind's geometry and shader.

**Verified:** New tests: 37 assertions / 7 test cases total in that file (3 pre-existing Rect + 4 new), all passing. Full suite `IMF_BUILD_NATIVE_RENDERER=ON` (fresh `build-native/` configure+build, deleted after verification): **443/443**, run with `ThreadPool stress` excluded (unrelated pre-existing hang — see Next Action above). Full suite `IMF_BUILD_NATIVE_RENDERER=OFF`: **488/488**, same exclusion, matches Phase 32.8's baseline exactly (this doc-comment-only change to a public header doesn't add any OFF-config test). `check_api_leaks.sh` clean before and after (the `TextureId` doc-comment fix only touches comment text, correctly excluded by the checker's comment-filtering).

**Branch:** none (on `develop`) — merged `7d099a1`/`64d9e0b`, local feature branch deleted.

### Phase 32.8 status ✅ Complete — `Widgets::Image` gets a real `DrawImage` producer

**Delivered:** `ImageElement::Paint()` (`src/Widgets/Image.cpp`) now splits on whether `OnClick` is set. Set (`PaintInteractive()`): unchanged — still calls `ImGui::ImageButton()` directly, matching the `Button`/`Checkbox`/etc. "interaction with no generic command equivalent" carve-out from Phase 31.2's `DECISIONS.md`. Unset (`PaintStatic()`): now pushes a real `Rendering::DrawImage` command instead of calling `ImGui::Image()` directly — the first live `DrawImage` producer anywhere in the tree since `ImGuiCompatRenderer`'s translation for it was built speculatively in Phase 31 and never exercised.

- `TextureId`'s value is `static_cast<std::uint64_t>(reinterpret_cast<uintptr_t>(texture))` — the same reinterpret-through-`void*` the old `ImGui::Image()` call already used, matching `TextureId`'s own documented "`ImGuiCompatRenderer` treats it as an `ImTextureID`" contract. No texture registry/`ImageLoader` was built — none exists yet, and building one now would be speculative (see `.claude/DECISIONS.md`, Phase 32.8).
- Two Phase 30 behaviours needed explicit, manual preservation since bypassing `ImGui::Image()` also bypasses what it did for free: tooltip hover-detection now uses an invisible `ImGui::Dummy()` at the image's rect (only when a tooltip is configured); disabled-state dimming is baked into the pushed command's `TintColor.w` by reading `ImGui::GetStyle().Alpha` inside a paired `BeginDisabled()`/`EndDisabled()`.
- `BorderColor` now draws as a separate stroke-only `DrawRect` at the image's own bounds — not a pixel-for-pixel match of `ImGui::Image()`'s `border_col` (which also pads outward by `ImGuiStyle::ImageBorderSize`), documented as an intentional minor fidelity gap; untested by any existing test, not exercised by any known caller.
- **Stale doc-comment fix:** `NativeRendererGL3.hpp`/`.cpp` said `DrawImage` "has no live producer anywhere in the tree yet" — now false, corrected to explain `BatchKind::Image` is a genuinely reachable (if still `IMF_ASSERT`-guarded) runtime path today, not a hypothetical one.

**New test:** `Tests/Tree/ReconcilerImage_test.cpp` — a real GL texture with known solid-red content, a non-interactive `ImageWidget`, a real `Reconciler::Show()` through the default `ImGuiCompatRenderer`, a full `BeginFrame()`/`EndFrame()` cycle, and a real `glReadPixels()` assertion confirming the image rect is red and the surrounding area isn't. Gated on `IMF_BACKEND_GLFW_OPENGL3` only (not `IMF_BUILD_NATIVE_RENDERER` — this path never touches `NativeRendererGL3`).

**Verified:** New test: 6 assertions / 1 test case, passing. Existing `ImageWidget` tests (`Tests/Tree/Widgets_test.cpp`, both the `OnClick`-set and `OnClick`-unset cases): unchanged, passing. Full suite (`IMF_BUILD_NATIVE_RENDERER=OFF`, the config this feature needs — no native renderer required): **488/488, zero failures**, run with `ThreadPool stress` excluded (see Next Action above for why — an unrelated pre-existing hang, not a regression from this phase; confirmed by running that one test in isolation separately in a prior session's baseline). `check_api_leaks.sh` clean before and after.

**Branch:** none (on `develop`) — merged `3801ef6`/`0f2bf40`, local feature branch deleted.

### Phase 32.7 status ✅ Complete — public `Application::UseRenderer()` API

**Delivered:** `Application& Application::UseRenderer(std::unique_ptr<Internal::IRenderer> renderer)`, forwarding to the existing `_reconciler->SetRenderer(std::move(renderer))`. This closes Phase 32.5's "no public API" gap — resolves item (b) from the Phase 32.5/32.6 punch list.

- **Key finding, not just an addition:** the Phase 32.5 `DECISIONS.md` row claiming a `unique_ptr<Internal::IRenderer>` public parameter is "exactly what the never-expose-Internal:: invariant forbids" was wrong. `Application.hpp`'s own constructor already takes `std::unique_ptr<Internal::IBackend> backend` today, and its own top-of-file example already shows a consumer writing `std::make_unique<ImFrame::Internal::GLFWOpenGL3Backend>()` directly — `check_api_leaks.sh`'s Check 2 greps for the literal substring `ImFrame::Internal::`, which `Internal::IBackend` (unqualified, resolved via enclosing-namespace lookup) never contains. Verified directly: added `Internal::IRenderer` the identical way, reran `check_api_leaks.sh` — still clean. See `.claude/DECISIONS.md`'s new Phase 32.7 section for the full analysis.
- Consequence of that finding: **no `Rendering::RendererHandle` Pimpl wrapper type was built** — it would have been a new abstraction solving a constraint that doesn't actually exist, given `IBackend`'s identical shape already ships. `UseRenderer()` is generic over any `Internal::IRenderer` (not renderer-specific, matching how the constructor is generic over any `IBackend`) — deliberately not the proposal's literal parameterless `UseNativeRenderer()`, which would need a registry/auto-dispatch mechanism across `NativeGraphicsContext` variants for backends (Vulkan/Metal/DX12/WebGPU `NativeRenderer*`) that don't exist yet — same "nothing to dispatch between yet" reasoning Phase 32.4 already used.
- `Application.hpp`: forward-declares `Internal::IRenderer` next to the existing `Internal::Reconciler` forward-declaration; `UseRenderer()` added to the fluent-builder section, doc comment shows the `NativeRendererGL3` opt-in usage.
- `Tests/App/Application_test.cpp`: new `SpyRenderer` (`Internal::IRenderer` subclass counting `Render()` calls) + `EmptyRootComponent`; new test drives a real `Application::Run()` (via `TestHeadlessBackend`, 3 frames) with `UseRenderer(make_unique<SpyRenderer>(...))` and `SetRoot()`, asserting `Render()` fired exactly 3 times — proves the wiring reaches `Reconciler::Show()`'s per-frame replay, not `NativeRendererGL3`'s own rendering correctness (already covered by Phase 32.4/32.6's dedicated pixel tests). `Tests/CMakeLists.txt`'s `ImFrame_Tests_App_Application` target gained a `${CMAKE_SOURCE_DIR}/src` include dir to reach `Rendering/Renderers/IRenderer.hpp`.

**Verified:** `IMF_BUILD_NATIVE_RENDERER=OFF` config (this feature needs no native renderer to exist — `Internal::IRenderer`/`ImGuiCompatRenderer` are always compiled): full suite **487/488** (one `Config` coalescing timing flake under full-suite load — the same pre-existing flake category documented in Phase 32.4's notes; reran in isolation immediately after: 1/1 passed). `check_api_leaks.sh` clean before and after. Consumer-usage line (`UseRenderer` + `NativeRendererGL3::RenderMode::DeferredReplay`) matches Phase 32.6's already-verified `ReconcilerNativeRenderer_test.cpp` path — not re-verified with real pixels through `Application` itself, since `Application` has no public pixel-readback for non-headless backends (`ReadHeadlessPixels()` is headless-only by design) and adding one would be scope creep unrelated to this API's own job.

**Branch:** none (on `develop`) — merged `c3112c8`/`dd0c6a7`, local feature branch deleted.

### Phase 32.6 status ✅ Complete — fixes the Phase 32.5 compositing blocker

**Delivered:** `NativeRendererGL3` gained a `RenderMode` (`Immediate` — default, unchanged Phase 32.4/32.5 behaviour; `DeferredReplay` — new). In `DeferredReplay`, `Render()` no longer draws synchronously — it snapshots the built batches into a heap-owned struct and pushes an `ImDrawList::AddCallback()` (plus a trailing `ImDrawCallback_ResetRenderState`) onto the *current* ImGui window's own draw list. `ImGui_ImplOpenGL3_RenderDrawData()` then invokes that callback at the exact point in the draw list where it was recorded — i.e. interleaved correctly with the window's own (also-deferred) content, fixing the z-order bug Phase 32.5 discovered and did not fix. Full design rationale in `.claude/DECISIONS.md`'s new "Phase 32.6" section.

- `NativeRendererGL3.hpp`/`.cpp`: added `RenderMode` enum + constructor parameter (defaults to `Immediate`, so all Phase 32.4 direct-FBO tests are unaffected with zero changes); extracted the shared "save GL state → bind program/VAO → iterate batches → restore GL state" logic into `DrawBatches()`, called both by `RenderImmediate()` and by the new deferred-callback path. Forward-declares `ImDrawList`/`ImDrawCmd` in the header (matching the `ImFont*`/`ImFontAtlas*` precedent in `IconFont.hpp`) so `<imgui.h>` stays confined to the `.cpp`.
- `Tests/Tree/ReconcilerNativeRenderer_test.cpp`: rewritten to run the *full* `BeginFrame() → Show() → EndFrame()` lifecycle (Phase 32.5's version deliberately read pixels mid-frame to document the bug without hitting it). Two test cases: one constructs `NativeRendererGL3(RenderMode::Immediate)` and confirms the Box is *still* erased after a full `EndFrame()` (the bug, reproduced end-to-end rather than sidestepped); the other constructs `RenderMode::DeferredReplay` and confirms the Box *survives* — the actual fix proof.

**Verified:** New test file: 5 assertions / 2 test cases, both passing. `Tests/Rendering/NativeRendererGL3_test.cpp` (all `Immediate`-mode, unaffected by the refactor): 20 assertions / 4 test cases, unchanged, passing. Full suite `IMF_BUILD_NATIVE_RENDERER=OFF`: **487/487, zero flakes** (matches the Phase 32.5 baseline exactly — this config doesn't even compile `NativeRendererGL3`). Full suite `IMF_BUILD_NATIVE_RENDERER=ON` (fresh `build-native/` configure+build, deleted after verification — not part of the repo): **439/439, zero flakes** (test count differs from Phase 32.5's documented 492/493 because this was a from-scratch configure, not the same build tree — zero failures either way, no flake reproduced). `check_api_leaks.sh` clean (change is entirely in `Backends/`/`Tests/`, outside its `include/ImFrame/` scope anyway).

**Not done, and why:** items (b) and (c) from Phase 32.5's list remain open — see Next Action above. `DeferredReplay` does not help `Application`-level public API design by itself; it only removes the compositing blocker that made such a design premature.

**Branch:** none (on `develop`) — merged `8366f43`/`963cb94`, local feature branch deleted.

### Phase 32.5 status ✅ Complete, merged into `develop` — the one to read carefully before touching `NativeRenderer` again

**Two real things were fixed/built, and one real, deeper problem was found and *not* fixed — read all three before assuming `NativeRenderer` is usable end-to-end:**

1. **Fixed: clear-timing bug.** `GLFWOpenGL3Backend`'s `glViewport`/`glClearColor`/`glClear` moved from `EndFrame()` to `BeginFrame()`. Traced directly: `Application::Run()`'s frame tick is `BeginFrame() → Reconciler::Show() [→ renderer->Render()] → EndFrame()` — the old clear (in `EndFrame()`) ran *after* `Show()`, so any real GL draw a swapped-in `NativeRendererGL3` issued was unconditionally erased before the buffer swap. Moving the clear earlier is transparent to ImGui's own full-redraw-every-frame rendering. Verified via full regression suite before/after (zero new failures, both configs).
2. **Fixed: `Reconciler` renderer injection.** `Reconciler`'s fixed `ImGuiCompatRenderer` member is now `std::unique_ptr<IRenderer>` (defaults identically), with a new `Internal`-only `SetRenderer()`. This was Phase 31.3's own named deferral point ("when Reconciler first needs to actually select between two implementations") — now justified.
3. **Discovered, not fixed: deferred-rasterization compositing bug.** `RootBridge::BeginRootWindow()`'s `ImGui::Begin()` *queues* the root window's own background into ImGui's draw list; ImGui only *rasterizes* that queued content later, in `EndFrame()`. `NativeRendererGL3::Render()` draws **immediately**. Reproduced directly: a `Box` drawn via `Reconciler::Show()` + `NativeRendererGL3` is correct if read via `glReadPixels()` **immediately after `Show()` returns**, but by the time `EndFrame()` finishes, the window's own deferred background has painted over it. **Any deferred (ImGui) content in a frame always rasterizes after any immediate (`NativeRenderer`) content, regardless of program order.** This is real production-code behavior (`RootBridge.cpp`), not a test artifact, and supersedes the vaguer Phase 32.1 "how does compositing work" open question with a concrete, root-caused blocker. Fixing it needs `NativeRenderer` to gain a deferred-replay mode (hook in around `ImGui::Render()`/`RenderDrawData()`) or a full offscreen-compositing redesign — real, undesigned work, not attempted here.

**Also *not* done, and why:** no public `Application::UseNativeRenderer()` — `Internal::IRenderer` cannot appear in the public `Application.hpp` per `.claude/CLAUDE.md`'s own "never expose `Internal::` in public headers" rule, and `Application`/`src/App/` cannot construct a concrete `NativeRendererGL3` itself without depending on `Backends/GLFWOpenGL3` (reverse of the established dependency direction). A real public API needs a new public (non-`Internal::`) renderer handle type — genuinely undesigned, not a quick addition.

**Verified:** `Tests/Tree/ReconcilerNativeRenderer_test.cpp` — reads pixels immediately after `Show()` (not via `backend.EndFrame()`/`ReadPixels()`) specifically to test what's actually true without asserting the false stronger claim; 5 assertions, passing. Full suite both configs: `ON` 493/494 (`ThreadPool stress` — pre-existing flake), `OFF` **487/487, zero flakes**. `check_api_leaks.sh` clean.

### Phase 32.1 status ✅ Complete, merged into `develop`

**Delivered:** `src/Rendering/Renderers/{BatchBuilder,TextureAtlas,ClipStack}.{hpp,cpp}` — the three pure-CPU supporting types from `PHASE_32_PROPOSAL.md`'s "What Gets Built" section, built with **no GPU calls and no backend dependency**, matching how Phase 31.1 shipped `CommandBuffer` alone before any renderer consumed it.

- `BatchBuilder` — walks a finished `CommandBuffer`, groups adjacent `DrawRect`/`DrawImage` into `Batch` records (GPU-ready vertex/index data), flushing on type/texture/clip/layer change, non-batchable commands (`DrawPath`/`DrawText`/`DrawShadow`), or a `kMaxBatchVertices` cap. Exposes `BatchStats` (draw-call count, vertex count, flush-reason histogram) for the future `PerfOverlay` integration.
- `TextureAtlas` — shelf-packing atlas, **grows by doubling height only** (not the proposal's literal square "1024×1024 doubling to 4096×4096") specifically so previously issued `AtlasRegion`s never move and never need re-uploading; owns a real CPU-side RGBA8 pixel buffer (`Upload`/`ReadRegion` round-trip correctly). Does **not** vendor `rectpack2D` as the proposal names — uses a self-written shelf packer instead (Claude Code does not fetch/vendor unreviewed third-party source; see `DECISIONS.md`).
- `ClipStack` — nested auto-intersecting clip-rect stack; tracks `ClipKind::AxisAligned` vs `Rounded` (OR'd up the chain) for the scissor-vs-stencil decision a real backend will need to make later.

**Verified:** 21 new test cases / 82 assertions, all passing. Full suite: 483/484 (pre-existing `Config` flake, confirmed clean in isolation). `check_api_leaks.sh` clean.

### Phase 32.2 status ✅ Complete, merged into `develop`

**Delivered:** the build-time shader pipeline from `PHASE_32_PROPOSAL.md`'s "Shader Cross-Compilation Pipeline" section, gated behind a new `IMF_BUILD_NATIVE_RENDERER` CMake option (default `OFF`, mirroring the Vulkan/Metal/DX12/WebGPU backend-gating pattern — see `DECISIONS.md`).

- `vcpkg.json` — new `native-renderer` manifest feature requesting `glslang[tools]` (the standalone `glslangValidator` GLSL→SPIR-V compiler). Confirmed installs and builds successfully on this machine (`glslang[core,tools]@16.3.0`).
- `cmake/CompileShader.cmake` + `cmake/EmbedShaderSource.cmake` — `compile_shader()` runs `glslangValidator -S <vert|frag> -V -D<STAGE_DEFINE>` (build fails loudly on invalid GLSL), then embeds **both** the raw GLSL source and the compiled SPIR-V binary (`constexpr unsigned char[]`, `alignas(4)`) into a generated header per shader/stage.
- `Shaders/{Rect,Image,Path}.glsl` — real, glslang-validated GLSL 450, each combining vertex+fragment stages behind `#if defined(STAGE_VERTEX)`/`#elif defined(STAGE_FRAGMENT)` guards. `Rect`/`Image` use Inigo Quilez's rounded-box SDF for `CornerRadii`; `Path` is a flat-colour shader for CPU-tesselated stroke geometry (no live producer yet — see 32.3 below).

**Verified:** all 6 shader/stage combinations compiled with zero glslangValidator errors. `ShaderPipeline_test.cpp` — 24 assertions confirming non-empty source/SPIR-V and the correct SPIR-V magic number. `check_api_leaks.sh` clean.

### Phase 32.3 status ✅ Complete, merged into `develop`

**Delivered:** `BatchBuilder`'s Phase 32.1 generic `Vertex{Position, Uv, Color}` — written before the Phase 32.2 shaders existed and matching neither of them — replaced with `RectVertex`/`ImageVertex`, each matching `Rect.glsl`/`Image.glsl`'s `STAGE_VERTEX` inputs field-for-field. `Batch::Vertices` is now `std::variant<std::vector<RectVertex>, std::vector<ImageVertex>>`. `DrawPath` stays non-batchable (no live producer yet — CPU polyline tesselator deliberately deferred).

**Verified:** 11 test cases / 72 assertions, all passing. Full suite: 486/487 (`ThreadPool stress` timeout — pre-existing flake). `check_api_leaks.sh` clean.

### Phase 32.4 status ✅ Complete, merged into `develop`

**Delivered:** `Backends/GLFWOpenGL3/NativeRendererGL3.{hpp,cpp}` — the first real `IRenderer` implementation that issues genuine `glDrawElements()` calls (not `ImDrawList` calls). Scoped to `BatchKind::Rect` only — `DrawImage` still has no live producer, same precedent as Phase 32.3; an `Image` batch trips `IMF_ASSERT` rather than being silently dropped.

- Lives in `Backends/GLFWOpenGL3/`, **not** `PHASE_32_PROPOSAL.md`'s literal `src/Rendering/Renderers/Backends/` path — it needs `<glad/glad.h>`, and this project's own architecture invariants restrict GL headers to `Backends/` (documented deviation, see `DECISIONS.md`).
- Uses **hand-written `#version 330 core` GLSL** (runtime-compiled via `glCompileShader()`), not the Phase 32.2 `#version 450`/SPIR-V pipeline — `GLFWOpenGL3Backend` requests a 3.3 core context, which doesn't support `Rect.glsl`'s `layout(binding=N)` qualifiers without an extension. This is the literal meaning of the proposal's "OpenGL3 backend uses GLSL with a compatibility profile" clause, not an oversight — `RoundedBoxSdf` is now kept in sync by hand across 3 copies (`Rect.glsl`, `Image.glsl`, this file).
- `Render()` reads the target viewport via `glGetIntegerv(GL_VIEWPORT, ...)` at call time rather than `IRenderer::Render()` gaining a parameter (avoids touching `ImGuiCompatRenderer`'s signature for a feature only one implementation needs). Saves/restores GL state it touches (current program, VAO, blend state) so it can coexist in a frame with other GL/ImGui drawing later.
- No `NativeRenderer` dispatcher class was built — `NativeRendererGL3` is used directly, mirroring how `ImGuiCompatRenderer` was directly usable for two sub-phases before any `Application`-level `unique_ptr<IRenderer>` selection existed (same "nothing to dispatch between yet" reasoning).

**Verified:** `Tests/Rendering/NativeRendererGL3_test.cpp` — real pixel tests via a hand-created offscreen FBO (not `GLFWOpenGL3Backend::BeginFrame()`/`EndFrame()`, whose internal `glClear()` ordering would erase a direct GL draw issued before it — documented in the test file). 4 test cases / 20 assertions: filled-rect colors, stroke-without-fill, rounded-corner SDF correctness, and multi-batch clip-boundary rendering — all real `glReadPixels()` assertions, not just "compiles." `IMF_BUILD_NATIVE_RENDERER=ON` full suite: 492/493 (`Config` coalescing flake, confirmed clean in isolation — the *other* known pre-existing flake category, distinct from `ThreadPool stress`). Default `OFF` config full suite: **487/487, zero flakes**. `check_api_leaks.sh` clean both configs.

**Branch:** none (on `develop`) — merged `6408507`/`eaafd48`, local feature branch deleted.

---

## Phase 31 — Render Command Buffer (all sub-phases) ✅ Complete, merged

### Phase 31 closing summary

**Delivered:** `Rendering::CommandBuffer` (the full command set from the proposal: `DrawRect`/`DrawText`/`DrawImage`/`DrawPath`/`DrawShadow`/clip and layer push-pop), `Internal::IRenderer`/`ImGuiCompatRenderer` (complete translation to real `ImDrawList` calls, verified with actual pixel assertions via `ReadPixels()` on 3 real backends), and `Box`/`Text` — the only two primitives with genuine draw-list-only `Paint()` bodies — migrated to emit commands instead of calling ImGui directly. `Element::Paint()`'s signature changed project-wide (`Rendering::CommandBuffer& cmd` as its first parameter) to thread the buffer through every node, even the ~30+ `Element`s that don't populate it.

**Deliberately not delivered** (each a documented decision in `.claude/DECISIONS.md`, not a discovered gap):
1. Every other `Element` — `GestureRegion`, `VirtualList`, `RootBridge`, `DockSpace`, and the **entire** `Widgets::*`/`Overlay::*`/`Rendering::*Widget` library (`Button`, `Checkbox`, `Slider<T>`, `TextInput<T>`, `Combo<T>`, `Radio`, `Separator`, `ProgressBar`, `ColorEdit`, `Image`, `Modal`, `ContextMenu`, `Viewport`/`Canvas2D`/`Viewport3D` wrappers) — still calls ImGui directly. Confirmed by direct survey (not assumed) that each of these either draws nothing, or its drawing is inseparable from ImGui's own interaction/window state (e.g. `ImGui::Button()` handles hit-testing and drawing as one opaque call).
2. `PushOpacityLayer`/`PushBlendLayer`/`PopLayer` are structural no-ops — real per-layer compositing needs an offscreen render target ImGui's draw-list model doesn't have.
3. `DrawPath`'s `FillRule` isn't honored (convex-polygon approximation only).
4. `FontId` isn't wired to any font registry (`ImGui::GetFont()` always used) — that's Phase 33's job.
5. No `Application`-level swappable `unique_ptr<IRenderer>` — `Reconciler` hardcodes `ImGuiCompatRenderer` directly, since there's only one implementation to choose from today. Promote this when Phase 32's `NativeRenderer` exists.
6. The "zero visible change" invariant is verified via self-consistency (real pixel assertions through real backends) rather than a true zero-diff comparison against a captured v2.0.0 reference-image baseline — none was captured, per the user's explicit decision to proceed without that prep step.
7. `Examples/DemoApp`'s `TreePanel` now reaches into `Internal::ImGuiCompatRenderer` directly (a new `${CMAKE_SOURCE_DIR}/src` private include on the `DemoApp` target) — there is no public API today for a consumer to replay a `CommandBuffer` outside `Application::SetRoot()`'s single-fixed-window path.

**A real, pre-existing bug was found and naturally fixed** (not patched separately): `BoxRO.cpp`'s old border-stroke call used a stale ImGui `AddRect` parameter order, silently passing `thickness=0`. The migration to `CommandBuffer` replaced that call entirely with correctly-written-from-scratch translation logic in `ImGuiCompatRenderer`, verified by a real pixel test (a 4px green border is now actually detected in `ReadPixels()` output).

**Doc corrections made along the way:** `Docs/Architecture.md` (written in Phase 30.7) claimed `ButtonWidget` etc. are `GestureRegion` compositions — Phase 31.3's survey disproved this, and the doc has been corrected plus expanded to describe the new `CommandBuffer`/`IRenderer` layer.

**Full-suite verification (final, on `develop` post-merge):** 458/458 passed, zero flakes, 2 expected `IconFont` skips, `check_api_leaks.sh` clean, graphify updated (5570 nodes, 7185 edges, 414 communities). **`DemoApp` was never visually verified in a running window** this phase — it's a native GLFW app and this session has no way to screenshot native windows; the pixel-level conformance tests (real color assertions on real backends) are the actual correctness evidence, not a human glance at a screenshot.

**Next Action:** Phase 31 is done as a body of work. Next per the roadmap is **Phase 32** (native GPU renderer — `NativeRenderer` implementing `IRenderer`, per `PHASE_31_PROPOSAL.md`'s "What Phase 32 Inherits"). Before starting it, whoever picks this up should re-read the Phase 31 closing summary above — `CommandBuffer` covers exactly `Box`/`Text` today, nothing else, by design; Phase 32 inherits a narrower surface than a literal reading of "the command buffer specification" might suggest.

**Branch:** none (on `develop`)

---

## Phase 31.3 — Thread `CommandBuffer` Through `Element::Paint()`; Migrate Box/Text ✅ Complete

Branch `feature/phase-31.3-paint-migration` (merged: commit `5ffbf93`, merge into `develop`). Third and largest sub-phase of Phase 31 — changing `Element::Paint()`'s signature and letting the compiler enumerate every broken override surfaced **~20 additional files** beyond the 14 already known from the Phase 31.1 survey: `src/Widgets/*.cpp` (`Button`, `Checkbox`, `Radio`, `Separator`, `ProgressBar`, `ColorEdit`, `Image`, plus templated `Slider<T>`/`TextInput<T>`/`Combo<T>`) and `src/Overlay/*.cpp` (`Modal`, `ContextMenu`) each define their OWN concrete `Element` calling ImGui directly — **not** `GestureRegion` compositions as `Migration_v1_to_v2.md`/`Architecture.md` claimed.

**What was built:** `Element.hpp`'s new `Paint(Rendering::CommandBuffer&, Vec2)` signature. `BoxRO.cpp`/`TextRO.cpp` — real migration to push `DrawRect`/`DrawText` commands (both files dropped their now-dead ImGui includes). `VirtualListRO.cpp` — the one real design decision: rows record into a *locally-scoped* buffer flushed via a fresh `ImGuiCompatRenderer` before `EndChild()`, not the frame-wide buffer, to avoid replaying onto the wrong draw list after the child window closes. `Reconciler.{hpp,cpp}` — owns a reused `CommandBuffer` + fixed `ImGuiCompatRenderer`; `Show()` resets, paints (main tree + portals into the same buffer), then replays once. ~20 mechanical signature-only files delegated to a background agent (verified via full-solution rebuild afterward, zero remaining mismatches).

**Verified:** 457/458 passed (1 pre-existing `Config` flake, confirmed clean in isolation), all `ConformanceApp` tests (including `TableWidget`/`VirtualList`) pass with real pixel output, `check_api_leaks.sh` clean, graphify updated (5568 nodes, 7183 edges, 407 communities).

---

## Phase 31.2 — `IRenderer` Interface + `ImGuiCompatRenderer` ✅ Complete

Branch `feature/phase-31.2-imgui-compat-renderer` (merged: commit `e023672`). `src/Rendering/Renderers/IRenderer.hpp` (new) — `Render(const CommandBuffer&)` + `Shutdown()`. `ImGuiCompatRenderer.{hpp,cpp}` (new) — translates every `Command` variant to real `ImDrawList` calls (`DrawRect`→`AddRectFilled`/`AddRect`, `DrawText`→`AddText`, `DrawImage`→`AddImage`/`AddImageRounded`, `DrawPath`→stateful `Path*` API convex approximation, `DrawShadow`→6 concentric fading rings, `PushClipRect`/`PopClipRect`→matching calls; opacity/blend layers are structural no-ops, no producer needs them yet). `Tests/Rendering/ImGuiCompatRenderer_test.cpp` (4 tests, 19 assertions) — real pixel verification via `GLFWOpenGL3Backend`+`ReadPixels()`.

Two real discoveries: (1) this ImGui build (1.92.8) defines `ImTextureID = ImU64`, not `void*` — `TextureId` converts directly via `ImTextureRef{static_cast<ImTextureID>(...)}`. (2) Found a pre-existing bug in `BoxRO.cpp`'s border-stroke `AddRect` call (stale parameter order → `thickness=0`) — documented, not patched separately, since Phase 31.3 deletes the line by migrating `BoxElement` off direct ImGui calls entirely.

Verified: 458/458 passed, zero flakes, `check_api_leaks.sh` clean, graphify updated (5526 nodes, 7139 edges, 409 communities).

---

## Phase 31.1 — `CommandBuffer` Core Type ✅ Complete

Branch `feature/phase-31.1-command-buffer` (merged). First sub-phase of Phase 31 ("Render Command Buffer" — inserts an abstract command buffer between the widget tree and ImGui, per `PHASE_31_PROPOSAL.md`). Proceeds without a captured v2.0.0 reference-image baseline, per explicit user decision (see `DECISIONS.md`, Interstitial section).

**Scope finding (important, shapes all of Phase 31):** A dedicated survey of every `Paint()` override in `src/Tree/RenderObjects/*.cpp` found only **`BoxElement`/`TextElement`** do real draw-list work (`AddRectFilled`/`AddRect`/`AddText`). Everything else either draws nothing at all (`Expanded`/`Flex`/`Stack`/`Grid`/`SizedBox`/`Spacer`/`Portal` — pure structural pass-through) or is 100% ImGui widget/interaction/window state with zero draw-list calls (`GestureRegion`'s hit-testing, `VirtualList`'s scroll-driven virtualization, `RootBridge`'s window bracketing, `DockSpace`/`DockSpaceWidget`'s dock-builder tree). **Phase 31 only routes Box/Text through `CommandBuffer`** — everything else keeps calling ImGui directly, unchanged. Deliberate, documented scope-down from the proposal's literal "every RenderObject" framing.

**What was built:** `include/ImFrame/Rendering/CommandBuffer.hpp` (new, public, header-only) — the full command set from the proposal (`DrawRect`/`DrawText`/`DrawImage`/`DrawPath`/`DrawShadow`/clip/layer push-pop as a `std::variant`), `TextureId`/`FontId` opaque handles, `CommandBuffer` class (`Push`/`Reset`/`Size`/`Capacity`, iterable). No separate `.cpp` (every method trivial). `Tests/Rendering/CommandBuffer_test.cpp` (4 tests, 1620 assertions). Verified: 454/454 passed, `check_api_leaks.sh` clean, graphify updated (5481 nodes, 7081 edges, 408 communities).

---

## Phase 30.7 — Documentation Rewrite + v2.0.0 Release-Criteria Checklist ✅ Complete

Branch `feature/phase-30.7-documentation` (merged: commit `0c152bb`, merge `eff9c70`). All 7 documentation pages required by the proposal, in `Docs/`: `GettingStarted.md` (full rewrite — was a Phase-0 placeholder — complete `Tree::State`-backed counter app in ~20 lines), `Architecture.md` (new — three-layer model + dependency diagram), `WidgetReference.md` (new — every `Component`/`PrimitiveWidget` type with usage examples), `StateManagement.md` (new — `State`/`Signal`/`Computed`/`InheritedWidget` + `WidgetTestDriver` testing section), `RenderingSurfaces.md` (new — `Viewport`/`Canvas2D`/`Viewport3D`), `BackendGuide.md` (new — CMake link-time backend selection), `Migration_v1_to_v2.md` (reviewed, no changes needed). All grounded in a fresh full API survey before writing.

**v2.0.0 Release Criteria (honest status, from `PHASE_30_PROPOSAL.md`):**

| # | Criterion | Status |
|---|---|---|
| 1 | Zero deprecated symbols in public headers | ✅ Done — Phase 30.2 deleted all Phase 10–14 deprecated API |
| 2 | `check_api_leaks.sh` extended audit passes — no ImGui symbols in public API | ✅ Done — Check 3 already covers `ImVec*`/`ImGui*`/`ImDrawList`/`ImFont*`/`ImTextureID`, passing clean every phase this session |
| 3 | All five backends pass conformance tests within pixel threshold | ❌ **Open gap** — `HeadlessBackend::ReadPixels()` is still a zero-filled stub (Phase 36's `SoftwareRenderer` not yet built); Metal can't compile on this machine (no macOS toolchain); DX12 is off by default in this cache. Phase 30.6 delivered self-consistency conformance across the 3 buildable backends instead — a real, tracked, honestly-documented gap, not silently closed |
| 4 | Reconciler performance audit: no O(n²) case, no memory growth under stress | ✅ Done — Phase 30.5, 5 adversarial regression tests, all passing |
| 5 | `WidgetTestDriver` available and documented | ✅ Done — built Phase 30.4, now documented in `StateManagement.md` |
| 6 | Migration guide complete and reviewed | ✅ Done — reviewed this phase, accurate through Phase 30.6 |
| 7 | All documentation pages written | ✅ Done — this phase |
| 8 | All sanitizer jobs (ASan, TSan, UBSan) pass clean | ⚠️ **Unverified this session** — the `sanitize` CI job (`.github/workflows/ci.yml`) is configured and runs all three on `ubuntu-latest`, but this is a Windows dev machine and the job was not run locally or checked against live CI in this session. Not claimed as passing without evidence |
| 9 | Demo application updated to use v2.0.0 API exclusively | ✅ Done — Phase 30.2 migrated `Examples/DemoApp/main.cpp` |

**7 of 9 criteria are met. v2.0.0 should not be tagged until #3 and #8 are resolved** (#3 needs Phase 36; #8 needs either a Linux CI run confirmed green, or a local WSL/Docker sanitizer run this session didn't attempt).

**User decision (explicit):** asked directly whether to tag v2.0.0 now with the 2 gaps documented, check CI status first, or wait — chose **"don't tag yet."** No git tag has been created (unchanged as of Phase 31.1 — see current Resumption Snapshot above).

---

## Phase 30.6 — Backend Conformance Expansion (widget-tree coverage; cross-backend pixel-diff still deferred) ✅ Complete

Branch `feature/phase-30.6-backend-conformance` (merged). **The proposal's literal design (cross-backend pixel-diff against a real `HeadlessBackend` render, all 5 backends, 3-per-channel threshold) is NOT achievable and was not attempted** — the user was asked directly and chose to scope down. `HeadlessBackend::ReadPixels()` still returns a zero-filled buffer that's never written (real rasterization is Phase 36's `SoftwareRenderer`, not yet built); Metal cannot compile on this machine (no macOS toolchain); DX12 is off by default in this cache.

**What was built:** `GLFWOpenGL3Backend` gained a new `ReadPixels()` method (captured during `EndFrame()`, row-flipped to normalise origin — a real, small backend change). `Tests/Backends/ConformanceApp.hpp` (new, shared fixture) touches a representative cross-section of the widget library. `Tests/Backends/OpenGL3Conformance_test.cpp` (new, 4 tests) plus 2 new test cases each in `VulkanConformance_test.cpp`/`WebGPUConformance_test.cpp`/`DX12Conformance_test.cpp` render `ConformanceApp` across all 4 themes and 3 layout sizes, asserting no crash / correct size / non-uniform (something was drawn) / fully opaque. Real platform quirk found and fixed: `GLFWOpenGL3Backend`'s "headless" window is actually a real, briefly-visible OS window whose actual framebuffer size can differ from the requested `WindowConfig` — tests now query `IBackend::WindowSize()` rather than assume it matches.

**Verified:** 450/450 passed, zero flakes, `check_api_leaks.sh` clean, graphify updated (5321 nodes, 6891 edges, 386 communities). `DX12Conformance_test.cpp`'s expansion was written but not locally built (`IMF_BUILD_DX12_BACKEND=OFF`), matching the existing Metal precedent.

---

## Phase 30.5 — Reconciler Performance Audit (test-only; no algorithm changes) ✅ Complete

Branch `feature/phase-30.5-reconciler-audit` (merged). Test-only phase: the audit read `Internal::ReconcileChildren`/`ReconcileChild` and `ComponentElement<T>`'s dirty-flag coalescing line-by-line before writing any test and found both already satisfy every adversarial case in `PHASE_30_PROPOSAL.md` — zero algorithm changes were needed, only new regression/stress coverage in `Tests/Tree/ReconcilerPerformance_test.cpp` (5 tests: 100-level deep nesting, 10,000-item flat list O(n) scaling, 2,000 alternating widget types, 1000 `State::Set()` calls coalescing to 1 rebuild, 10,000-frame structural churn). Verified: 442/442 passed, `check_api_leaks.sh` clean, graphify updated (5277 nodes, 6813 edges, 386 communities).

---

## Phase 30.4 — `WidgetTestDriver` Headless Test Harness (logic mode only) ✅ Complete

**Branch:** `feature/phase-30.4-widget-test-driver` (merged: commit `c8573c1`, merge `5a1f653`). A second, independent full verification pass was run fresh on top of the merge commit to confirm nothing regressed post-merge: full Release rebuild clean, **437/437 tests passed** with **zero flakes** (2 expected `IconFont` skips), `check_api_leaks.sh` clean, graphify updated (5228 nodes, 6729 edges, 381 communities).

**Why this isn't a literal implementation of the proposal's prose:** three research agents surveyed the actual `Tree::Element`/`Tree::Widget` architecture before any design was committed to (see `DECISIONS.md` for full findings). Two real gaps between the proposal and what's buildable today: (1) `Tree::Element` has **no child-traversal API at all** — only `Parent()`, one direction — so a literal runtime string/index `path` system would require a new virtual hook on `Element` itself, a core-module change not attempted here; (2) neither `Internal::HeadlessBackend::ReadPixels()` nor `Rendering::HeadlessViewport::ReadPixels()` perform real ImGui rasterization today (both confirmed via full reads — one always zero-filled, the other only reflects manually-written test content), so a "rendering" test mode would assert on nothing meaningful. Given this, Phase 30.4 implements the "logic" test mode only, with compile-time type-directed navigation instead of runtime paths, and explicitly defers "rendering" mode to whenever real headless rasterization exists (Phase 31's command-buffer work is a candidate, per the proposal's own "What Phase 31 Inherits" section).

**What was built:** `include/ImFrame/Tree/WidgetTestDriver.hpp` (new, public, header-only) — `WidgetTestDriver<T>`, `FindDescendant<Target>(Widget)`, `SimulateClick(Widget)`. `driver.Component()` gives direct access to the top-level component under test for state inspection instead of a `State<T>(path)` accessor. `Tests/Tree/WidgetTestDriver_test.cpp` (new, 5 tests). Two real issues caught and fixed: a `check_api_leaks.sh` Check 2 failure from a fully-qualified `ImFrame::Internal::ChildrenOf(...)` call site (fixed by calling it unqualified), and a `[[nodiscard]]` warning on a discarded `SimulateClick(...)` return (fixed by wrapping in `REQUIRE(...)`).

---

## Interstitial — `Widget(...)` Implicit-Conversion Cleanup (between Phase 30.3 and 30.4) ✅ Complete

**Branch:** `refactor/simplify-widget-implicit-conversion` (merged) | **Tests:** 431/432 passed under `-j4` (excluding the documented pre-existing `ThreadPool stress` hang) — the 1 failure (`Logger SetMinLevel suppresses entries below the threshold`) was the same recurring `-j4` shared-resource-flake category as the `Config` tests in every prior phase, confirmed clean in isolation. 2 expected `IconFont` skips.

The user pointed out that wrapping every primitive/component in `Widget(...)`/`Tree::Widget(...)` before handing it to a `Build()` return, `Children({...})` list, or `Child()`/`Content()` setter felt unnatural compared to conventional declarative-UI trees (Flutter/SwiftUI). Investigation confirmed `Tree::Widget`'s converting constructor is deliberately **non-`explicit`** (`Widget.hpp`, `NOLINT(google-explicit-constructor) — intentional implicit conversion`) — the wrapping was never technically required, just a convention copy-pasted from Phase 27's first examples and never revisited. Verified empirically by compiling scratch code with MSVC before touching anything: `return ButtonWidget("Save");`, `Children({ButtonWidget("A"), Text("Right")})`, and `Expanded(Text("Hi"))` all compile cleanly with zero explicit wraps.

**What changed:** removed every redundant `Widget(...)`/`Tree::Widget(...)` wrap across the codebase — `src/Overlay/Toast.cpp`, `src/Widgets/PropertyGrid.cpp`, `src/Widgets/Table.cpp` (real implementation code, done by hand), `Docs/Migration_v1_to_v2.md` (26 examples rewritten, plus a new explanatory paragraph on why no example wraps), 19 widget/primitive headers' `@example` Doxygen blocks (done via a background agent, spot-checked), and `Tests/Tree/*.cpp` (8 files) + `Examples/DemoApp/main.cpp` (~90 wraps — done via a second background agent, which also ran its own build verification). **Zero changes to `Tree::Widget` itself** — this is a pure usage/documentation cleanup; the implicit-conversion capability already existed.

---

## Phase 30.3 — DockSpace Reimplementation (`DockSpaceRO` extraction, opt-in `DockSpaceWidget`) ✅ Complete

**Branch:** `feature/phase-30.3-dockspace-widget` (merged) | **Tests:** 431/432 passed under `-j4` (excluding the documented pre-existing `ThreadPool stress` hang) — the 1 failure was the same recurring `-j4` `Config` parallel-file-write race, confirmed clean in isolation. 2 expected `IconFont` skips.

**Scope decision — read before touching `DockSpace` again:** I asked the user whether Phase 30.3 should (a) keep `Application`'s automatic dockspace behavior exactly as-is and just reimplement its internals through the tree module, or (b) remove the automatic dockspace and make it a real opt-in tree node (the "real docking integration" Phase 27 `DECISIONS.md` deferred) — no response came back that session. Proceeded with **(a)**, the lower-risk, pattern-consistent choice. **This is explicitly not the aggressive reading** — if that was actually wanted, `Application`'s automatic dockspace removal, tree-root dockability, and a second `DemoApp` migration are still open work. Full reasoning in `DECISIONS.md`.

**What was built (Phase 30.3):**
- `src/Tree/RenderObjects/DockSpaceRO.hpp`/`.cpp` (new) — free functions (`BeginDockSpaceWindow`, `EndDockSpaceWindow`, `InitDefaultLayoutNodes`, `CaptureIniSettings`, `RestoreIniSettings`), mirroring the existing `RootBridge.hpp`/`.cpp` shape exactly. These are now the *only* place in the `DockSpace`/`DockSpaceWidget` code path that touches raw ImGui.
- `src/App/DockSpace.cpp` rewritten to call the new free functions instead of `<imgui.h>`/`<imgui_internal.h>` directly — **zero behavior change**: `App::DockSpace`'s public API, state, and the exact sequence of calls are unchanged. `Tests/App/DockSpace_test.cpp` passes unmodified, proving this.
- `include/ImFrame/App/DockSpaceWidget.hpp` + `src/Tree/RenderObjects/DockSpaceWidgetRO.cpp` (new) — a genuine public `Tree::PrimitiveWidget`, usable inside a `SetRoot()` tree, built on the *same* `DockSpaceRO` free functions. **Not wired into `Application`'s automatic path** — purely additive, cannot regress any existing app. No child/content slot in this phase. Uses distinct window/dockspace-id strings (`"##ImFrameDockSpaceWidget"`/`"ImFrameDockSpaceWidget"`) from `App::DockSpace`'s (`"##DockSpace"`/`"MainDockSpace"`) — a real bug was caught and fixed here before it shipped.
- `Tests/Tree/DockSpaceWidget_test.cpp` (new, 3 tests) — render-without-error via `SetRoot()`, default-layout initialization, and `MenuBar()`/`GetMenuBar()` round-trip.
- graphify updated: 5191 nodes, 6672 edges, 383 communities.

**Two real bugs found and fixed while building this (both before any test run saw them fail silently-wrong):**
1. `DockSpaceWidgetElement`'s `Paint()` originally reused `App::DockSpace`'s exact `"##DockSpace"`/`"MainDockSpace"` strings. Since `Application::RunOneFrame()` always drives the automatic `App::DockSpace` too, a `SetRoot()` tree using `DockSpaceWidget` would open the *same* ImGui window ID twice per frame — a guaranteed assertion/crash, caught during design review before writing tests. Fixed by parameterizing `BeginDockSpaceWindow(menuBar, windowName, dockspaceIdName)`.
2. The "initialises the default layout" test's first draft called `ImGui::GetID(...)` from inside `OnUi()` and checked immediately — both wrong: `ImGui::GetID()` is scoped to the *current* window's ID stack (different inside `OnUi()`'s `"##DockSpace"` context vs. the widget's own `"##ImFrameDockSpaceWidget"` window), and `OnUi()` runs *before* the `SetRoot()` tree's reconciler pass each frame, so the node doesn't exist yet on frame 1. Fixed by using 2 frames, checking on the second, and reproducing the ID hash manually via `ImHashStr(name, 0, win->ID)` after `ImGui::FindWindowByName()`.

---

## Phase 30.2 — Deprecated API Removal (21 headers deleted, DemoApp migrated) ✅ Complete

**Branch:** `feature/phase-30.2-deprecated-api-removal` (merged) | **Tests:** 427/429 passed under `-j4` (excluding the documented pre-existing `ThreadPool stress` hang) — the 2 failures were the same recurring `-j4` Config parallel-file-write race, both confirmed clean in isolation. 2 expected `IconFont` skips.

**What was built/removed (Phase 30.2):**
- **Stripped the deprecated class, kept the file** (15 files — `*Widget` replacement stayed untouched): `Widgets/{Button,Checkbox,Slider,TextInput,Combo,Table,Separator,Image,ProgressBar,ColorEdit,Radio,PropertyGrid}.hpp` (+ matching `.cpp`s where present), `Layout/Grid.hpp`, `Overlay/{Modal,ContextMenu}.hpp`.
- **Deleted whole files** (replacement lives entirely in `Tree::Primitives`/elsewhere): `Widgets/{Text,Spacer}.hpp`+`.cpp`, `Layout/{HStack,VStack}.hpp` (header-only), `Layout/{Panel,ScrollArea}.hpp`+`.cpp`.
- **Cascading dead-code cleanup found during removal** (not in the original proposal text, found by grepping for each deleted class's exclusive backing helpers before declaring a file "safe to delete"): `src/Layout/LayoutImpl.cpp` deleted whole (its 5 functions had no callers left once `HStack`/`VStack`/deprecated-`Grid` were gone); `PropertyGridScope` + `Internal::PropertyGridBeginRow/EndRow/Separator` deleted (the class's sole friend/constructor path died with `PropertyGrid`, leaving it uninstantiable — not merely uncalled); `Widgets::Renderable` concept deleted (lost its only user, `PropertyGridScope::Row<TWidget>`). `ChildScope` (`Layout/ChildScope.hpp`) was **kept** despite losing its only callers (`Panel`/`ScrollArea`) — it's a generic, already-public, already-working `BeginChild`/`EndChild` RAII wrapper with no coupling to the deleted imperative API, unlike `PropertyGridScope`. See `DECISIONS.md` for the full keep-vs-delete reasoning.
- **`include/ImFrame/ImFrame.hpp`** — removed the 6 `#include`s for the deleted whole-files.
- **20 old test files deleted** (`Tests/Widgets/*_test.cpp` ×14, `Tests/Layout/*_test.cpp` ×5, `Tests/Overlay/Modal_test.cpp`) + their `Tests/CMakeLists.txt` registrations. `Tests/Tree/Widgets_test.cpp` (the `*Widget` replacement tests) and `Tests/Widgets/LinePlot_test.cpp`/`Tests/Overlay/Toast_test.cpp` (not deprecated) untouched.
- **`Examples/DemoApp/main.cpp` migrated in full** to the declarative widget-tree API — every one of the ~53 deprecated call sites replaced (`ButtonWidget`, `CheckboxWidget`, `SliderWidget<float>`, `TextInputWidget<std::string>`, `ComboWidget<std::string>`, `ProgressBarWidget`, `ModalWidget`, `Flex` for HStack/VStack, `GridWidget`, `Box` for Panel, `VirtualList` for ScrollArea, `TableWidget`, `Tree::Primitives::Text`, `SeparatorWidget` for the old `ImGui::SeparatorText` section dividers). Introduced a local `TreePanel` helper (not a library addition) that drives one `Tree::Element`'s Mount/Update/Layout/Paint cycle inside each panel's own `ImGui::Begin()`/`End()` — `Application::SetRoot()` can't be used here since it only supports one fixed, non-dockable root window, and this demo has 6 independently dockable panels. The `#pragma warning(disable : 4996)` block and its "known accepted deprecated-API user" comment were removed — nothing deprecated remains to suppress.
- `Docs/Migration_v1_to_v2.md` finalized: future-tense "removed in Phase 30.2" flipped to past tense; "Status" section rewritten to describe what was actually deleted (including `PropertyGridScope`/`Renderable` and the `DemoApp`/`TreePanel` migration).
- graphify updated: 5138 nodes, 6606 edges, 374 communities (down from 5445/6954/409 in Phase 30.1, consistent with net code removal).

**Non-obvious issue hit and fixed this phase:** the first `DemoApp::OnUi` migration captured `[&app, &state, &widgetsPanel, &layoutsPanel, &animPanel, &tablePanel, &iconButtonsPanel]` (7 references = 56 bytes) and failed to compile — `Application::OnUi` takes `Utility::Delegate<void()>`, whose SBO buffer is `2 * sizeof(void*)` = 16 bytes on 64-bit, exactly what the *original* code's `[&app, &state]` (2 pointers) fit into. Fixed by moving the 5 `TreePanel` instances into `DemoState` as members, so `OnUi` only captures `[&app, &state]` again.

**Also fixed this session (before starting 30.2's own work):** `scripts/check_api_leaks.sh`'s `strip_comments()` only recognized full-line comments, not trailing `///<` ones — false-positived on a pre-existing, unrelated line (`BackendInfo.hpp:264`). Fixed to `filter_real_leaks()`; committed separately as `6cae4ff`.

---

## Phase 30.1 — Widget Replacement Gap Closure (Separator, Image, ProgressBar, ColorEdit, Radio, Grid, PropertyGrid) ✅ Complete

**Branch:** `feature/phase-30.1-widget-gap-closure` (merged) | **Tests:** 525/525 passed (excluding the documented pre-existing `ThreadPool stress` hang). 2 expected `IconFont` skips.

**Why this sub-phase existed:** `PHASE_30_PROPOSAL.md` describes a single monolithic "v2.0.0 Hardening & Unification" phase (deprecated-API removal, ImGui-leak audit, `WidgetTestDriver`, 5-backend conformance, reconciler perf audit, doc rewrite) but assumed Phase 29 already left every widget with a replacement. It didn't — Phase 29's own `DECISIONS.md`/`PHASE_STATUS.md` documented 7 widgets with no Tree/Component equivalent (`Separator`, `Image`, `ProgressBar`, `ColorEdit`, `Radio`, `PropertyGrid`, `Grid`). Phase 30 is being executed as ordered sub-phases (30.1–30.7); this sub-phase closed that gap — the hard prerequisite before Phase 30.2 could delete the deprecated Phase 10–14 headers.

**What was built (Phase 30.1):**
- 5 new single-ImGui-call primitives, same pattern as Phase 29's `ButtonWidget`/`CheckboxWidget` (own `Element`, same-file colocation with the deprecated class): `SeparatorWidget` (Separator.hpp/.cpp, stateless), `ImageWidget` (Image.hpp/.cpp, collapses old `Show()`/`ShowButton()` into one widget gated on whether `OnClick` is set), `ProgressBarWidget` (ProgressBar.hpp/.cpp, stateless), `ColorEditWidget` (ColorEdit.hpp/.cpp, binds via `Vec4*`), `RadioWidget` (Radio.hpp/.cpp, binds via `int*`, no `OnChange` — matches the old `Radio`'s bool-return-only contract).
- `GridWidget` (`include/ImFrame/Layout/Grid.hpp` + new `src/Tree/RenderObjects/GridRO.cpp`) — pure `Layout()`/`Paint()` math (no `ImGui::BeginTable`), mirroring the Phase 27 HStack/VStack→`Flex` precedent. `GridElement` is modeled directly on `FlexElement`, using the existing `ReconcileChildren` child-list diffing; column width = equal share of available width, row height = tallest child in that row.
- `PropertyGridWidget` (`Widgets/PropertyGrid.hpp/.cpp`) — pure composition via `Build()`, no new primitive/`Element` at all. Each row is `Flex(Horizontal){ Expanded(Text(label), factor), Expanded(rowWidget, 100-factor) }` — `SplitRatio` becomes a `Flex` factor pair (`round(ratio*100)` / `100-that`) instead of a fixed pixel width, since `Build()` runs before `Layout()` has constraint info; reuses `Flex`'s existing `DistributeFlexFactors` math for free. Declarative `Rows(std::vector<PropertyGridRow>)` API replaces the old `Begin()`/`Row()`/`Separator()` RAII scope (no declarative equivalent exists); separator rows reuse the new `SeparatorWidget`.
- `Docs/Migration_v1_to_v2.md` updated: all 7 widgets moved from "not yet reimplemented" to full per-widget migration notes; "What's still deprecated with no replacement" section replaced with a "every widget now has a replacement" status note.
- `.claude/DECISIONS.md` — new "Phase 30.1" section (5 entries) covering the primitive-vs-composition choices above.
- `Tests/Tree/Widgets_test.cpp` — 10 new `TEST_CASE`s (Separator×1, Image×2, ProgressBar×1, ColorEdit×1, Radio×1, Grid×2, PropertyGrid×2).
- `CMakeLists.txt` — added `src/Tree/RenderObjects/GridRO.cpp` under a new "Phase 30.1" source-list section.
- graphify updated: 5445 nodes, 6954 edges, 409 communities.

**Design decisions (documented in `DECISIONS.md`):**
1. `SeparatorWidget`/`ProgressBarWidget`/`ColorEditWidget`/`RadioWidget` are new primitives reusing the exact single ImGui call the deprecated class used — confirms the Phase 29 "new primitive wrapping `Show()`" pattern generalizes to every remaining Phase 10–14 widget.
2. `ImageWidget` collapses the old `Show()`/`ShowButton()` dual API into one `OnClick`-gated widget rather than two types — matches `ButtonWidget`/`CheckboxWidget`'s existing OnClick/OnChange convention.
3. `GridWidget` is pure layout math, not an `ImGui::BeginTable`-backed primitive — the deprecated class's own internal table helpers (`src/Layout/LayoutImpl.cpp`) were scheduled for deletion in 30.2, so building on them would have created a second removal problem; mirrors the `Flex` precedent instead.
4. `PropertyGridWidget`'s `SplitRatio` is expressed as `Flex` factors, not a resolved pixel width — sidesteps `Build()`-time's lack of constraint information entirely, with zero new layout code.
5. `PropertyGridWidget`'s imperative RAII scope API has no declarative equivalent — replaced with a flat `Rows()` list, matching how `TableWidget::Column()`/`Flex::Children()` already take fully-formed collections rather than builder callbacks.

---

## Phase 29 — Widget Library Reimplementation (Portal, VirtualList, Component-based widgets) ✅ Complete

**Branch:** `feature/phase-29-widget-reimplementation` (merged) | **Tests:** 514/516 passed — 2 pre-existing/environmental failures (see Phase 30.1 snapshot above for the same recurring categories). 2 expected `IconFont` skips.

**Scope note:** the proposal's own Phase Reference table undersells this phase as "VirtualList, Portal" — the actual proposal text is a full widget-library reimplementation (~15 sub-tasks). Delivered in full, with several pragmatic scope calls documented below and in `DECISIONS.md`.

**What was built (Phase 29):**
- `include/ImFrame/Tree/Portal.hpp` + `src/Tree/PortalRegistry.hpp/.cpp` + `src/Tree/RenderObjects/PortalRO.hpp/.cpp` — `Portal` primitive renders its child at root scope. `PortalElement` occupies zero space/paints nothing at its structural position; registers itself in a thread-local registry (mirrors `Context.hpp`'s `g_stateRegistrar` pattern) on every `Mount()`/`Update()`. `Reconciler::Show()` drains the registry after the main tree paints and calls `RenderDeferred()` on each — portal content is always the last thing drawn (reuses the same `"##ImFrameTreeRoot"` window via ImGui's documented multi-`Begin()` append idiom), so it renders on top without needing a second window.
- `include/ImFrame/Tree/VirtualList.hpp` + `src/Tree/RenderObjects/VirtualListRO.cpp` — fixed-row-height virtualised list. `VirtualListElement` opens its own `BeginChild` scroll region, computes the visible index range live from `ImGui::GetScrollY()`/`GetWindowHeight()` each `Paint()`, and only Mount/Update/Layout/Paints rows in that range (cached in an `unordered_map<int, unique_ptr<Element>>`, pruned as rows scroll out). A trailing zero-size `Dummy` at `itemCount * itemHeight` reserves the true (unvirtualized) scroll extent.
- **6 interactive widgets reimplemented as new primitives** (own `Element`, direct ImGui calls reusing the *exact* call sequence the old `Show()` used) rather than `Build()`-composed from GestureRegion/Box/Flex, added alongside their deprecated predecessors in the *same* header/`.cpp`: `ButtonWidget` (Button.hpp/.cpp), `CheckboxWidget` (Checkbox.hpp/.cpp), `SliderWidget<T>` (Slider.hpp — template, fully inline, calls the existing non-template `ShowSliderInt/Float/Double` helpers), `TextInputWidget<T>` (TextInput.hpp — same pattern, `ShowTextInputStr/U8`), `ComboWidget<T>` (Combo.hpp — same pattern, `ShowComboImpl`). All bind to caller-owned state via a **raw pointer** (`bool* value`, not `bool&`) — reference members would make the config type non-copy-assignable, breaking every primitive's `_config = widget.As<T>()` reconciliation pattern.
- `include/ImFrame/Widgets/Types.hpp` — added 3 shared `Internal::` helpers (`MeasureControlSize`, `BeginControlPaint`, `EndControlPaint`; defined in `src/Widgets/WidgetImpl.cpp`) so `SliderWidget<T>`/`TextInputWidget<T>`/`ComboWidget<T>`'s inline `Layout()`/`Paint()` can touch ImGui without including `<imgui.h>` in a public header.
- `TableWidget` (Table.hpp/.cpp) — genuinely composed via `Build()` on top of `VirtualList` + `Flex` + `GestureRegion`, the phase's headline capability. Scope: column definitions with a per-cell text callback + `OnRowClick`. **Not reimplemented** (documented gap, use deprecated `Table`): sort-state, per-row context menus, striping, Fixed/Stretch/Auto column width modes.
- `ModalWidget` (Modal.hpp/.cpp) and `ContextMenuWidget` (ContextMenu.hpp/.cpp) — new primitives wrapping the *same* native `ImGui::BeginPopupModal`/`BeginPopupContextItem` the old classes used. **Not Portal-based** — native ImGui popups already escape parent clipping on their own overlay layer, so Portal would only add complexity with no functional gain here (documented scope decision, see `DECISIONS.md`).
- `ToastOverlayWidget` + `ToastManager::Snapshot()` (Toast.hpp/.cpp) — the one overlay that *does* use `Portal`, since `ToastManager::Render()` draws via `ImGui::GetForegroundDrawList()` with no native escape-clipping mechanism to lean on. Deliberately **stateless** — takes a `vector<ToastSnapshot>` (from `ToastManager::Instance().Snapshot()`, reusing the existing queue/fade logic verbatim) and composes a `Portal`-wrapped `Box`/`Text` stack. Coexists with the unchanged automatic `Application::RunOneFrame()` → `Render(dt)` path; using both simultaneously double-renders.
- `ViewportWidget`, `Canvas2DWidget`, `Viewport3DWidget` (Viewport.hpp/.cpp, Canvas2D.hpp/.cpp, Viewport3D.hpp/.cpp) — thin primitive wrappers calling the existing non-copyable `Viewport`/`Canvas2D`/`Viewport3D`'s `Show()`. Bind via raw pointer for the same copy-assignability reason as the interactive widgets.
- **Deprecation sweep**: `[[deprecated]]` added to all Phase 10–14 `Show()`-builder classes across `Button`, `Checkbox`, `Slider<T>`, `TextInput<T>`, `Combo<T>`, `Table`, `ColorEdit`, `Image`, `ProgressBar`, `Radio`, `Separator`, `Spacer`, `Text`, `PropertyGrid`, `Panel`, `HStack`, `VStack`, `Grid`, `ScrollArea` (Widgets/Layout), plus `Modal`, `ConfirmModal`, `ContextMenu` (Overlay). Old classes remain fully functional.
- `Docs/Migration_v1_to_v2.md` — new file covering every deprecated class, its equivalent (or documented gap), the pointer-vs-reference binding change, and the `State<T>`-reset-on-parent-rebuild caveat.
- `Tests/Tree/Portal_test.cpp` (3 tests), `Tests/Tree/VirtualList_test.cpp` (4 tests), `Tests/Tree/Widgets_test.cpp` (14 tests covering all reimplemented widgets) — 21 new tests, all passing.
- graphify updated: 5231 nodes, 6658 edges, 407 communities.

**Design decisions made without a written proposal spec (documented in `DECISIONS.md`):**
1. Button/Checkbox/Slider/TextInput/Combo are new **primitives** (own `Element`, direct ImGui calls), not `Build()`-composed from GestureRegion+Box+Flex+Text as the proposal's prose suggested — the old `Show()` implementations are each a *single* ImGui call (`ImGui::Button`, `ImGui::Checkbox`, etc.) that already handles all hover/press/focus visual state internally via ImGui's style system. Composing from primitives would have required either reintroducing per-instance interaction state (hitting the `State<T>`-reset-on-parent-rebuild pitfall below) or reimplementing ImGui's own widget internals from scratch — reusing the existing single call is simpler, correct, and matches the Foundation's "same ImGui calls, different path" goal exactly.
2. Real architectural gap found and worked around, not fixed: `ComponentElement<T>::Update()` unconditionally overwrites `_component` from the incoming `Widget`. A `Component` with a `State<T>` member embedded in the *value itself* loses that state whenever its parent rebuilds and constructs a fresh value (not just when the component's own state changes) — parent-triggered rebuilds are indistinguishable from "this is semantically a new instance" under the current design. Worked around by keeping the new interactive widgets state-free (bind to caller-owned pointers instead). A proper fix (e.g. per-field diffing, or a "preserve across rebuild" marker on `State<T>`) is future work, not attempted this phase.
3. `Combo`/`Modal`/`ContextMenu` reimplementations don't use `Portal` — native ImGui popups (`BeginCombo`, `BeginPopupModal`, `BeginPopupContextItem`) already render on an independent overlay layer that escapes parent clipping. `Portal` is reserved for `ToastOverlayWidget`, the one case (`GetForegroundDrawList()`-based rendering) that has no native escape mechanism.
4. `Application::RunOneFrame()`'s automatic `ToastManager::Instance().Render(dt)` call was left untouched rather than removed/made-conditional — removing it would be a breaking change to Phase 13, and there was no clean way to make `ToastOverlayWidget`'s presence in a `Build()` tree "switch off" the automatic path without a new coordination flag not asked for by the proposal.
5. MSVC-specific: several deprecated classes have out-of-line fluent setters returning the class type by reference (`Table::Flags`, `ContextMenu::Item`, `PropertyGrid::SplitRatio`, `Modal::Size`, etc.) — MSVC's C4996 fires on these even in the class's *own* `.cpp` (their return type counts as a "use" of the deprecated type), unlike setters that are inline in the header (exempt). Suppressed with `#pragma warning(push/pop, disable: 4996)` scoped around just the affected `.cpp` sections. `Examples/DemoApp/main.cpp` (predates Phase 29, uses ~15 deprecated widgets extensively) got the same treatment file-wide, since it explicitly links the `imframe_warnings` `/WX` interface target and is exactly the kind of pre-existing "baseline" usage the proposal's `deprecated_usage.txt` concept describes.

---

## Phase 28 — Reactive State ✅ Complete

**Branch:** `feature/phase-28-reactive-state` | **Tests:** 496/496 passed (0 regressions; 2 expected IconFont skips; 1 pre-existing `Config Load parses string values` flake under `-j4`, confirmed passes in isolation).

**What was built (Phase 28):**
- `include/ImFrame/Tree/Context.hpp` — replaced Phase 27 empty placeholder. Thread-locals `Internal::g_stateRegistrar` (`std::function<void()>*`) and `Internal::g_currentBuildingElement` (`Tree::Element*`); RAII scopes `StateRegistrarScope`/`BuildElementScope`; `Context::Of<T>()` walks `Element::Parent()` chain looking for `GetInheritedValue(typeid(T))`, registering the caller as a dependent via `RegisterDependentDirtyCallback()` when found.
- `include/ImFrame/Tree/Element.hpp` — added `Parent()`, `GetInheritedValue(std::type_index) const noexcept` (default `nullptr`), `RegisterDependentDirtyCallback(std::function<void()>)` (default no-op), `GetDirtyFlag() const noexcept -> std::weak_ptr<std::atomic<bool>>` (default empty) — all virtual, overridden only by `InheritedElement<T>` and `ComponentElement<T>`.
- `include/ImFrame/Tree/State.hpp` — `State<T>` : `shared_ptr<Node{T data, function<void()> onDirty}>`; `Get() const` refreshes `onDirty` from `g_stateRegistrar` when set; `Set(function<void(T&)>) const` mutates then fires `onDirty`. Render-thread-only (no mutex).
- `include/ImFrame/Tree/Signal.hpp` — `Tree::Signal<T>` (distinct from `Utility::Signal<void(Args...)>`): mutex-protected `Node{mutex, T value, function<void()> onDirty, Utility::ThreadSafeSignal<void()> changeSignal}`; `Get() const` (locks, registers onDirty), `operator=(T) const` (locks, stores, fires onDirty, emits changeSignal), `OnChange(function<void()>) const -> Utility::Connection` for permanent subscribers (used by `Computed<T>`). Safe to assign from any thread.
- `include/ImFrame/Tree/Computed.hpp` — `Computed<T>(Fn, const Signal<Deps>&...)`: subscribes to each dep via `OnChange()`, eagerly evaluates once at construction; `Value() const` recomputes+caches only when `atomic<bool> dirty` was set by a dep firing; registers its own `onDirty` (mutex-protected) with `g_stateRegistrar` on each `Value()` call so a `Computed` can itself be a dependency inside a `Build()`.
- `include/ImFrame/Tree/InheritedWidget.hpp` — `InheritedWidget<T>` (`PrimitiveWidget`, wraps `T value` + `Widget child`) + `Internal::InheritedElement<T>` (template, must be public — inlines its own single-child reconcile rather than using the internal `ReconcileChild` helper). `Update()` uses `if constexpr (std::equality_comparable<T>)` to skip `NotifyDependents()` when the value is unchanged; non-comparable `T` always notifies. Forward-declares `Tree::InheritedWidget<T>` before the `Internal::` block — MSVC needs the primary template visible to parse `widget.As<Tree::InheritedWidget<T>>()`'s `<T>` as template args, not `<` as less-than.
- `include/ImFrame/Tree/Widget.hpp` — `ComponentElement<T>` gained `std::optional<Tree::Widget> _lastBuilt` (Widget has no default ctor) and `std::shared_ptr<std::atomic<bool>> _dirtyFlag`; `Update()` now does `_component = newWidget.As<T>()` then checks-and-clears the dirty flag: dirty → `Rebuild()`; clean → forwards `_lastBuilt` to `_child->Update()` so descendant `ComponentElement`s still get a chance to check their own dirty flags. `Rebuild()` wraps `_component.Build()` in `StateRegistrarScope` + `BuildElementScope`.
- `include/ImFrame/ImFrame.hpp` — Phase 27/28 Tree section merged; added includes for State/Signal/Computed/InheritedWidget.
- `Tests/Tree/State_test.cpp` — 21 tests: State Get/Set/dirty-refresh/copy-sharing; Signal Get/assign/OnChange/cross-thread (`[tsan]`); Computed eager-eval/cache/recompute/onDirty-cascade; 2 `ComponentElement` integration tests (dirty-skip, `GetDirtyFlag`).
- `Tests/Tree/InheritedWidget_test.cpp` — 8 tests: nearest-ancestor lookup, type-mismatch nullptr, inner-shadows-outer, same-value-no-notify vs changed-value-notifies, non-comparable-always-notifies, 2 full `HeadlessBackend`+`Application::SetRoot` integration tests (round-trip render, Signal-driven dirty cascade through an `InheritedWidget`).
- `Tests/CMakeLists.txt` — added `ImFrame_Tests_Tree_State` (labels `unit;tsan`) and `ImFrame_Tests_Tree_InheritedWidget` (label `unit`, needs `${CMAKE_SOURCE_DIR}` include for `HeadlessBackend.hpp`).
- graphify updated: 4740 nodes, 5899 edges, 385 communities

**Two non-obvious issues found and fixed this phase:**
1. MSVC parse failure (`C3878`/`C2872` cascades) from `InheritedElement<T>::Mount()`/`Update()` calling `widget.As<Tree::InheritedWidget<T>>()` before `Tree::InheritedWidget<T>` was declared anywhere in the translation unit — the compiler couldn't tell `<T>` was a template-argument list. Fixed with a one-line forward declaration (`template <typename T> class InheritedWidget;`) ahead of the `ImFrame::Internal::` block.
2. `Widgets::Vec2`/`Widgets::EdgeInsets` etc. are siblings of `ImFrame::Internal` and `ImFrame::Tree` under `ImFrame` — inside `ImFrame::Internal::InheritedElement<T>::Layout()`/`Paint()`, the correct unqualified spelling is `Widgets::Vec2`, not `Tree::Widgets::Vec2` (there is no `ImFrame::Tree::Widgets` namespace). Same pattern `ComponentElement<T>` (also in `ImFrame::Internal::`) already used correctly — copy that, don't over-qualify.
3. Test-file gotcha (not a library bug): Catch2 `TEST_CASE` names containing an em-dash (`—`, U+2014) round-trip through `ctest`'s cmd.exe filter argument mangled to `G��`/`G` on this Windows/MSVC toolchain, causing "No tests ran" (reported as a ctest failure) even though the test itself passes when run directly against the `.exe`. Use plain ASCII `-` in `TEST_CASE` names, not em-dashes — reserve em-dashes for comments/docs only.

**Next Action:** Commit Phase 28. Merge `feature/phase-28-reactive-state` → `develop` (`--no-ff`). Then read `PHASE_29_PROPOSAL.md`, create `feature/phase-29-virtuallist-portal` off `develop`, begin Phase 29 (VirtualList, Portal).

**Branch:** `feature/phase-28-reactive-state`

---

## Phase 27 — Widget Tree Core ✅ Complete

**Branch:** `feature/phase-27-widget-tree-core` | **Tests:** 467/467 passed (0 regressions). CI invariants verified: `grep -r imgui include/ImFrame/Tree/` and `grep -r "ImGui::" src/Tree/` (excluding `RenderObjects/`) both return nothing.

**What was built (Phase 27):**
- `include/ImFrame/Tree/Key.hpp` — explicit identity override (`Key(uint64_t)`, `HasValue()`)
- `include/ImFrame/Tree/Component.hpp` — `Component` concept (`Build() const` returning non-void)
- `include/ImFrame/Tree/Context.hpp` — empty Phase 28 placeholder, not yet threaded into `Build()`
- `include/ImFrame/Tree/Element.hpp` — abstract `Element` (Mount/Update/Unmount/Layout/Paint/CanUpdate), public `BoxConstraints`
- `include/ImFrame/Tree/Widget.hpp` — type-erased `Widget` value type; `PrimitiveWidget`/`Component` dispatch; `WidgetConcept`/`PrimitiveModel<T>`/`ComponentModel<T>`/`ComponentElement<T>` in `ImFrame::Internal::`; `Widget::As<T>()` recovery; `Widget::FlexFactor()`
- `include/ImFrame/Tree/Primitives/{Box,Text,Flex,Stack,GestureRegion,SizedBox,Expanded,Spacer}.hpp` — the 8 primitives, builder pattern, each with `CreateElement()`
- `src/Tree/ElementInternal.hpp/.cpp` — `ReconcileChildren`/`ReconcileChild`: key-based-then-structural child-list diffing
- `src/Tree/Layout.hpp/.cpp` — pure flex math: `DistributeFlexFactors`, `ComputeMainAxisOffsets`, `ComputeCrossAxisOffset`
- `src/Tree/Reconciler.hpp/.cpp` — owns root `Element`; `Show()` does Mount/Update → Layout → Paint each frame (always rebuilds — no dirty-tracking yet)
- `src/Tree/RenderObjects/{RootBridge,BoxRO,TextRO,FlexRO,StackRO,GestureRegionRO,SizedBoxRO,ExpandedRO,SpacerRO}.cpp` — concrete `Element` subclasses in `ImFrame::Internal::`; only files with raw `ImGui::` calls
- `include/ImFrame/App/Application.hpp` + `.cpp` — `Application::SetRoot<Component T>(T& root)` (templated, captures reference); owns `Internal::Reconciler`; calls `_reconciler->Show()` after `_onUi()` each frame
- `Tests/Tree/Layout_test.cpp` — 16 pure-math tests
- `Tests/Tree/Reconciler_test.cpp` — 4 tests (stable reuse, type-change replace, key-reorder identity, 50-level nesting) via custom test-local widget types, no ImGui
- `Tests/Tree/Primitives_test.cpp` — 5 tests via `HeadlessBackend` + `SetRoot`; `GestureRegion` hover/click driven via `ImGuiIO::AddMousePosEvent`/`AddMouseButtonEvent` from `OnUpdate()`
- `include/ImFrame/ImFrame.hpp`, `CMakeLists.txt`, `Tests/CMakeLists.txt` — Phase 27 includes/sources/test targets added
- `include/ImFrame/Widgets/Types.hpp` — added `EdgeInsets`, `TextAlign`
- graphify updated: 4606 nodes, 5711 edges, 369 communities

**Two non-obvious bugs found and fixed this phase (full detail in `DECISIONS.md` 2026-07-01 rows):**
1. Namespace collision: `Widget.hpp`'s type-erasure internals were originally nested as `ImFrame::Tree::Internal::`, which shadowed the project's actual `ImFrame::Internal::` (where `RenderObjects/*.cpp` define concrete Elements) for any unqualified `Internal::X` lookup from within `ImFrame::Tree::Primitives`. Fixed by moving Widget.hpp's internals to the single top-level `ImFrame::Internal::`.
2. `DockSpace::Begin()` calls `ImGui::DockSpace(id, {0,0})`, consuming the host window's entire content region before `SetRoot()`'s tree ever paints — `GestureRegion::OnClick`/`OnHover` silently never fired because the whole subtree painted outside the window's clip rect. Fixed: `Reconciler::Show()` now opens its own `RootBridge::BeginRootWindow()` window, pinned every frame to `ImGui::GetMainViewport()`'s work area (`ImGuiCond_Always`) since an unpositioned window otherwise floats at ImGui's auto-cascade position with a tiny default size.

**Next Action:** Commit Phase 27. Merge `feature/phase-27-widget-tree-core` → `develop` (`--no-ff`). Then read `PHASE_28_PROPOSAL.md`, create `feature/phase-28-reactive-state` off `develop`, begin Phase 28 (State, Signal, Computed, InheritedWidget — wires `Context::Of<T>()` into `Build()`).

**Branch:** `feature/phase-27-widget-tree-core`
3. Dawn's D3D12 backend calls `EnsureDXCLibraries()` at device-init time (not just at shader compile time), loading `dxil.dll` via `LoadLibraryEx`. The DLL ships with vcpkg's `directx-dxc` package in `debug/bin/` but is not on the system PATH, so every test executable timed out trying to create a device. Fixed: `CMakeLists.txt` POST_BUILD copies `dxil.dll` and `dxcompiler.dll` beside each WebGPU test executable. See `DECISIONS.md` 2026-06-24 rows for full detail.

**What was built (Phase 23, native path hardware-verified):**
- `Backends/DawnWebGPU/DawnWebGPUBackend.hpp/.cpp` — full `IBackend` impl: Init/Poll/BeginFrame/EndFrame/Shutdown, multi-window via `_windows` map, headless mode (`bool headless` path), `ReadPixels()` (headless only — headless render target is RGBA8 `WGPUTexture`, captured via `wgpuQueueOnSubmittedWorkDone` Future + `wgpuInstanceWaitAny`), `GetNativeGraphicsContext()` returning populated `WebGPUContext`.
- `Backends/DawnWebGPU/WebGPUDeviceSetup.hpp/.cpp` — `CreateWebGPUResources()` creating instance (with `TimedWaitAny` feature + limit), requesting D3D12 adapter via `wgpuInstanceRequestAdapter` Future, requesting device via `wgpuAdapterRequestDevice` Future, both using `wgpuInstanceWaitAny`. `OnDeviceLost` / `OnUncapturedError` callbacks.
- `Backends/DawnWebGPU/WebGPUSurface.hpp/.cpp` — `Acquire()`/`Present()`/`Reconfigure()` wrapping `wgpuSurfaceGetCurrentTexture`/`wgpuSurfacePresent`/`wgpuSurfaceConfigure`; handles `SuccessOptimal`/`SuccessSuboptimal` (both valid), `Timeout`/`Outdated`/`Lost`/`Error` (triggers resize/reconfigure).
- `Backends/DawnWebGPU/NativeSurface.cpp` — `CreateNativeSurface()` using `WGPUSurfaceSourceWindowsHWND` (native SDL HWND path) + Emscripten canvas path guarded by `#ifdef __EMSCRIPTEN__`.
- `Backends/DawnWebGPU/InputTranslation.hpp/.cpp` — `SDL_Event → InputEvent` translation, reusing Phase 22's DX12 translation table verbatim (same SDL3 event API).
- `include/ImFrame/Backends/BackendInfo.hpp` — `WebGPUContext` struct: `Device (WGPUDevice*-aliased void*)`, `Queue`, `Surface`, `TextureFormat (uint32_t)`.
- Root `CMakeLists.txt` / `vcpkg.json` — `IMF_BUILD_WEBGPU_BACKEND` option; `webgpu-backend` feature adds `dawn[d3d12]` + `sdl3` + `imgui[sdl3-binding,webgpu-binding]`; `Tests/CMakeLists.txt` POST_BUILD copies `dxil.dll`/`dxcompiler.dll` beside both GPU test executables.
- **Emscripten path (UNVERIFIED):** `EmscriptenWebGPUBackend.hpp/.cpp`, `SurfaceEmscripten.hpp/.cpp`, `InputTranslationEmscripten.hpp/.cpp` written per proposal; guarded by `#ifdef __EMSCRIPTEN__`; `CMakeLists.txt` scaffold present. No wasm32-emscripten vcpkg triplet was stood up — verify on a machine with Emscripten SDK installed.

**Last Completed:** Phase 22 — SDL3 + DirectX 12 Backend — see dedicated section below. Merged into `develop`. DX12SwapChain::Resize() bug (missing `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` flag in `ResizeBuffers()`) was the key hardware-discovered fix.

**Previously Completed:** Phase 21 — SDL3 + Metal backend — merged into `develop` 2026-06-19 **still unverified** (no macOS toolchain available). Phase 20 — SDL3+Vulkan — fully verified with 9 real hardware bugs found.

**Currently Stable:** 374/376 tests pass (1 pre-existing `ThreadPool stress` timeout, 2 expected `IconFont` skips); build clean on MSVC Debug.

> Update this block at the end of every session before committing.

---

## Phase 23 — Dawn WebGPU Backend ✅ Complete (native); ⚠️ Emscripten UNVERIFIED

**Branch:** `feature/phase-23-webgpu-backend` | **CMake:** `IMF_BUILD_WEBGPU_BACKEND` option | **Tests:** 14 native (all pass); 0 Emscripten (not run)

**What was built:**

`Backends/DawnWebGPU/DawnWebGPUBackend.hpp/.cpp` — full `IBackend` implementation. Init sequence: `InitSDL → CreateWebGPUResources (instance + adapter + device) → CreateSurface → ConfigureSurface → InitImGui → show window`. Headless path creates an RGBA8 `WGPUTexture` as the render target instead of a swap-chain surface. Multi-window via `_windows: unordered_map<WindowHandle, WindowData>`. `ReadPixels()` headless-only: submits a `wgpuCommandEncoder CopyTextureToBuffer` into a mapped `WGPUBuffer`, then calls `wgpuQueueOnSubmittedWorkDone` + `wgpuInstanceWaitAny(UINT64_MAX)` to block, then `wgpuBufferGetMappedRange` + memcpy. Row de-striding is not needed (256-byte alignment is a D3D12/Vulkan concern, not WebGPU's `wgpuBufferGetMappedRange`).

`Backends/DawnWebGPU/WebGPUDeviceSetup.hpp/.cpp` — `CreateWebGPUResources()` encapsulates the two async-request calls that are required before rendering can begin. Instance creation requires `WGPUInstanceDescriptor{requiredFeatures=[TimedWaitAny], requiredLimits={timedWaitAnyMaxCount=64}}` — NOT `nullptr` (see DECISIONS.md 2026-06-24). Both `wgpuInstanceRequestAdapter` and `wgpuAdapterRequestDevice` use the Future/WaitAny pattern. `OnDeviceLost` handles `Destroyed` (silent, expected from `wgpuDeviceRelease`) and `FailedCreation` (logs, then caller detects `device==nullptr` and returns `Error::GraphicsInitFailed`).

`Backends/DawnWebGPU/WebGPUSurface.hpp/.cpp` — wraps `WGPUSurface`; `Acquire()` calls `wgpuSurfaceGetCurrentTexture()` and handles all six status codes (`SuccessOptimal`/`SuccessSuboptimal` both proceed, others trigger a reconfigure); `Present()` calls `wgpuSurfacePresent()`; `Reconfigure()` calls `wgpuSurfaceConfigure()` with the current window size.

`Backends/DawnWebGPU/NativeSurface.cpp` — `CreateNativeSurface()` uses `WGPUSurfaceDescriptor{nextInChain=WGPUSurfaceSourceWindowsHWND{hwnd=SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)}}` on Windows. The Emscripten path uses `WGPUSurfaceSourceCanvasHTMLSelector_Emscripten` (guarded by `#ifdef __EMSCRIPTEN__`).

`Backends/DawnWebGPU/InputTranslation.hpp/.cpp` — `SDL_Event → InputEvent` translation, identical logic to `Backends/SDL3DX12/InputTranslation.cpp` (same SDL3 API surface). Reused rather than shared to keep backends independent.

**Three runtime-discovered bugs fixed (not compile errors)** — see DECISIONS.md 2026-06-24 rows:
1. `wgpuCreateInstance(nullptr)` → all `wgpuInstanceWaitAny(timeout>0)` calls fail silently. Both `requiredFeatures=[TimedWaitAny]` AND `requiredLimits->timedWaitAnyMaxCount=64` are required.
2. `IMF_ASSERT(false)` in `OnDeviceLost` → MSVC abort dialog on device-creation failure, freezing test processes for 64 s (ctest timeout hits). Removed; only log on truly unexpected loss.
3. `dxil.dll` missing beside test executables → Dawn's D3D12 backend fails at device-init time (`EnsureDXCLibraries`). Fixed via POST_BUILD copy in `Tests/CMakeLists.txt`.

**Emscripten path (UNVERIFIED):** `EmscriptenWebGPUBackend.hpp/.cpp`, `SurfaceEmscripten.hpp/.cpp`, `InputTranslationEmscripten.hpp/.cpp` written per the proposal. No wasm32-emscripten vcpkg triplet was stood up. Before marking Phase 23 fully ✅: compile with an Emscripten SDK, fix whatever fails, run the browser test.

Verified (native path): Debug build, 14/14 tests pass across `WebGPUInput_test.cpp`, `WebGPUBackend_test.cpp`, `WebGPUConformance_test.cpp`. Full 376-test regression clean (374 pass, 1 pre-existing ThreadPool stress timeout, 2 expected IconFont skips).

---

## Phase 22 — SDL3 + DirectX 12 Backend ✅ Complete

**Branch:** `feature/phase-22-dx12-backend` (created from `develop`) | **CMake:** `IMF_BUILD_DX12_BACKEND` option added | **Tests:** 17 total (16 pass; `DX12Leak_test.cpp` is debug-only, 15 run in Debug + 11 in Release since `DX12Leak_test` is excluded from Release)

**What was built:**

`Backends/SDL3DX12/SDL3DX12Backend.hpp/.cpp` — full `IBackend` implementation. Init sequence: `ApplyDpiAwareness → InitSDL → create SDL window (hidden) → EnableDebugLayerIfDebug → CreateFactoryAndAdapter → CreateDeviceAndQueues → CreateDescriptorHeaps → CreateSwapChainFor(primary) → AllocateFrameResources(primary) → InitImGui → show window`. `ID3D12Device4` minimum (needed for `CreateCommandList1()`, which returns command lists already closed). GPU-preference adapter selection via `IDXGIFactory6::EnumAdapterByGpuPreference` when available, falling back to `EnumAdapters1` with a DedicatedVideoMemory-based discrete/integrated heuristic (DXGI has no direct equivalent to Vulkan's `VkPhysicalDeviceType`). Debug builds register an `ID3D12InfoQueue1` message callback routing WARNING+ to ImFrame's `Logger`.

**`DescriptorAllocator`** — a simple free-list slot allocator over one `ID3D12DescriptorHeap` (RTV or shader-visible CBV_SRV_UAV). Two shared instances, sized once at Init (`MAX_WINDOWS=16 × (framesInFlight+1)` for RTV, 1024 for SRV) — not per-window heaps.

**`ImGui_ImplDX12_Init()` divergence from the proposal's "Expected pattern"** — verified against the actual installed imgui 1.92 header before implementing, per the proposal's own DECISIONS.md instruction. The legacy fixed-font-SRV-handle API is obsolete; the current API requires `SrvDescriptorAllocFn`/`SrvDescriptorFreeFn` callbacks that imgui itself calls to allocate/free descriptor slots (font and user textures alike). `DescriptorAllocator` was designed generically enough to serve directly as the callback target via `UserData`.

**`DX12SwapChain`** — `FLIP_DISCARD` only, RTV descriptors allocated from the shared RTV `DescriptorAllocator`. `SetMaximumFrameLatency()` + `GetFrameLatencyWaitableObject()` is the primary frame-pacing mechanism in `BeginFrame()`, with a per-slot `FrameFence::Wait()` as a backstop for the edge case where the waitable object fires before the GPU has actually finished. HDR checked via `IDXGIOutput6::GetDesc1()` against the monitor containing the window (found via `MonitorFromWindow`), not via the swap chain itself (avoids a chicken-and-egg dependency on a swap chain that doesn't exist yet).

**Headless mode** — a committed `ID3D12Resource` (`DXGI_FORMAT_R8G8B8A8_UNORM`, deliberately *not* matching the windowed swap chain's `B8G8R8A8_UNORM`) as the render target instead of a swap chain back buffer. Choosing RGBA8 directly (rather than reusing the swap chain's BGRA8) means `ReadPixels()` needs zero channel-swizzle code — simpler than the Vulkan/Metal backends' BGRA8→RGBA8 swizzle, since this resource isn't constrained by DXGI's swap-chain format restrictions.

**`ReadPixels()`** — headless-only, mirroring the Phase 20/21 windowed-capture scoping decision. The copy happens on the same direct-queue command list as the render (not the copy queue the proposal originally described — see DECISIONS.md), using `CopyTextureRegion` into a `D3D12_HEAP_TYPE_READBACK` buffer, then de-strides the 256-byte-aligned (`D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`) row pitch down to a tightly-packed buffer before returning, per the proposal's row de-striding invariant — verified by a dedicated test using a 100px-wide (intentionally non-256-aligned) render target.

**One real bug found via hardware testing** (not a compile error): `DX12SwapChain::Resize()`'s call to `ResizeBuffers()` omitted `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` from its flags argument. DXGI rejects this outright (confirmed via the DXGI debug layer — note: queried through a *separate* `IDXGIInfoQueue`, not the `ID3D12InfoQueue` already hooked for D3D12-level messages, since DXGI and D3D12 have independent debug-layer message streams). This fired on literally the first frame of every windowed test: `SDL_ShowWindow()` (called at the end of `Init()`) triggers an `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` event that the backend correctly treats as a real resize, so every windowed session immediately attempted a same-size `ResizeBuffers()` call before the first `BeginFrame()`. The failed resize silently left the swap chain's back-buffer vector empty (already cleared in preparation for recreation), crashing the next frame's `CurrentBuffer()` call. See DECISIONS.md for the full diagnostic process.

Verified clean: Debug build with the D3D12 debug layer and GPU-based validation enabled, plus a dedicated `DX12Leak_test.cpp` that registers its own synchronous `ID3D12InfoQueue1` callback — zero ERROR/CORRUPTION-severity messages across a full init-render-shutdown cycle. All 15 Debug tests and 11 Release tests (the 4 fewer being the excluded debug-only leak test) pass across multiple randomized-order runs. Full existing 378-test suite re-run afterward with zero new regressions.

---

## Phase 21 — SDL3 + Metal Backend ⚠️ UNTESTED, merged into `develop` anyway — do not mark Complete

**Branch:** `feature/phase-21-metal-backend`, merged `--no-ff` into `develop` 2026-06-19 on explicit user instruction, ahead of the macOS verification gate | **CMake:** `IMF_BUILD_METAL_BACKEND` option added | **Tests:** 0 written, 0 run — see warning below

> **This phase has never been compiled.** Written entirely on a Windows machine with no Metal/Objective-C++ toolchain, from the proposal text and vcpkg port inspection alone. Tests were explicitly skipped this session per user instruction ("CI might catch any errors if we're lucky"). Every item below is a description of intent, not a verified fact. See `DECISIONS.md` 2026-06-18 rows for full rationale on every scoping call.

**What was written (unverified):**
- `Backends/SDL3Metal/SDL3MetalBackend.hpp/.mm` — `IBackend` impl: Init/Poll/BeginFrame/EndFrame/Shutdown, multi-window, `ReadPixels()` (headless path only — see scoping note below). `bool headless` constructor parameter (not wired through `Application::CreateHeadless()`).
- `Backends/SDL3Metal/MetalLayer.hpp/.mm` — `CAMetalLayer` configuration (windowed) or offscreen `MTLTexture` allocation (headless); HDR capability check via `NSScreen.maximumPotentialExtendedDynamicRangeColorComponentValue` with silent SDR fallback; `Acquire()`/`Present()`/`Resize()` — resize only updates `drawableSize`, no recreation.
- `Backends/SDL3Metal/FrameResources.hpp` — per-slot reserved `MTLBuffer` (unused by ImGui-only rendering, reserved for Phase 24). The real CPU/GPU sync primitive is a single `dispatch_semaphore_t` per window (not per-slot).
- `Backends/SDL3Metal/CMakeLists.txt` — `enable_language(OBJCXX)`, explicit `-fobjc-arc` per `.mm` file (confirmed via vcpkg's own imgui port that OBJCXX does NOT imply ARC by default), reuses Phase 20's `InputTranslation.cpp` directly via `target_sources()` rather than copying it.
- `Backends/iOSMetal/` — scaffold only: header declares the class, `.mm` is an intentional `#error` stub, `CMakeLists.txt` self-guards on `CMAKE_SYSTEM_NAME STREQUAL "iOS"`.
- `include/ImFrame/Backends/BackendInfo.hpp` — `MetalContext` gained `PixelFormat`/`FramesInFlight`; `Device`/`CommandQueue` stay `void*` (typed accessor deliberately deferred to Phase 24).
- Root `CMakeLists.txt` / `vcpkg.json` — `IMF_BUILD_METAL_BACKEND` option (read before `project()`, same pattern as Phase 20's Vulkan flag); explicit `message(WARNING ...)` if set on a non-Apple host (no silent no-op); `metal-backend` vcpkg feature depends on `sdl3` + `imgui[sdl3-binding,metal-binding]` gated `"platform": "osx"`.
- **Scoped out, not implemented:** windowed (non-headless) `ReadPixels()` — the proposal never specifies this path beyond one ambiguous sentence; only the headless offscreen-texture path is implemented, matching what the proposal's own Testing section actually exercises.
- **What IS actually verified** (the only parts checkable on a Windows machine): `scripts/check_api_leaks.sh` passes against the `BackendInfo.hpp` changes; a configure-only CMake run confirms the new options don't break the default Windows/GLFW build with both new options OFF.

**Before this can be marked ✅ Complete:** compile `ImFrame_SDL3Metal` on real macOS (or CI), fix whatever doesn't compile (expect several rounds — Phase 20's Vulkan backend needed 9 real-hardware bug fixes that were invisible at write-time), write and pass `Tests/Backends/MetalBackend_test.cpp` / `MetalConformance_test.cpp` / `MetalHDR_test.cpp`.

---

## Phase 20 — SDL3 + Vulkan 1.3 Backend ✅ Complete

**Branch:** `feature/phase-20-vulkan` | **CMake:** v2.0.0 | **Tests:** 360 total (360 pass; 2 expected IconFont skips)

**What was built:**

`Backends/SDL3Vulkan/SDL3VulkanBackend.hpp/.cpp` — full `IBackend` implementation. Init sequence: `InitSDL → CreateInstance → CreateDebugMessenger → create SDL window + Vulkan surface → SelectPhysicalDevice → CreateDevice → InitVMA → CreateDescriptorPool → CreateViewportCommandPool → CreateSwapChainFor(primary) → AllocateFrameResources(primary) → InitImGui → show window`. Dynamic rendering only (no `VkRenderPass`/`VkFramebuffer`); all GPU memory through VMA; physical device scored by type (discrete=1000 > integrated=100 > CPU=1) with VRAM tie-break; debug object naming via dynamically-loaded `vkSetDebugUtilsObjectNameEXT`; multi-window via `CreateWindow`/`DestroyWindow` overrides sharing the primary device/queues.

`Backends/SDL3Vulkan/SwapChain.hpp/.cpp` — format/present-mode/extent selection; `Create()`/`Destroy()`; uses `oldSwapchain` hint on recreation to avoid `vkDeviceWaitIdle`; `supportsReadback` flag tracks whether `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` was available and requested for the swap chain images.

`Backends/SDL3Vulkan/FrameResources.hpp` — per-frame semaphores/fence/command pool/buffer, indexed by `frameIndex % framesInFlight`.

`Backends/SDL3Vulkan/InputTranslation.hpp/.cpp` — pure `SDL_Event → InputEvent` translation functions (key, mouse, gamepad, touch, window, text); SDL's 1-based mouse button indices remapped to ImFrame's 0=left/1=right/2=middle convention; gamepad axes have a 0.1f dead zone.

`include/ImFrame/Backends/BackendInfo.hpp` — `WindowConfig` gained `HDROutput` (bool) and `FramesInFlight` (int, default 2); `VulkanContext` restructured to `{Instance, PhysicalDevice, Device, GraphicsQueueFamily, GraphicsQueue, ViewportCommandPool, DescriptorPool, SwapchainImageFormat}` (no `RenderPass` — dynamic-rendering invariant).

**`SDL3VulkanBackend::ReadPixels()`** — `[[nodiscard]] std::vector<std::byte> ReadPixels() const`, matching `HeadlessBackend::ReadPixels()`'s contract exactly (RGBA8, tightly packed, top-to-bottom, callable between `EndFrame()` and the next `BeginFrame()`, empty on failure). Implementation copies the just-rendered primary-window image into a persistent VMA-mapped `HOST_VISIBLE` staging buffer (recreated on resize) *inside* `EndFrame()`, before the COLOR_ATTACHMENT_OPTIMAL→PRESENT_SRC_KHR barrier — touching a presentable image after `vkQueuePresentKHR` is a Vulkan spec violation (caught by validation layers on the first attempt at copying post-present). `ReadPixels()` itself just `vkQueueWaitIdle`s and memcpy's the mapped buffer, swizzling BGRA→RGBA when needed. HDR (16-bit float) swap chains return empty.

`Tests/Backends/VulkanInput_test.cpp` — 25 tests, `unit` label, no GPU required, pure `InputTranslation` function tests.

`Tests/Backends/VulkanBackend_test.cpp` — 9 tests, `vulkan` label, `TIMEOUT 60`, requires a real Vulkan-capable GPU: Init success, double-Init rejection (`AlreadyInitialised`), idempotent `Shutdown()`, `GetNativeGraphicsContext()` returns a fully-populated `VulkanContext`, 10-frame `BeginFrame`/`EndFrame` loop, 10x resize-driven swap chain recreation, secondary window create/use/destroy leaves the primary window functional, `ReadPixels()` dimension/content/multi-frame checks.

`Tests/Backends/VulkanConformance_test.cpp` — 2 tests, `vulkan` label. Scoped down from the proposal's literal Vulkan-vs-HeadlessBackend pixel comparison, which is not meaningful yet — `HeadlessBackend::ReadPixels()` always returns a zero-filled buffer (Phase 19 built it as a pure API-surface stub; real headless rendering is Phase 36's `SoftwareRenderer`). Instead verifies Vulkan's own render→capture→swizzle pipeline: a red/blue split-screen scene preserves channel identity (catches a swapped R/B swizzle), and an empty scene reads back uniform and fully opaque.

Verified with full Vulkan validation layers enabled (Debug config) across all three Vulkan test executables — zero warnings/errors across multiple randomized-order runs.

**9 real bugs found via hardware testing (not compile errors)** — see `DECISIONS.md` 2026-06-17 rows for the full list. Highlights: a dangling-pointer use-after-free in `vkCreateInstance` setup that segfaulted on first run; a CMake `PRIVATE`→`PUBLIC` link-visibility fix (the header stores `VkInstance`/`VkDevice`/`VmaAllocator`/`SDL_Window*` by value, unlike the GLFW backend's forward-declared `GLFWwindow*`); vcpkg's `sdl3` port requires an explicit `vulkan` feature; wrong queue-family selection accepted a compute-only family as graphics-capable; descriptor pool was missing types imgui's dynamic-texture system needs; swap chain recreation destroyed the old swapchain while still in use by the queue; and the first `ReadPixels()` attempt tried to copy a swapchain image after it had already been presented — a Vulkan spec violation invisible in Release builds and only caught by Debug-build validation layers.

**Still pending (not a Phase 20 gap — blocked on future phases):** the proposal's original Vulkan-vs-HeadlessBackend pixel comparison needs Phase 36's `SoftwareRenderer` to give `HeadlessBackend` real rendering to compare against. Revisit `VulkanConformance_test.cpp` when Phase 36 lands.

**Key decisions:** see `DECISIONS.md` 2026-06-17 rows.

---

## Phase 19 — Backend Abstraction Hardening ✅ Complete

**Branch:** `feature/phase-18-polish-hardening-ci` | **CMake:** v1.9.0 | **Tests:** 324 total (320 pass; 4 pre-existing)

**What was built:**

`include/ImFrame/Backends/InputEvent.hpp` — `WindowHandle = uint32_t`, `PrimaryWindow = 0`, `ModFlags` bitmask enum with `HasFlag()`, `KeyAction` enum, `TouchPhase` enum, 11 event structs (KeyEvent, MouseButtonEvent, MouseMoveEvent, MouseScrollEvent, TextInputEvent, GamepadEvent, TouchEvent, StylusEvent, WindowResizeEvent, WindowFocusEvent, WindowCloseRequestEvent), `InputEvent` std::variant.

`include/ImFrame/Backends/FrameInfo.hpp` — `FrameInfo { ShouldClose, DeltaTime, DisplayRefreshInterval, ActiveWindows }`.

`include/ImFrame/Backends/BackendInfo.hpp` — `VSyncMode` enum (Off/On/Adaptive); `WindowExtent`; `NativeGraphicsContext` variant (OpenGLContext/VulkanContext/MetalContext/DX12Context/WebGPUContext/HeadlessContext with HeadlessPixelFormat); `IBackend` interface updated: pure-virtual `Poll()->FrameInfo`, `BeginFrame(WindowHandle=PrimaryWindow)`, `EndFrame(WindowHandle=PrimaryWindow)`, `NativeHandle`, `CancelClose`; non-pure with defaults: `WindowDpiScale`, `WindowSize`, `WindowIsMinimized`, `WindowIsFocused`, `DrainInputEvents`, `GetNativeGraphicsContext`, `CreateWindow`, `DestroyWindow`.

`Backends/Headless/HeadlessBackend.hpp/.cpp` — complete implementation; compiled into ImFrame core (not a separate target); RGBA8 pixel buffer zero-initialised; `CreateWindow()` returns `++_nextHandle` (simulates multi-window for tests); `InjectInputEvent`/`DrainInputEvents` round-trip.

`Backends/GLFWOpenGL3/GLFWOpenGL3Backend.hpp/.cpp` — major update: `ImGui_ImplGlfw_InitForOpenGL(_window, false)` + 10 manual GLFW callbacks each chaining to `ImGui_ImplGlfw_*Callback()`; `PollGamepads()` with dead zone 0.1f, epsilon 0.001f, emit-on-change; `_secondaryWindows` unordered_map; `_nextHandle` counter starting at 1; delta time via `glfwGetTime()`; refresh interval via `glfwGetVideoMode()`.

`src/App/Application.hpp/.cpp` — `RunOneFrame()` calls `Poll()->FrameInfo`; drains input events; `DisplayRefreshInterval()`; `CreateSecondaryWindow()`; `DestroySecondaryWindow()`; `InjectInputEvent()` (dynamic_cast to HeadlessBackend); `ReadHeadlessPixels()` (dynamic_cast).

`Tests/Backends/InputEvent_test.cpp` (14 tests / 39 assertions), `Tests/Backends/HeadlessBackend_test.cpp` (11 tests / 307225 assertions — pixel buffer loop), `Tests/Backends/MultiWindow_test.cpp` (9 tests / 37 assertions).

**Key decisions:** see `DECISIONS.md` 2026-06-15 rows.

---

## Phase 18 — Polish, Hardening & CI ✅ Complete

**Branch:** `feature/phase-17-dev-tools` | **CMake:** v1.8.0 | **Tests:** 288 total (all pass; no new tests added this phase)

**What was built (modifications only — no new files except scripts and docs):**

`noexcept` audit — all destructors in `include/ImFrame/` explicitly marked `noexcept` (FileWatcher, ThreadPool, BackgroundWorker, Signal::Connection, EventBus::SubscriptionToken, EventBus, TimerHandle, Logger, FileSink, Config, Delegate, ChildScope, Panel inline setters, ScrollArea inline setters, PopupScope, PropertyGridScope, WindowScope, Application, ThemeHotReload); matching `.cpp` out-of-line definitions updated to match.

`[[nodiscard]]` audit — `Application::RunOneFrame()`, `Application::Run()`, `FileWatcher::Poll()`, and all RAII scope types (`ChildScope`, `PopupScope`, `WindowScope`, `PropertyGridScope`) marked `[[nodiscard]]`; two missed discard sites in `Config.cpp` (watcher.Poll call) and `ThemeHotReload.cpp` (watcher.Poll call) fixed with `(void)` cast.

`scripts/check_api_leaks.sh` — full implementation replacing stub; 3 checks: (1) no `#include.*imgui` in `include/ImFrame/`, (2) no `ImFrame::Internal::` in non-comment code, (3) no ImGui type names (`ImVec*`, `ImGui*`, `ImFont*`, `ImTextureID`) in non-comment code; `IconFont.hpp` explicitly exempted (intentional bridge); strip_comments helper filters Doxygen block and `//` lines from grep output.

`.github/workflows/ci.yml` — added 3 new jobs: `sanitize` (ASan/TSan/UBSan as matrix on ubuntu/clang-17; Debug build; xvfb+Mesa); `coverage` (gcov-13; `lcov` filtered; 80% threshold enforced via `awk`; artifact upload); `docs` (Doxygen build every PR/push; deploy to GitHub Pages on push to `main` via `peaceiris/actions-gh-pages@v3`).

`docs/Doxyfile` — created (`include/ImFrame/` → `docs/html`; graphviz SVG; EXTRACT_ALL=NO; `IMF_DEV_TOOLS=1` predefined; `WARN_IF_UNDOCUMENTED=YES`).

`CMakeLists.txt` — VERSION bumped 1.7.0 → 1.8.0.

`Examples/DemoApp/main.cpp` — Phase 1 smoke test replaced with full feature showcase: runtime theme switching (Dracula/Nord/CatppuccinMocha/Light via `ImGui::BeginMainMenuBar`); all core widgets (Button, TextInput, Slider, Checkbox, Combo, ProgressBar); layout containers (HStack/VStack/Grid/Panel/ScrollArea); `AnimatedValue<float>` demo; Toast (Info/Success/Warning/Error); `Modal` confirmation dialog; virtualised `Table` (10 000 rows, per-column renderer mode); `LinePlot` (sin/cos) + `BarPlot`; persistent DockSpace layout save/load/reset.

`Examples/DemoApp/CMakeLists.txt` — added `imgui::imgui` link (DemoApp calls ImGui directly for menus and table cell renderers; imgui is PRIVATE in ImFrame target so not transitively available).

**Key decisions:** see `DECISIONS.md` 2026-06-14 rows.

---

## Phase 17 — Developer Tooling & Hot Reload ✅ Complete

**Branch:** `feature/phase-17-dev-tools` | **CMake:** v1.7.0 | **Tests:** 288 total (10 new DevTools tests; all pass)

**What was built:**
All code guarded by `#if defined(IMF_DEV_TOOLS)` (ON in Debug, OFF in Release via CMake option).

`LogViewer.hpp/cpp` — reads from a `std::shared_ptr<UiSink>`; renders an `ImGui::BeginTable` with 4 columns (Time/Level/Message/Location); level filter `_levelFilter[6]` array; text search `_searchBuf[256]` with substring match on message and filename; auto-scroll toggle; `Clear` button; `VisibleEntryCount()` for headless-friendly test assertions. Registered with `WindowManager` via `Register(wm)`.

`PerfOverlay.hpp/cpp` — `PerfOverlayConfig` (corner/padding/alpha); 60-frame rolling `_frameTimes` ring buffer; `AnimatedValue<float>` `_smoothFps`/`_smoothFrameMs` (speed=5); reads `ImGuiIO::MetricsRenderVertices/Indices` from previous frame; viewport null-guard (linter addition); `ImGuiWindowFlags_NoDocking | NoBringToFrontOnFocus | NoMove` etc.

`ThemeHotReload.hpp/cpp` — `FileWatcher` on configurable directory; file-local TOML `[r,g,b,a]` parser using `std::from_chars` (locale-safe); `ApplyColorMap` maps 19 string keys to `ColorRole` enum values via `ThemeBuilder::SetColor`; `TakePending()` consumes `optional<Theme>` and clears; destructor calls `Unwatch()`.

`Application.hpp/cpp` wired: `_uiSink` created and added to Logger in `Run()`; `optional<LogViewer> _logViewer` emplaced and registered; `ThemeHotReload` seeded with active/Dracula base, watches `Assets/Themes/`; `optional<Theme> _hotTheme` stores reloaded theme (avoids dangling `_pendingTheme` pointer); `RunOneFrame()` polls hot-reload, applies theme, renders LogViewer + PerfOverlay inside dockspace.

`Tests/DevTools/LogViewer_test.cpp` — 5 tests: visible-count, level-filter, text-search, Render no-crash, Register wires visibility. Uses a `RenderScope` helper (not full Application) to avoid competing internal LogViewer.

`Tests/DevTools/ThemeHotReload_test.cpp` — 5 tests: nullopt-before-change, TOML accent override, non-.toml ignored, unknown keys skipped, TakePending clears state. Uses `TempDir` RAII + `WaitForPending` polling loop.

**Key decisions:** see `DECISIONS.md` 2026-06-13 rows (_hotTheme lifetime; TOML-only float-array parser; PUBLIC IMF_DEV_TOOLS propagation).

---

## Phase 16 — DockSpace & Panel Manager ✅ Complete

**Branch:** `feature/phase-16-dockspace-layout-manager` | **CMake:** v1.6.0 | **Tests:** 278 total (5 new DockSpace tests; all pass)

**What was built:**
`DockSpace.hpp/cpp` — 4 new public methods: `SetConfig(Utility::Config*)` (wires persistence; parses `"Layout._index"` + `"Layout.Default"` from Config at startup); `SaveLayout(name)` (captures ini via `ImGui::SaveIniSettingsToMemory()`, stores in Config under `"Layout.<name>"`, updates `_layoutNames` + `"Layout._index"`); `LoadLayout(name)` (reads Config, calls `ImGui::LoadIniSettingsFromMemory()`); `ResetLayout()` (reloads `_defaultIni` captured at first-run); `ListLayouts()` (returns `_layoutNames` vector in insertion order). `InitDefaultLayout()` now also saves the captured ini to `_defaultIni` + Config `"Layout.Default"`. `Begin()` on first call loads Config's default ini before `DockSpace()` so ImGui can match saved node IDs.
`Window.hpp/cpp` — `WindowManager::RenderLayoutMenu(DockSpace&)` renders a `"Layout"` submenu: one `MenuItem` per saved layout (calls `LoadLayout`), inline `InputText + Save` button (calls `SaveLayout`), `"Reset to Default"` item (calls `ResetLayout`). Private `_saveLayoutBuf[128]` persists across frames.
`Application.hpp/cpp` — `Utility::Config _layoutConfig` value member (in-memory by default); `GetDockSpace() noexcept -> DockSpace&` accessor; `_dockSpace.SetConfig(&_layoutConfig)` wired in `Run()` before the render loop.

**Key decisions:** See `DECISIONS.md` 2026-06-12 rows.

---

## Phase 15 — Plot Integration ✅ Complete

**Branch:** `feature/phase-15-plot-integration` | **CMake:** v1.5.0 | **Tests:** 273 total (5 new LinePlot tests; all pass)

**What was built:**
`PlotContext.hpp/cpp` — `PlotContext` class (non-copyable/non-moveable; `Init()`/`Shutdown()`/`ApplyTheme()`); `PlotColormap` enum (0–9 matching `ImPlotColormap_`); `ImPlotContext*` forward-declared in public header so `<implot.h>` stays entirely in `PlotContext.cpp`.
`LinePlot.hpp/cpp` — fluent builder: `Size/XLabel/YLabel/Series(name, xs, ys)`; `Show()` renders via `ImPlot::BeginPlot`/`PlotLine`; series cleared after each `Show()` call; supports multiple series.
`BarPlot.hpp/cpp` — fluent builder: `Size/Bars(name, values, barWidth)/Horizontal(bool)`; `Show()` uses `ImPlotSpec::Flags = ImPlotBarsFlags_Horizontal` for horizontal bars.
`ScatterPlot.hpp/cpp` — fluent builder: `Size/Points(name, xs, ys)/MarkerSize(float)`; `Show()` uses `ImPlotSpec::MarkerSize`; supports multiple point series.
`HeatMap.hpp/cpp` — fluent builder: `Size/Data(span, rows, cols)/ColorMap/ScaleMin/ScaleMax`; `Show()` calls `PushColormap`/`PopColormap` around `BeginPlot` (always balanced); uses `ImPlotFlags_NoLegend`.
All four `Show()` implementations and `PlotContext` methods defined in single TU `src/Widgets/PlotContext.cpp`; file-local `PlotScope` RAII (conditional `EndPlot`, moveable, mirrors `PopupScope` pattern).
`Application.hpp/cpp` wired: `PlotContext _plotContext` value member; `Init()` after backend init; `Shutdown()` before backend shutdown; `ApplyTheme` called when `_pendingTheme` is set.
`include/ImFrame/ImFrame.hpp` — added 4 new plot widget includes.

**Key fixes during implementation:**
- implot v1.0 removed `ImPlotCol_Line` (per-item color now `ImPlotSpec::LineColor`) and `ImPlotStyleVar_MarkerSize` (now `ImPlotSpec::MarkerSize`); `PlotBars` flags via `ImPlotSpec::Flags` not raw int. Discovered by inspecting the installed `implot.h`.
- MSVC rejects `if (PlotScope scope(expr))` if-with-declaration using `()` init; separated into declaration + `if (scope)`.
- `Delegate` 16-byte SBO: test lambdas with 3+ captured refs overflow; fixed by making test data `static const` inside lambdas.
- CTest `TIMEOUT 30` added to LinePlot test registration as loop breaker.

**Key decisions:** `DECISIONS.md` 2026-06-10 rows.

---

## Phase 14 — Table + PropertyGrid ✅ Complete

**Branch:** `feature/phase-14-table-propertygrid` | **CMake:** v1.4.0 | **Tests:** 268 total (267 non-flaky pass; 10 Table + 9 PropertyGrid new test cases across 2 executables)

**What was built:**
`Table.hpp/cpp` — `ColumnWidthMode` enum (Fixed/Stretch/Auto); `SortDirection` enum + `ColumnSortSpec` + `SortState` structs (public wrapper types — no ImGuiTableSortSpecs* in public API); `ColumnDef` fluent builder (Label, Width, WidthMode, SortEnabled, Renderer); `Table` class (id, columnCount, Flags/OuterSize/Scrollable/Borders/Striped builder, Column, ContextMenu, Render, GetSortState); dual rendering mode: if ANY column has `ColumnDef::Renderer` → per-column mode (Table manages `TableNextColumn()` internally); otherwise → row-renderer mode (user calls `TableNextColumn()`); `ImGuiListClipper` used in all cases; sort spec readback via `TableGetSortSpecs()` → stored in `_sortState`; right-click context menu: mouse-Y vs row-top hit-test during clipper loop, `_pendingContextRow` triggers `OpenPopup`, `_activeContextRow` persists across popup frames.
`PropertyGrid.hpp/cpp` — `PropertyGridScope` RAII (moveable, destructor calls `EndTable()` when `_open`; mirrors ChildScope/PopupScope pattern); `Row<TWidget>()` template calls `Internal::PropertyGridBeginRow/EndRow` helpers (declared in header, defined in .cpp) to keep `<imgui.h>` out of public header; `Separator(groupName)` tints row with `ImGuiCol_TableHeaderBg` and optionally renders group label; `PropertyGrid::Begin()` opens 2-column `ImGuiTableFlags_SizingStretchProp` table with `_splitRatio`/`1-_splitRatio` weights (default 35/65).

**Key decisions:** `std::function<void(int)>` for all Table callbacks (row/column renderers and ContextMenu) — captures typically exceed 16-byte Delegate SBO; `SortState` wrapper instead of `ImGuiTableSortSpecs*` — no ImGui types in public headers; per-column vs row-renderer dual mode selected by presence of `ColumnDef::Renderer`; `PropertyGridScope` mirrors `ChildScope` pattern with `EndTable()` in destructor guarded by `_open`.

---

## Phase 13 — Overlay System ✅ Complete

**Branch:** `feature/phase-13-overlays` | **CMake:** v1.3.0 | **Tests:** 247 total (240 non-flaky pass; 6 Toast + 8 Modal new test cases across 2 executables)

**What was built:**
`Toast.hpp/cpp` — `ToastType` enum (Info/Success/Warning/Error); `ToastConfig` struct; `ToastManager` singleton with Pimpl (`active: vector<Toast>`, `queue: deque<Toast>`); `Toast` internal struct with `AnimatedValue<float>` opacity (speed=10); fade-in→hold→fade-out state machine; `erase_if` removes expired; `Promote()` populates from queue; `Render(dt)` draws colored rounded rects via `ImGui::GetForegroundDrawList(viewport)` — no ImGui window needed; free functions `ToastInfo/Success/Warning/Error`.
`Modal.hpp/cpp` — `PopupScope` RAII (conditional `EndPopupModal` — only when `_open=true`, contrast with `ChildScope`'s unconditional `EndChild`); `Modal` with `_pendingOpen` deferred-open pattern (same-frame `OpenPopup + BeginPopupModal`); fluent `Size/NoClose`; `ConfirmModal` self-contained two-button dialog (OK/Cancel), `OnResult(Delegate<void(bool)>)` fires once.
`ContextMenu.hpp/cpp` — persistent fluent builder; `Item(label, action, enabled)` + `Separator()`; `Show()` (context item) + `ShowWindow()` (context window); `CloseCurrentPopup()` on item click.
`Application.cpp` wired: `Overlay::ToastManager::Instance().Render(_deltaTime)` called after `_onUi()` inside `RunOneFrame()`.
No new public includes in `ImFrame.hpp` — overlay headers were already listed as stubs.

**Key decisions:** ForegroundDrawList avoids window management overhead for toasts; PopupScope EndPopupModal is conditional (ImGui contract differs from EndChild); ContextMenu items use std::function (any capture size, no SBO concern for menu actions).

---

## Phase 12 — Animation Engine ✅ Complete

**Branch:** `feature/phase-12-animation` | **CMake:** v1.2.0 | **Tests:** 233/233 pass (17 new test cases across 3 executables)

**What was built:**
`Easing.hpp` — 11 `inline` lambdas (`Linear`, `EaseInQuad`, `EaseOutQuad`, `EaseInOutQuad`, `EaseInCubic`, `EaseOutCubic`, `EaseInOutCubic`, `EaseOutBack`, `EaseOutElastic`, `Spring`, `Bounce`) in `ImFrame::Easing::`. All runtime-only; not constexpr (MSVC cmath constraint).
`Tween<T>` (constrained by `Lerpable`) — `Play/Pause/Resume/Reverse/Reset/Restart`; `Update(dt) -> T`; `OnComplete(std::function<void()>)` fires exactly once; `Progress()`/`RawProgress()`/`IsPlaying()`/`IsDone()`. Header-only.
`AnimatedValue<T>` — `SetTarget/Update/Value/SnapToTarget`; exponential decay formula `current + (target-current)*(1-exp(-speed*dt))`; frame-rate-independent by construction. Header-only.
`AnimSequence` — `Then(Tween<T>&)/Wait(float)/Call(std::function<void()>)/Loop(bool)`; step list as `std::vector<std::function<bool(float)>>`; `Update(dt)`/`IsDone()`. Header-only.
No new `src/` files — entire Phase 12 is header-only templates in `include/ImFrame/Anim/`.

**Key decisions:** Easing lambdas are `inline` not `inline constexpr` (MSVC cmath not constexpr); `OnComplete` uses `std::function` not `Delegate` (one-shot, no SBO benefit, keeps Anim/ self-contained).

---

## Phase 11 — Layout Containers ✅ Complete

**Branch:** `feature/phase-11-layout-containers` | **CMake:** v1.1.0 | **Tests:** 216/216 pass (26 new test cases across 5 executables)

**What was built:**
`ChildScope` — RAII wrapper (`_visible`, `_active` flags); non-inline destructor in `ChildScope.cpp` calls `ImGui::EndChild()` always (ImGui 1.92+ contract); moveable-only.
`Panel` — fluent builder wrapping `BeginChild`; `Size/Border/Padding/Background` setters; `Begin() -> [[nodiscard]] ChildScope`; Padding/Background pushed before `BeginChild()`, popped immediately after.
`HStack(spacing)` — `Render(Renderable auto&&...)` variadic template; calls `Internal::HStackSameLine(spacing)` between items.
`VStack(spacing)` — same pattern; `Internal::VStackDummy(spacing)` between items.
`Grid(columns)` — `Render(Renderable auto&&...)` with `Internal::GridBeginTable/GridNextColumn/GridEndTable`; early return if `BeginTable` returns false; `EndTable` only called on success.
`ScrollArea(id)` — `HorizontalBar/VerticalBar/Begin`; instance methods `ScrollToBottom/ScrollPosition/SetScrollPosition` callable from inside an active scope.
`LayoutImpl.cpp` — single TU containing all Internal helpers for HStack/VStack/Grid.

**Key decisions:** ChildScope non-inline dtor (no imgui.h in public header); layout Render() templates use same Internal helper pattern as Phase 10 widgets; EndTable only called when BeginTable returns true (differs from EndChild which is always called).

---

## Phase 10 — Core Widget Set ✅ Complete

**Branch:** `feature/phase-10-widgets` | **CMake:** v1.0.0 | **Tests:** 190/190 pass (12 widget executables, 37 new TEST_CASEs)

**What was built:**
`Types.hpp` — `Vec2`, `Vec4`, `TextureHandle`, `Renderable` concept (ImGui-free; layout-verified by static_assert in each .cpp).
12 widget headers (`Button`, `Checkbox`, `Radio`, `ColorEdit`, `Image`, `ProgressBar`, `Separator`, `Spacer`, `Text`, `TextInput<T>`, `Slider<T>`, `Combo<T>`) — all in `ImFrame::Widgets::`, fluent builder API, `Show()` returns `bool`.
9 non-template widget `.cpp` files + `WidgetImpl.cpp` (internal helpers: `ShowTextInputStr/U8`, `ShowSliderInt/Float/Double`, `ShowComboImpl`).
12 test files in `Tests/Widgets/` using `TestHeadlessBackend` + `Application::OnUi`.
`Renderable` concept defined for Phase 11 layout containers.

**Key decisions:** Vec2/Vec4 own types (not ImGui types) to preserve public-header invariant. Template Show() inline in header dispatches via `if constexpr` to non-template `ImFrame::Internal` helpers defined in WidgetImpl.cpp.

---

## Phase 9 — Icon Font Integration ✅ Complete

**Branch:** `feature/phase-9-icon-fonts` | **CMake:** v0.9.5 | **Tests:** 149 pass (4 icon tests: 2 runtime + 5 static_assert compile-time)

**What was built:**
`Icons.hpp` — 1,402 FA6 Free glyph constants as `inline constexpr const char*` in `ImFrame::Icons::Fa::`, range sentinels `FA_RANGE_MIN`/`FA_RANGE_MAX` as `constexpr uint32_t`, three composition macros (`IMF_ICON`, `IMF_ICON_LABEL`, `IMF_ICON_LABEL_AFTER`) for compile-time string-literal concatenation.
`IconFontConfig` struct + `IconFont::Load(ImFontAtlas*, const IconFontConfig&) -> ImFont*` — atlas merge with `MergeMode=true`, configurable glyph range and vertical offset; `ImFont`/`ImFontAtlas` forward-declared in public header (no imgui.h in public API).
`FontConfig` extended with `isIconFont` + `glyphOffsetY`; `Application::Run()` dispatches to `IconFont::Load()` for icon fonts.
Vendored `fa-solid-900.ttf`, `fa-regular-400.ttf`, `fa-brands-400.ttf` in `Assets/Fonts/` (SIL OFL 1.1).

**Key fix:** FA6 renamed `home` → `house` — constant is `Fa::House` (U+F015, `\xef\x80\x95`).

**Tests:** `IconFont_test.cpp` — 5 compile-time `static_assert`s + 4 runtime TEST_CASEs (`nullptr` guards pass always; atlas tests skip gracefully without font in CWD, pass when run from binary output dir).

---

## Phase 8 — Theme Engine ✅ Complete

**Branch:** `feature/phase-8-theme-engine` | **CMake:** v0.9.0 | **Tests:** 15 pass (8 ColorToken + 7 Theme)

**What was built:**
`ColorToken` (constexpr RGBA, `ColorFromHex` hex factory),
`SpacingToken` + `RadiusToken` (constexpr aggregates mapping to ImGui style vars),
`ColorRole` enum (19 semantic roles),
`Theme` struct (constexpr aggregate with `Apply()` — 63 direct `ImGuiCol_*` assignments, `static_assert(ImGuiCol_COUNT == 63)`),
`ThemeBuilder` (header-only fluent builder: `SetColor/SetSpacing/SetRadius/Build()`),
`Dracula` / `Nord` / `CatppuccinMocha` / `Light` as `inline constexpr Theme::Theme` globals in `ImFrame::Themes::`,
`Application.cpp` wired to call `_pendingTheme->Apply()` before `DockSpace::Begin()`.

**Tests:** `ColorToken_test.cpp` (8, no ImGui context), `Theme_test.cpp` (7, HeadlessBackend), all `[unit]`

**Fix applied:** `Application.hpp` forward-declaration changed from `class Theme` to `struct Theme` (MSVC `/WX` mismatch error).

---

## Current Phase: Phase 9 — Icon Font Integration

**Status:** Not Started
**Branch:** `feature/phase-9-icon-fonts` (create from `develop` after merging Phase 8)

### Phase 9 Completion Criteria
*(Populate from `PHASE_09_PROPOSAL.md` before starting)*

---

## Phase 7 — Application & Window ✅ Complete

**Branch:** `feature/phase-7-app-window` | **CMake:** v0.8.0 | **Tests:** 6 pass

**What was built:**
`Application` (DI backend, fluent builder: WithFont/WithTheme/OnUi/OnUpdate/OnClose/WithMenuBar, Run/RunOneFrame/DeltaTime/DpiScale),
`WindowManager` (Register/RenderMenu/Clear),
`WindowScope` + `BeginWindow()` (RAII ImGui::Begin/End with move semantics + `_valid` flag),
`DockSpace` (Begin/End/WithMenuBar/InitDefaultLayout three-column: 25%/50%/25%),
`HeadlessBackend` (internal null backend),
`IBackend::CancelClose()` + `GLFWOpenGL3Backend` implementation.

**Tests:** `Application_test.cpp` (4), `DockSpace_test.cpp` (2), all labeled `[unit]`

---

## Phase 6 — Supporting Utilities ✅ Complete

**Branch:** `feature/phase-6-supporting-utilities` | **CMake:** v0.6.0 | **Tests:** 35 pass

**What was built:**
`Timer`/`TimerHandle` (frame-driven, sorted vector, O(k log n)),
`Logger` singleton (BackgroundWorker-backed async dispatch, `IMF_LOG_*` macros, compile-time filtering),
`ConsoleSink` (ANSI/TTY-detected) + `FileSink` (byte-threshold rotation) + `UiSink` (ring buffer),
`Config` (TOML-subset: bool/int64/float/string/sections, Get/Set/Load/Save, coalesced saves, FileWatcher integration, `OnChanged` signal).

**Tests:** Timer (11), Logger (11), Config (13), labeled `[unit]` and `[unit;tsan]`

---

## Phase 5 — Event System ✅ Complete

**Branch:** `feature/phase-5-event-system` | **CMake:** v0.5.0 | **Tests:** 43 pass

**What was built:**
`Delegate<R(Args...)>` (zero-heap 16-byte SBO, header-only),
`Signal<void(Args...)>` + `ThreadSafeSignal` (copy-list-before-iterate pattern),
`Connection` (RAII slot handle),
`EventBus` (shared_mutex per type, Pimpl, Meyer's singleton, `DispatchAsync` via BackgroundWorker),
`SubscriptionToken` (weak_ptr-based, safe after bus destruction).

**Tests:** Delegate (18), Signal (12), EventBus (13), labeled `[unit]` and `[tsan]`

---

## Phase 4 — Concurrency Utilities ✅ Complete

**Branch:** `feature/phase-4-concurrency` | **CMake:** v0.4.0 | **Tests:** 19 pass

**What was built:**
`Thread` (RAII jthread, OS naming/priority/affinity, non-copyable/non-moveable),
`ThreadPool` (Dmitry Vyukov MPMC ring buffer, capacity 1024, counting_semaphore workers),
`BackgroundWorker` (mutex+deque+two CVs, FIFO, Drain()),
`FileWatcher` migrated from `std::thread` → `std::jthread` (4 sites).

**Tests:** Thread (6), ThreadPool (6), BackgroundWorker (7), labeled `[unit]` and `[tsan]`

---

## Earlier Completed Phases

| Phase | Name | CMake | Notes |
|-------|------|-------|-------|
| 0 | Project Scaffold | v0.0.0 | CMake, vcpkg, CI, `.clang-format`, `.clang-tidy` |
| 1 | Platform Backend Abstraction | v0.1.0 | `IBackend`, `GLFWOpenGL3Backend`, `Error` type |
| 2 | Core Types | — | ⏭ Skipped — will revisit if needed |
| 3 | Filesystem Utilities | v0.3.0 | `Path`, `File`, `Directory`, `FileWatcher` (20 tests) |

---

## Upcoming Phases

| Phase | Name |
|-------|------|
| **8** | **Theme engine + 4 built-in themes ← NEXT** |
| 9 | Icon font integration |
| 10 | Core widget set (14 widgets) |
| 11 | Layout containers |
| 12 | Animation system |
| 13 | Overlays (Toast, Modal, ContextMenu) |
| 14 | Table + PropertyGrid |
| 15–18 | TBD (data binding, scripting, hot-reload, API leak tooling) |
| 19 | FrameInfo, InputEvent |
| 20–23 | Vulkan / Metal / DX12 / WebGPU backends |
| 24–26 | Rendering layer |
| 27–29 | Widget tree + reactive state |
