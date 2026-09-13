---
name: imframe-cross-platform-checklist
description: Use whenever designing, proposing, implementing, or reviewing any ImFrame feature that touches platform-specific behavior — windowing/chrome, file system, input, rendering backends, clipboard, notifications, audio, or anything with an IBackend-level implementation per platform. Apply this checklist proactively during design and proposal-writing, not only when explicitly asked about cross-platform concerns — silent platform gaps discovered only at final review (Phase 49.3) are far more expensive to fix than ones caught at design time.
---

# ImFrame Cross-Platform Verification Checklist

ImFrame targets Windows, macOS, Linux (X11 + Wayland), iOS, Android, and WebAssembly/Emscripten. Phase 49.3 established a formal verification process for this before the v4.0 API freeze, but the checks it encodes are useful at any point a platform-specific feature is being designed — catching a gap during design is far cheaper than catching it during final review.

## The core rule: no silent failures

**A platform limitation must always be an explicit, documented `Error::NotSupported` (or equivalent) return — never a silent no-op or a crash.** If `SetMicaEffect()` can't work on Linux, it must return an error the caller can check, not silently do nothing. When proposing or reviewing a platform-specific API, always ask: "what happens on the platform where this doesn't exist?" and make sure the answer is an explicit, testable signal.

## When designing a new platform-touching feature, check

**Hardware/vendor coverage.** Does behavior differ across NVIDIA / AMD / Intel / Apple Silicon / ARM Mali / software renderers? If the feature touches rendering at all, assume yes until proven otherwise.

**OS version range.** What's the minimum supported version per platform, and does the feature degrade gracefully below the version where the underlying API exists? (Reference points: Windows 10 1903+, macOS 10.15+, Ubuntu 22.04+, iOS 13+, Android API 28+ — confirm current minimums against `Docs/PlatformSupport.md` if available, since these may be revised.)

**System settings interaction.** Does the feature behave correctly under: DPI scale 100–300%, OS dark/light mode, OS reduced-motion preference, OS high-contrast mode, RTL system locale, large/accessibility text size? A feature that ignores `Application::PrefersReducedMotion()` or doesn't scale with `DpiScale()` is incomplete, not just imperfect.

**Fresh install path.** Could a new contributor get from "clean machine" to "this feature working" using only documented steps? If the feature needs an undocumented prerequisite (an SDK, an environment variable, a permission grant), that's a documentation gap to close before considering the feature done.

**Rendering consistency.** If the feature produces visual output, is the result expected to be pixel-identical across backends, or will it legitimately differ (e.g., macOS traffic light rendering vs. Windows window controls)? Legitimate differences must be called out explicitly in `Docs/PlatformSupport.md`, not left as an unexplained inconsistency.

**Network conditions** (if applicable). Slow connection, timeout, mid-transfer disconnect, server error response, malformed response — does the feature fail with a clear `Error` value on every platform, without freezing the UI?

**Backend switching.** If the platform supports multiple backends (e.g., Windows: OpenGL3/Vulkan/DX12/WebGPU), does the feature work identically regardless of which one is active?

**Mobile-specific concerns** (iOS/Android only). Safe area insets, keyboard avoidance, app suspend/resume without corrupting GPU state, low-memory warnings triggering graceful resource eviction (not crashes).

**Emscripten/browser concerns** (if applicable). Does the feature have a sensible fallback or explicit unsupported-error path in a browser context? Test across Chrome, Firefox, Safari (macOS host only), and Edge if the feature is rendering- or input-related.

**Performance baseline.** Desktop discrete GPU and mid-range mobile hardware have very different realistic budgets. Don't hold a mobile or WebAssembly target to a desktop discrete-GPU performance bar — but do make sure a documented, platform-appropriate baseline exists rather than no baseline at all. Reference baselines from the most recent code review phase if available (desktop discrete < 2ms GPU / integrated < 4ms / Apple Silicon < 2ms / iOS < 3ms / Android mid-range < 6ms / WASM < 8ms, as last established — confirm these haven't been superseded).

## When reviewing a platform-specific implementation

Walk the checklist above as questions, and for each one that applies to the feature being reviewed, confirm there's a test or a documented limitation — not just "it works on my machine." If a gap is found, the fix is either: implement the missing case, or add an explicit, tested `Error::NotSupported` path plus a `Docs/PlatformSupport.md` entry. Silence is never an acceptable resolution.

## Output

When this checklist surfaces gaps during a proposal or review, record them either as new content in the relevant phase proposal's invariants/testing sections (if still in design), or — if the implementation is already complete and the gap is a real discovered limitation — as a `DECISIONS.md` entry (see the `imframe-decisions-log` skill) plus a row in the platform parity matrix.
