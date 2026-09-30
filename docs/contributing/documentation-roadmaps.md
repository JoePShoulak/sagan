---
title: Documentation roadmaps
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation roadmaps

These roadmaps guide the work leading into the 1.0 documentation audit and its
eventual public release. They serve different purposes:

1. the **documentation expansion roadmap** turns the internal notes into a
   complete, publishable documentation set; and
2. the **behavior-confirmation audit** walks through the language from basic to
   complex so its behavior and documentation can be approved together.

Reaching implementation version 1.0 does not automatically make a page
publication-ready. Each page must still be checked against the released
compiler and confirmed for the current documentation version.

## Roadmap A: expand the 1.0 documentation

### Phase 0 — establish the 1.0 evidence baseline

- Tag the exact compiler revision that defines Sagan 1.0.
- Record supported platforms, toolchain versions, CLI modes, and library
  versions.
- Pin the compatible VS Code extension release and record its relationship to
  the Sagan 1.0 grammar and compiler release.
- Produce machine-readable grammar, token, diagnostic, and public-API
  inventories where the implementation can provide them.
- Freeze the 1.0 examples and expected outputs used as documentation evidence.
- Select the first public documentation version and keep the site internal
  until the publication gate passes. This ongoing unreleased version is named
  `experimental`.

**Exit condition:** every documentation claim can point to the 1.0 source,
tests, a design decision, or an explicitly labeled limitation.

### Phase 1 — reconcile and organize the existing material

- Audit every existing page for duplicate or contradictory statements.
- Assign one canonical home to each rule; link to it rather than copying it.
- Separate language guarantees from implementation details and teaching prose.
- Move superseded design discussion into decisions or RFC history.
- Add a glossary and consistent terminology for syntax, types, values,
  ownership, modules, and simulation concepts.

**Exit condition:** the navigation and cross-links expose one clear source of
truth for every 1.0 subject.

### Phase 2 — complete the normative language reference

- Finalize the lexical specification and complete grammar.
- Document declarations, expressions, statements, modules, visibility, and
  entry points.
- Specify name resolution, type checking, conversions, generics, interfaces,
  classes, enums, matching, exceptions, and memory behavior.
- State evaluation order, mutation rules, control-transfer behavior, and every
  portability or determinism guarantee.
- Give each rule at least one accepted example and each important restriction
  a rejected example with its expected diagnostic.

**Exit condition:** an implementer or advanced user can answer a language-rule
question without relying on compiler source code.

### Phase 3 — build the learning path

- Rewrite Getting started around installation, a first executable program,
  testing, building, and troubleshooting.
- Add a first-session VS Code path covering extension installation, opening a
  `.sagan` file, selecting or confirming the language mode, and recognizing the
  features the extension does and does not provide.
- Turn the language tour into a progressive sequence that never depends on a
  concept not yet introduced.
- Add focused how-to guides for common simulation, geometry, numerical, module,
  and composition tasks.
- Build complete examples that grow from a small program to a representative
  multi-module simulation.
- Add explicit links from tutorial explanations to their normative reference
  rules.

**Exit condition:** a new user can progress from installation to a meaningful
Sagan program using only the published documentation.

### Phase 4 — document libraries and tools

- Publish the automatically available math API from its authoritative source.
- Document physics and rendering as explicit-import first-party libraries,
  without blurring their boundary with built-in math.
- Give the VS Code extension its own user guide covering Marketplace and local
  installation, compatible Sagan versions, file association, syntax scopes,
  bracket and comment behavior, configuration, examples, updates, known
  limitations, troubleshooting, and uninstalling or reverting it.
- Document how the extension is built, packaged, tested, versioned, and released,
  including the process for keeping its grammar synchronized with the language.
- Document the compiler CLI, build outputs, diagnostics, any formatter or
  language-server behavior, package workflow, and debugging tools. Clearly
  distinguish extension-provided coloring and editing conveniences from
  compiler-backed semantic features.
- Generate API material where possible, then add human explanations and
  examples rather than treating generated symbols as a complete reference.
- Record platform support and observable compatibility guarantees.

**Exit condition:** every supported public command, tool, extension feature, and
core-library API has discoverable reference material and a verified example.

### Phase 5 — finish implementation and contributor documentation

- Describe the compiler pipeline and the boundaries between tokenizer, parser,
  semantic analysis, code generation, runtime, and libraries.
- Document repository layout, local setup, test categories, coverage,
  diagnostics, release preparation, versioning, and documentation maintenance.
- Preserve Schematic attribution, Zachary Westerman's contribution, and GPLv3
  provenance in both user-facing history and contributor guidance.
- Explain how decisions and RFCs are proposed, accepted, superseded, and
  reflected in the reference.

**Exit condition:** a contributor can reproduce the supported build, validate
their changes, and identify the authority for a design decision.

### Phase 6 — publication gate

- Run the behavior-confirmation audit below.
- Resolve every blocking contradiction and broken command.
- Complete accessibility, search, navigation, link, rendering, and mobile
  checks.
- Confirm that all public pages are marked `publication-ready` for the chosen
  documentation version and that experimental-only material is excluded or clearly
  labeled.
- Have Zach review the expert topics and historical attribution identified in
  the final audit stages.
- Build a release candidate on the production hosting path before making the
  site public.

**Exit condition:** the release check, site build, human review, and production
smoke test all pass against the same documentation revision.

## Roadmap B: behavior-confirmation audit

This audit is deliberately ordered from concrete, low-coupling concepts to
cross-cutting or controversial ones. Complete a stage before treating later
pages as confirmed. A later discovery may reopen an earlier stage.

### Review method for every page

For each page:

1. read it in normal navigation order;
2. classify each claim as a 1.0 guarantee, implementation detail, design
   rationale, example, or known limitation;
3. run every command and example against the pinned 1.0 build;
4. confirm that accepted examples succeed and rejected examples fail for the
   documented reason;
5. record **confirm**, **revise**, **defer**, or **needs Zach review**;
6. add or link automated evidence for behavior that should not regress; and
7. move the page from `work-in-progress` to `review-needed`, then to
   `publication-ready` only after its questions are resolved.

An audit record should contain the page, compiler revision, documentation
version, reviewer, date, commands run, decision, open questions, and linked
issues or design decisions.

### Stage 0 — approve the documentation experience

Before confirming individual language claims, review the documentation site as
a product. Check the overall visual design, typography, color palettes,
responsive behavior, navigation, search, version selector, status markers,
page banners, code presentation, and accessibility. Review representative pages
on desktop and mobile rather than judging the home page alone.

Confirm the information architecture at the same time: decide whether the
selected top-level sections and individual pages belong on the site, whether
anything important is missing or unnecessarily separate, and whether each
page's intended content and depth are appropriate. Use representative overview,
tutorial, reference, example, implementation, and contributor pages to approve
the recurring page structure before substantial polishing makes it expensive
to change.

Record approved site-wide conventions and page-selection decisions in the
documentation workflow or an appropriate design decision. Treat unresolved
visual, structural, navigation, or content-scope concerns as blockers for the
later page-by-page audit.

**Approval question:** do you like and agree with the site's general design,
formatting, navigation, page selection, and intended content well enough to use
them as the foundation for the remaining documentation audit?

### Stage 1 — orientation and the supported workflow

Review Home, Getting started, installation, the first program, command-line
usage, and editor support. Confirm that a clean user can install prerequisites,
build Sagan, obtain the documented version, install the compatible VS Code
extension, open a `.sagan` file with the expected language mode, compile or run
the first supported program, and understand what is or is not supported.

**Approval question:** does this present the language you want a new user to
encounter first?

### Stage 2 — source text and lexical rules

Review source encoding, identifiers, Unicode and emoji behavior, whitespace,
comments, documentation comments, literals, interpolation, keywords,
punctuation, and operators. Exercise boundary and malformed-input examples.
For each settled lexical form, compare the compiler's behavior with the VS Code
extension's highlighting and editing behavior; record intentional differences
and treat accidental drift as a defect.

**Approval question:** are the spellings and source-level rules stable and
pleasant enough to guarantee for 1.0?

### Stage 3 — fundamental values and expressions

Review scalar values, variables, mutability, assignment, precedence,
evaluation order, calls, member access, safe access, indexing, collections,
points, vectors, Cartesian/spherical representation, and basic conversions.
Keep the geometry model visible for final expert verification: points are affine
locations, vectors are displacements, point/vector conversion requires an
origin rather than a cast, and Cartesian/spherical conversion is explicit.

**Approval question:** do ordinary expressions read and behave the way you
expect before advanced typing is involved?

### Stage 4 — declarations, functions, and control flow

Review variable and function declarations, parameters, returns, lambdas,
conditionals, loops, matching fundamentals, `yield`, scope, definite
initialization, and unreachable code. Confirm both syntax and observable
execution.

**Approval question:** can straightforward procedural Sagan be explained
without exceptions, caveats, or hidden execution rules?

### Stage 5 — modules, programs, diagnostics, and tools

Review modules, imports, exports, aliases, namespaces, entry points, separate
files, compiler modes, diagnostics, generated C++, native execution, and editor
features. Audit every claimed extension capability, its settings, packaging,
installation paths, upgrade behavior, compatibility declaration, and known
limitations. Confirm that snippets or sample files use current 1.0 syntax and
that visual highlighting is never described as parsing or semantic validation.
Confirm what forms a program and how users understand failures.

**Approval question:** is the supported build-and-run model coherent for a
real multi-file project?

### Stage 6 — named data and nominal types

Review enums, payload-bearing cases, optionals, collections, pattern matching,
exhaustiveness, classes, construction, fields, methods, privacy, and `self`.
Defer composition conflicts, generic variance, and ownership consequences to
later stages.

**Approval question:** do the primary data-modeling tools cover ordinary user
needs with predictable behavior?

### Stage 7 — type system and generic programming

Review inference, checking, lossless conversions, overloads, function types,
generic functions, generic classes and faces, specialization, constraints, and
diagnostics. Test inference boundaries and cases requiring explicit spelling.

**Approval question:** is the type system strict in the intended places without
making common programs needlessly verbose?

### Stage 8 — interfaces and composition

Review `face`, `is`, `has`, transitive conformance, defaults, conflict
resolution, dispatch, visibility, and the boundary between composition and any
form of inheritance. Use examples with multiple interacting faces rather than
only isolated declarations.

**Approval question:** does interface-based composition remain understandable
and predictable when several abstractions interact?

### Stage 9 — references, lifetime, and absence

Review strong and weak references, reference counting, face-typed values,
optional weak reads, safe access, coalescing, cycles, destruction, and escaping
closures. Confirm which lifetime properties are guarantees and which remain
programmer responsibilities.

**Approval question:** can users reason about object lifetime and absence
without relying on undocumented C++ behavior?

### Stage 10 — exceptions and non-local control transfer

Review `hope`, `unless`, `finally`, `scream`, propagation, matching, cleanup,
returns through cleanup, native runtime failures, and interactions with
resources and reference lifetimes.

**Approval question:** are exceptional paths as precisely specified and tested
as normal paths?

### Stage 11 — numerical and simulation guarantees

Review checked arithmetic, overflow, floating-point behavior, dimensions,
points versus vectors, Cartesian versus spherical representation,
deterministic execution, platform differences,
math availability, and the boundaries of physics and rendering. Confirm exact
guarantees rather than aspirations.

**Approval question:** are Sagan's simulation-focused promises both useful and
honest about their limits?

### Stage 12 — final expert review with Zach

Reserve the most coupled or controversial subjects for a focused review after
the preceding behavior is concrete:

- point and vector separation, arithmetic, conversions, and generic use;
- spherical conventions, Cartesian conversion, and reference-frame policy;
- interface composition, default conflicts, conformance, dispatch, and whether
  any implementation inheritance belongs in Sagan;
- generic constraints, method-specific generics, explicit type arguments, and
  any variance rules;
- reference-cycle policy, weak edges, destruction, and escaping closures;
- exception matching, native runtime errors, cleanup guarantees, and the
  boundary between recoverable and fatal failures;
- deterministic-simulation guarantees across platforms and compiler versions;
- module/package boundaries and initialization;
- the built-in math boundary and the public architecture of physics and
  rendering; and
- Schematic history, inherited design or implementation, attribution, and
  GPLv3 provenance.

For each topic, prepare a short review packet containing the current rule, two
or three representative programs, implementation evidence, alternatives that
were rejected, unresolved questions, and the exact decision requested.

**Exit condition:** each topic has an accepted decision or an explicit deferral
that accurately limits the 1.0 documentation. Zach's review should be recorded
in the relevant decision, RFC, or attribution page rather than existing only in
meeting notes.

## Completion definition

The documentation is ready to move from experimental review toward public release
when:

- every navigable public page has a recorded audit result;
- all commands and examples pass against the pinned 1.0 release;
- the documented VS Code extension version passes its feature checklist against
  representative 1.0 source and is explicitly tested for grammar drift;
- normative rules have tests and no unresolved contradictions;
- deferred behavior is clearly outside the 1.0 contract;
- expert-review decisions are recorded and reflected everywhere they apply;
- every public page is verified for the same documentation version; and
- the strict site build and documentation release check pass.
