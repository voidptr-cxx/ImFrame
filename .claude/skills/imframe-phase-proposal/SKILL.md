---
name: imframe-phase-proposal
description: Use whenever asked to write, draft, update, or extend an ImFrame development phase proposal — e.g. "write phase N proposal", "draft a proposal for the X system", "create proposals from phase A to phase B". Always consult this skill before writing any PHASE_NN_PROPOSAL.md file, including sub-phase proposals (NN.1, NN.2, etc.), since structural drift from the established format breaks consistency across the full 50+ proposal set. This skill encodes the exact required structure, tone, and level of technical depth.
---

# ImFrame Phase Proposal Writer

ImFrame is developed as a sequence of numbered phases, each documented as a standalone proposal file before implementation. Every proposal in the existing set (Phase 00 through Phase 50, plus sub-phases like 49.1–49.4) follows an identical structure. New proposals must match it exactly — inconsistency between proposals is a documentation defect, not a stylistic choice.

## The golden rule

**A proposal always starts from what the previous phase actually delivered, not from what was originally planned.** If the user reports specific implementation details from a completed phase (a new file, a discovered API quirk, an architectural adjustment), the next phase's proposal must reference those specifics by name in its Foundation section. Never write a generic "Phase N-1 was completed" — name the actual artifacts.

## Required structure, in order

### 1. Title block

```
# Phase NN — Short Title
> ImFrame · vX.Y.Z · tag1 · tag2 · tag3
```

The version tag follows the project's semantic versioning scheme for that phase (check `.claude/PHASE_STATUS.md` or prior proposals for the current version if unsure). The tags after the version are 3–5 short noun phrases naming the phase's key deliverables, separated by `·`.

### 2. `## Foundation`

One to two short paragraphs. States what the immediately preceding phase delivered — using specific file names, type names, and method names where the user has provided them — and what gap or capability the new phase addresses. This section is the connective tissue between phases; it must never be skipped or generic.

### 3. `## What Gets Built`

The bulk of the proposal. Organized into `###` subsections, one per major component or type being introduced. Written in **flowing technical prose, not bullet lists**. Each subsection should read like a precise specification: name the type, describe its constructor and key methods with exact signatures where useful, explain *why* a design choice was made when it isn't obvious, and describe how it composes with previously-established types from earlier phases (cite the phase number when referencing prior work, e.g. "via `BackgroundWorker` from Phase 4").

Do not pad with bullet points inside this section. A few sentences to a few paragraphs per component is normal. Long proposals are expected — depth is the point.

### 4. `## New Files` (or `## No New Files` when applicable)

A single fenced code block showing a directory tree of new and modified files, using `←` comments to annotate purpose where helpful. If a phase deliberately touches only existing files (a hardening or cleanup phase), state `## No New Files` explicitly and explain why.

### 5. `## Testing`

One paragraph per new test file, naming the file under `Tests/...` and describing concretely what it verifies — not "tests work correctly" but the specific assertions (e.g. "verifies `WaitAll()` returns after all tasks complete, return values are correct, queue-full condition blocks and unblocks correctly").

### 6. `## Invariants Introduced`

Bolded short invariant statements, each followed by an explanation of what breaks if it's violated and how it's enforced (CI grep, static_assert, runtime assert, code review). This section is what keeps future phases honest — invariants are promises later phases must not break.

### 7. `## What Phase N+1 Inherits`

A bullet list (this is the one section where bullets are correct) connecting concrete deliverables from this phase to what the next phase will use them for. Each bullet should name a specific type or method and what it enables downstream.

## Style rules

- No marketing language, no exclamation points, no "powerful" / "seamless" / "robust" filler adjectives.
- Use exact C++ syntax for signatures: `std::expected<T, Error>`, `[[nodiscard]]`, `Delegate<Sig>`, etc. — never paraphrase a signature loosely.
- Reference earlier phases by number whenever reusing an established pattern or type, so the proposal documents its own dependency chain.
- When the user reports real implementation findings (not just "phase complete" but actual file lists, discovered quirks, API behavior), capture anything noteworthy as a candidate `DECISIONS.md` entry — see the `imframe-decisions-log` skill for that format — before or alongside writing the next proposal.

## Multi-phase requests

If asked for a range (e.g. "phases 25 through 37"), write each phase as a fully independent, complete proposal — never abbreviate later phases in a batch just because earlier ones in the same batch were thorough. Each one must stand alone as if it were the only proposal being delivered that session. Split into separate files; do not leave multiple phases concatenated in one file for the user to receive.

## Sub-phase numbering

Sub-phases (`NN.1`, `NN.2`, `NN.3`...) are used when a single integer phase needs to be split into focused, sequential deliverables that share a version number or are tightly coupled in purpose (e.g. Phase 49.1 DemoApp, 49.2 Documentation, 49.3 Code Review, 49.4 Git Commit — all closing out v3.9.x before the major version bump in Phase 50). Sub-phases still follow the full structure above, including their own Foundation, Testing, and Invariants sections.

## After writing

Save the file as `PHASE_NN_PROPOSAL.md` (zero-padded to two digits, e.g. `PHASE_07_PROPOSAL.md`, `PHASE_49_3_PROPOSAL.md` for sub-phases) and present it to the user. Do not summarize the proposal's content back to the user in your own words after presenting it unless asked — let the document speak for itself, then offer a few specific, non-redundant observations about implementation risk or design tension if genuinely useful.
