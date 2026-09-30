---
title: Documentation audit roadmap
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation audit roadmap

This audit prepares a frozen Sagan 1.0 documentation release while teaching the
language from its simplest ideas to its most consequential design choices. Work
through one stage at a time. Do not turn the roadmap itself into another large
reference document.

## How each stage works

For each stage:

1. read the listed pages in order;
2. run the relevant executable examples;
3. discuss unclear rules and their practical consequences;
4. correct the documentation or implementation evidence;
5. record one of the page outcomes below; and
6. advance only when the remaining questions are deliberately deferred.

### Page outcomes

- **Work in progress:** not yet audited or still being revised.
- **Publication ready:** confirmed for Sagan 1.0 by the current reviewer.
- **Review needed:** understandable enough to defer, but reserved for Zach's
  focused review.

Only change a page's status after an explicit review decision. A
publication-ready page records `verified_in: 1.0.0`, the review date, and the
reviewer's name. A page sent to Zach uses `status: review-needed` and remains
`publication_ready: false` until his questions are resolved.

## Stage 0 — site experience

Review the home page and the site as a whole.

- Confirm layout, colors, typography, mobile behavior, and accessibility.
- Confirm that Learn, Reference, Tools, and Project development are easy to
  distinguish.
- Test navigation, search, version selection, page-status markers, and links.
- Confirm that reader documentation is not cluttered with internal planning.

**Decision:** is this the site structure and visual language we want for 1.0?

## Stage 1 — purpose and design principles

Read About Sagan, Design overview, Philosophy, and Goals and non-goals.

- Confirm what Sagan is for and who it serves.
- Confirm the simulation-first, strongly typed, composition-oriented identity.
- Make each formal term understandable to a reader without language-design
  experience.

**Decision:** does this accurately explain what Sagan is and why it exists?

## Stage 2 — install, write, and run a program

Review Downloads, Installation, First program, Editor support, and the first
runnable examples.

- Install or build the pinned 1.0 compiler using the documented path.
- Write and run a program from a Bash terminal.
- Install the compatible VS Code extension and exercise every documented
  editor feature.
- Confirm that expected output, failures, and troubleshooting are sufficient
  for independent practice.

**Decision:** can a new user begin experimenting with Sagan without outside
help?

## Stage 3 — source and basic syntax

Review the early language tour plus the lexical, declaration, expression, and
statement reference sections.

- Cover source text, names, comments, literals, operators, and interpolation.
- Cover variables, mutability, assignment, functions, calls, and control flow.
- Run short focused examples for each everyday construct.

**Decision:** does ordinary Sagan code read and behave as expected?

## Stage 4 — programs, modules, and tools

Review modules, packages, entry points, command-line use, diagnostics, and the
larger runnable examples.

- Build and run single-file and multi-file programs.
- Confirm imports, exports, package layout, and entry-point behavior.
- Confirm that compiler and editor errors are understandable and actionable.

**Decision:** can a user organize and troubleshoot a real Sagan project?

## Stage 5 — values and data modeling

Review collections, optionals, enums, pattern matching, classes, construction,
fields, methods, privacy, and `self`.

- Start with common modeling tasks before discussing advanced type machinery.
- Confirm accepted examples, rejected examples, and their diagnostics.
- Identify any behavior whose implications need a deeper explanation.

**Decision:** are Sagan's everyday data-modeling tools coherent and predictable?

## Stage 6 — types, generics, and composition

Review inference, conversions, overloads, function types, generics, constraints,
`face`, `is`, `has`, conformance, defaults, conflicts, and dispatch.

- Explain each formal rule in plain language.
- Show why interface-based composition was chosen and what users gain or lose.
- Defer confusing or disputed interactions to Zach rather than blocking simpler
  confirmed material.

**Decision:** can readers reason about reusable abstractions without hidden
rules?

## Stage 7 — lifetime and failure behavior

Review reference counting, strong and weak references, cycles, absence,
closures, `hope`, `unless`, `finally`, `scream`, and native runtime failures.

- Trace representative values through construction, sharing, failure, cleanup,
  and destruction.
- Separate guaranteed Sagan behavior from backend implementation details.
- Send unresolved ownership or cleanup interactions to Zach's queue.

**Decision:** can users predict how values live, fail, and clean up?

## Stage 8 — numerics and simulation semantics

Review checked arithmetic, units, points and vectors, Cartesian and spherical
representations, coordinate frames, floating-point behavior, determinism, and
the math, physics, and rendering boundaries.

- Confirm exact guarantees rather than aspirations.
- Explain the consequences for real simulations and cross-platform results.
- Expect this stage to produce most of Zach's focused-review material.

**Decision:** are Sagan's simulation guarantees useful, precise, and honest?

## Stage 9 — Zach review and 1.0 freeze

Collect every page marked **Review needed** into a short queue. For each page,
provide the disputed rule, a plain-language explanation, representative
programs, observed behavior, and the decision required. Resolve each item or
record a precise 1.0 limitation.

Then:

1. pin the exact Sagan 1.0 source ref and `v1.0.0` tag;
2. ensure every public 1.0 page is publication-ready for `1.0.0`;
3. run `bash scripts/docs.sh check` and the release documentation gate;
4. publish and archive the immutable `1.0.0` documentation version; and
5. return `experimental` to tracking continued development on `main`.

The archived 1.0 documentation must never be rewritten to match later language
changes. Corrections belong in a new released documentation version.

## Start here

Begin with **Stage 0 only**. Record concise observations about the site shell,
navigation, and presentation. Once those are resolved, move directly through
Stage 1 and then Stage 2 so the reviewer can write and run Sagan programs early
enough to use them throughout the remaining audit.
