---
name: imframe-wiki-page
description: Use whenever asked to write, draft, or update a GitHub Wiki page for ImFrame, or asked to document a feature for the "Getting Started", "Core Concepts", "Feature Guides", "API Reference", or "Contributing" books established in Phase 49.2. Always apply this skill's structure rather than writing free-form documentation, since the Wiki's five-book structure and per-page format are deliberate and consistent across every existing page.
---

# ImFrame Wiki Page Writer

The ImFrame GitHub Wiki (`ImFrame.wiki` repository) is organized into five books, each with a consistent per-page structure, established in Phase 49.2. New or updated pages must match the existing structure and tone.

## The five books — pick the right one

| Book | Audience | Examples |
|---|---|---|
| 1. Getting Started | First-time users, zero to running app | Installation, Your First Application, Migrating from ImGui |
| 2. Core Concepts | Users learning the framework's model | The Widget Tree, State Management, Theming |
| 3. Feature Guides | Users solving a specific task | Custom Title Bar, Networking, Plugins & Scripting |
| 4. API Reference Index | Users who already know ImFrame, need a lookup | One index page per `ImFrame::` namespace, linking into Doxygen |
| 5. Contributing & Internals | Contributors, deep technical readers | Architecture Overview, Coding Standards, Writing a Proposal |

If unsure which book a topic belongs in, default to: conceptual/explanatory → Book 2; task-oriented "how do I X" → Book 3.

## Required page structure

Every page (except Book 4 index pages, which follow a simpler listing format described below) follows this structure:

1. **One-paragraph summary** — readable in 10 seconds. States what the page covers and why it matters, no preamble.
2. **"When to use this"** — a short section, never more than five bullets, one sentence each. Skip this section if the page is purely conceptual (Book 2) rather than task-oriented (Book 3).
3. **Main content** — the substantive explanation, with code examples (see rule below) and, where relevant, an embedded screenshot reference from the DemoApp's CI reference images.
4. **"Common mistakes"** — the three most frequent errors a user is likely to hit, each with a one or two sentence explanation of the fix.
5. **"See also"** — links to related pages across any of the five books.

## Code examples — hard rule

**Every code example must be traceable to real source** — either the `Examples/DemoApp/` source from Phase 49.1, or genuine library source. Never invent a plausible-looking example from scratch if a real one exists. If no real example exists yet for a topic being documented, say so explicitly and flag it rather than fabricating one — a fabricated example that doesn't actually compile is worse than no example, since `Tests/WikiExamples/wiki_examples_test.cpp` compiles every Wiki code snippet in CI and a fake one will fail that build.

## Screenshots

Reference the DemoApp's committed CI reference images (`Tests/Reference/DemoApp/`) rather than describing a manually-crafted screenshot. If the relevant section/theme combination doesn't have a reference image yet, note that rather than inventing a placeholder description.

## Book 4 (API Reference Index) format — simpler

Each Book 4 page is a flat list, one `ImFrame::` namespace per page (`Core`, `Utility`, `App`, `Theme`, `Tree`, `Rendering`, etc.). For each public type or free function: one-sentence description, link to the corresponding Doxygen page. No prose sections, no examples — this book is a lookup table, not a tutorial. Cross-references between related types are added manually as inline links within the one-sentence descriptions, not as a separate "See also" section.

## Tone and writing style

- Direct, second person ("you"), present tense.
- No marketing language. Avoid "powerful", "seamless", "simply", "just" (as in "just call X" — if it were that simple it wouldn't need documenting).
- Technical precision over friendliness — assume the reader is competent and wants the real behavior, including caveats and edge cases, not a simplified happy-path-only version.
- Cross-reference other phases/types by their actual names (`State<T>`, `InheritedWidget<Theme>`) — never paraphrase a type name loosely.

## File naming and placement

Page files use `Title-With-Hyphens.md` matching the page title, placed in the correct book's subdirectory (e.g. `Book-3-Feature-Guides/Custom-Title-Bar.md`). Update `_Sidebar.md` to include the new page in the correct book section if adding a new page rather than editing an existing one.

## After writing

If the page documents a feature whose code examples don't yet exist in `wiki_examples_test.cpp`, note that the CI test file needs a corresponding entry — don't silently leave a snippet untested.
