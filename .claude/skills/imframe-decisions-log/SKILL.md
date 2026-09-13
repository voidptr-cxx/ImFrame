---
name: imframe-decisions-log
description: Use whenever the user reports a completed ImFrame phase with real implementation findings, asks to "log this decision", "update DECISIONS.md", or describes something discovered during actual implementation (an API quirk, a design choice made under real constraints, a workaround) that future phases need to know about. Always check whether implementation findings reported by the user warrant a DECISIONS.md entry, even if not explicitly asked — undocumented implementation discoveries are lost context for future sessions.
---

# ImFrame Decisions Log Writer

`DECISIONS.md` is the project's durable record of real architectural and implementation decisions discovered during actual coding — as distinct from the phase proposals, which describe planned design. Proposals say what should happen; `DECISIONS.md` says what actually happened and why, especially when it differed from or refined the plan.

## When an entry is warranted

Write an entry when the user reports something that:
- Changed an originally planned approach based on a real constraint discovered during implementation (e.g. an upstream library's API differing from what was assumed).
- Established a pattern that future phases must follow or be aware of.
- Documents a non-obvious tradeoff between two implementation options that were both viable.
- Records a workaround for a platform, compiler, or dependency quirk.

Do not write an entry for routine, expected implementation work that matches the proposal exactly with no surprises — that doesn't need a decision record, just a `PHASE_STATUS.md` checkbox.

## Required format

```markdown
## YYYY-MM-DD — Short Title
**Decision:** One or two sentences stating what was decided, in plain declarative form.
**Alternatives considered:** The other options that were viable, named specifically — not "we could have done it differently" but the actual named alternatives.
**Rationale:** Why this option won. Reference concrete constraints (performance, platform behavior, API limitations, consistency with an earlier phase's pattern).
**Impact:** Which future phase(s) this affects and how — name the phase number and what it must do differently or can now rely on as a result.
**Discovered during:** Phase NN implementation.
```

Use today's actual date if known from context, otherwise use the most recent date already present in the file plus a reasonable increment, or ask the user if precision matters.

## Insertion order

Entries are stored **reverse-chronologically** — the newest entry goes at the very top of the decisions list, immediately before the next-most-recent existing entry. Never append to the bottom of the file. When editing the file, insert the new entry block directly above the previously-most-recent entry, and leave everything below it untouched.

## Multiple findings in one report

If the user's phase-completion report contains more than one noteworthy finding (e.g. two separate implementation discoveries), write them as two separate dated entries, both inserted at the top in the order they're most relevant to read (most architecturally significant first), not necessarily the order the user mentioned them.

## Style rules

- Keep each field to one to three sentences. This is a log, not a proposal — terse and factual.
- Name real types, real files, real APIs. "The Vulkan backend" is acceptable; prefer "`VulkanContext`'s `ViewportCommandPool`" when the specific name is known.
- The Impact field should almost always name a specific downstream phase number, since the entire point of the log is forward-looking context transfer.
- Never editorialize or add encouragement/praise in the entry itself — it's a technical record, not feedback to the user.

## After writing

If the user is also expecting the next phase's proposal in the same turn, write the `DECISIONS.md` update first (it's quick), then proceed to the proposal — referencing the new decision naturally in the proposal's Foundation section where relevant. See the `imframe-phase-proposal` skill for that format.
