---
title: Documentation status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Project status

This page is the technical progress map. It distinguishes behavior that works
today from planned libraries and external release gates. The `work-in-progress`
metadata at the top means the prose still awaits the owner's documentation
audit; it does not mean every listed compiler feature is unfinished.

Current development commits are preparation for a future 2.0 release. Their
1.x commit versions are checkpoints, not a declaration that the 2.0 migration
or release gates are complete. The major-version commit is reserved for the
coordinated 2.0 publication decision.

The current units work also preserves compound-denominator grouping and shows
the selected unit in native `print` output and string interpolation. Named
derived units such as `newton` and `watt` retain their catalog dimensions; the
[units reference](../reference/units.md) includes executable examples.

## Roadmap snapshot

| Area | Status | Evidence |
| --- | --- | --- |
| Language direction | **1.0 hypercore defined** | README, reference, and executable tests |
| Token vocabulary | **Implemented** | `tokens.hpp`, `tokens.cpp` |
| Tokenizer | **Complete for the current lexical specification** | Unicode-aware lexer, comprehensive self-tests, examples |
| Parser and Sagan AST | **Complete for the current syntax specification** | modules, imports, exports, declarations, functions, types, composition, expressions, collections, control flow, matching, exceptions, documentation, AST renderers, parser demos |
| Semantic analysis | **Hypercore rules implemented** | scopes, names, types, lossless widening, generics, payload enums, exhaustive matches, classes, faces, collections, units, control flow, and entry points |
| Runtime and memory model | **Reference ownership model implemented** | shared reference-counted class/face values, dynamic dispatch, explicit `weak let` fields, compile-time rejection of all-strong declaration cycles and strong face fields, optional weak reads, payload matching, safe `?.`, and lazy `??` |
| Standard/core libraries | **Model settled; APIs open** | math is automatic; physics and rendering are explicit first-party imports |
| C++ code generation and execution | **Hypercore backend implemented** | direct `sagan file.sagan`, packages, classes/faces, exceptions, lambdas, checked arithmetic, collections, units, control flow, and demos |
| Native units of measure | **Implemented** | static dimensions/quantities/units, affine temperatures, scientific catalog and SI prefixes, callable constraints, custom declarations, erased native representation, `make units-demo` |
| Explicit constants | **Implemented as immutable bindings** | `const` plus ASCII SCREAMING_SNAKE_CASE, const-view mutation checks, class fields with declaration initializers, native execution and editor grammar tests; general compile-time evaluation deferred |
| Deterministic execution | **1.0 hypercore contract settled** | exact hypercore operations and runtime failures are deterministic; floating/toolchain/host boundaries are explicitly excluded |
| Module and package resolution | **Executable package foundation implemented** | strict manifests, qualified modules mapped to nested files, package-root containment, loose-module compatibility, declaration/export validation, namespaces, aliases, ordering, cycle diagnostics, native package demo |
| Editor tooling | **Usable language server and VS Code client; advanced support in progress** | reusable compiler library, source identity/UTF-16 positions, recovery, overlays, semantic queries, recovered-source formatting, safe edits, stdio LSP, cancellable check/build/run operations, document/project test execution, installed package imports, and experimental DAP; full package completion and debugger release support remain gated |
| Unified Sagan errors | **Deferred; not complete** | Keep the agreed terminal/editor presentation, stable per-error codes, multi-error recovery, Sagan call-site tracebacks, safe hints, and native-failure mapping on the roadmap; see [diagnostics](../tooling/diagnostics.md) and [language-service Phase 1](../tooling/language-service-roadmap.md#phase-1-source-identity-and-structured-diagnostics) |

Every release is a coordinated ecosystem freeze: compiler/language, included
libraries, documentation, and extension must agree and pass together. See the
[ecosystem release checklist](../contributing/ecosystem-release-readiness.md).

## Tokenizer verification

The tokenizer build verifies Unicode 17 XID and emoji identifiers, malformed
UTF-8 rejection, NFC normalization, every current keyword and operator,
focused error cases, and randomized byte-input robustness. Interpretation of
newlines inside ambiguous `<...>` and `{...}` constructs belongs to the parser
and is not unfinished tokenizer behavior.

## What can run today

The `bin/sagan` compiler can directly compile and run supported source files or
manifest-backed packages. It can also print tokens, parse the current grammar, emit text,
DOT, SVG, or interactive HTML ASTs, and print the semantic model. The
HTML renderer supports zooming and panning. Parser and semantic demonstrations
provide broad successful source files plus focused malformed examples. The
semantic mode validates names and scopes. Type mode additionally validates the
implemented type rules. The backend emits C++ for checked programs, and the
execution demo compiles that output with `g++` and runs it.
Entry mode performs the complete implemented checks. The root file can execute
top-level statements; normal completion exits with status 0, and `exit(code)`
sets an explicit status. A function named `main` is ordinary code.

The live Codecov report tracks tokenizer lifecycle behavior, Unicode and emoji
edge cases, string escapes, malformed input, parser and semantic errors, all AST
renderers, CLI behavior, and defensive invariants.

## Parser verification

The parser currently verifies `let` declarations, core primary expressions,
the settled operator-precedence table, chained calls/indexing/member access,
ordinary and safe access, mutating method calls, ordinary/raw/multiline/
interpolated strings, and array/dictionary/Cartesian-vector/point expressions
with spreads, plus exactly three-dimensional spherical-vector/point literals.
Spherical literals do not accept spreads. All collection forms accept trailing
commas. Text, DOT, SVG, and interactive HTML tree renderers
cover every implemented AST node. Blocks, same-line single-statement bodies,
ordinary and compound assignment, expression statements, and
`if`/`else if`/`else` control flow are also verified. The parser
accepts executable root-file statements while imported modules remain
declaration-only. `for`/`in`, `while`, and `until` loops,
unlabeled `break` and `continue`, and bare or value-bearing `return` statements
are implemented and rendered in every AST format. Braced or same-line
single-statement `match`/`case`
supports expression-shaped patterns plus a unique final `case else`; pattern
meaning and exhaustiveness remain semantic work. The parser and execution demos include
`hope`/`unless`/`finally` and value-bearing `scream`. Exact type-and-value
handlers execute in source order, unmatched values propagate, and cleanup runs
across normal completion, exception paths, and early returns. Successful source plus focused
expression, collection, control-flow, matching, exception, and
unterminated-block errors are demonstrated. Classes execute with typed fields,
checked `new(...)` constructors, default construction, `self`, private field access/mutation, and
ordinary or `!`-suffixed methods. `is` and `has` composition require every face
signature to have an exact class implementation or an unambiguous default. Simple
nominal enums now execute with `Type.member` selection, equality, matching,
interpolation, and readable printing. Face defaults execute, may call other face
requirements through `self`, and require an explicit class override when two
composed faces provide the same signature. Face requirements and defaults flow
through composed faces, and cycles are rejected. Face-typed values accept only
classes with declared transitive conformance and dispatch through shared
reference-counted objects. Optional values, payload matching, safe access, and
lazy coalescing provide the absence model used by `weak let` class fields.
Weak fields begin empty, accept strong class or face values on assignment, and
read as `Optional<T>` so expired targets become `None`. The checker rejects
all-strong declaration cycles, including nested optional/generic references,
and requires face-typed fields to be weak because their concrete targets are
dynamic. Every permitted ownership cycle therefore has an explicit weak edge
and can be reclaimed without a tracing collector. Named functions
and methods accept block or `=>` expression bodies.
Function types use `(Parameter, ...) => Result`. Typed expression lambdas may
be stored, passed, returned, or invoked immediately, and keep captured local
variables and parameters alive in shared reference-counted cells. Closure
copies share captured mutation. Contextual `self` capture remains deferred.
Lambdas that attempt that capture are rejected explicitly in 1.0.
Module declarations, import sources and aliases, and standalone exports are
parsed and rendered. The loader resolves loose sibling files or manifest-backed packages transitively,
validates module names and public exports, preserves aliases, and rejects
cycles. Selective imports and exported namespace members are isolated, linked,
semantically analyzed, type-checked, and emitted together for native
cross-module calls. Qualified names map to nested files beneath the manifest
source root. Installed external dependencies now resolve through manifest
aliases, a local package index, and validated exact `sagan.lock` pins.
Resolution is offline; package installation commands, a remote registry,
mutable module initialization, and distribution remain future work.
Documentation comments attach to supported declarations with retained text and
source spans and appear in every AST renderer. Focused errors cover orphaned,
same-line, executable-statement, and enum-member placements.
Bare and value-bearing `yield` statements and documented enum members complete
the current parser grammar. Enum cases may carry typed payloads, construct
values, bind payload names in `match`, and establish exhaustiveness. Every case
has a unique signed 64-bit tag, with zero-based implicit sequencing and explicit
assignments that reset the following sequence.

## Deliberately deferred language work

Generic sum enums execute with contextual or explicitly qualified type
arguments, and top-level generic functions execute with call-site inference.
Generic classes, generic face defaults, and class-level generic methods execute
with specialization and inference. Explicit function, method, and constructor
type arguments execute, and function/class parameters can require structural
face conformance with `is`. Method-specific face generics, package installation
and distribution, and the concrete math, physics, and rendering APIs remain
post-1.0 work. Implementation inheritance, parallelism, unsafe escape
hatches, registries, and remote dependency retrieval are also explicitly
deferred. Exact lockfiles and offline installed-package resolution are now
implemented. Math's automatic availability and the explicit-import
status of the first-party physics and rendering libraries are settled.

## Required pre-1.0 design checkpoints

The geometry checkpoint is complete: locations are named `Point` and remain
affine values distinct from displacement `Vector` values. Point-vector
translation and point subtraction are implemented; point addition and
context-free point/vector conversion are rejected. Cartesian values use ordinary
`(...)` and `<...>` literals. Exactly three-dimensional spherical points and
vectors use adjacent `s(...)` and `s<...>` literals, radians, and named radial/
angular components. Spherical arithmetic and Cartesian conversion are reserved
for the first core math geometry API after 1.0. All four families have fixed-size
component storage, frame tracking remains deferred, and future generic APIs must
preserve the semantic and representation distinctions.

The pre-1.0 units checkpoint is complete. Units are native compile-time metadata,
not optional interfaces: dimensions, named quantities, concrete scales, affine
points/differences, exact conversions, custom declarations, SI prefixes, and a
scientific catalog are implemented. Unit annotations apply to locals, fields,
function/method/constructor/lambda parameters and results, generic arguments,
and face signatures. Generated values retain only their ordinary numeric or
geometry representation. Dynamic/logarithmic units, uncertainty, fractional
dimensions, external catalogs, and coordinate frames remain deferred.

The hypercore determinism checkpoint is complete. Sagan guarantees reproducible
language results for exact integer/boolean/string/enum/optional operations,
source-ordered control flow and collections, deterministic module resolution,
and reference-counted identity/capture behavior. Floating-point bit identity,
host/toolchain output, external I/O, concurrency, and future library numerics
are outside this initial contract; see [Determinism](determinism.md).

Windows is the initial supported installation platform; macOS and Linux remain
tracked future targets. The Windows installer foundation now bundles the UCRT64
compiler used by the C++ backend, places `sagan` on PATH, supplies location,
permission, progress, Start-menu, file-association, and uninstall behavior, and
is built and smoke-tested in CI. Both installed entry-point executables
statically link their GCC/C++ startup runtimes, and the smoke test removes
compiler runtime directories from `PATH` before exercising the CLI, generated
program, and Explorer launcher. An optional `[application] mode` selects
`console` or `windowed` Explorer launch, loose files default to `console`, and
context-menu verbs override either choice. Windowed failures retain a local log
and show a native diagnostic dialog. Published release artifacts use GitHub
Releases as the canonical source; HP1 provides a verified mirror.

The L1 window-distribution work packages the explicit physics/render libraries,
render native bridge, and two-body demo with Windows releases. A normal
`sagan --run-package` call and the GUI launcher use the resolved package rather
than a demo-specific compiler command. Headless projects still omit the render
bridge. The [two-body roadmap](../standard-library/two-body-window-roadmap.md)
records staged-payload verification and remaining manual Explorer validation.

Installer policy is Windows x64 only, a self-contained offline package,
in-place upgrades, refused downgrades, and SHA-256 sidecars. Initial 1.0.0
publication explicitly permits an unsigned installer after automated
isolated-environment testing and owner approval. Authenticode signing and a
clean Windows x64 computer or VM run become post-publication acceptance work for
a later patch release.

The pre-1.0 lifecycle review is complete. Every push to `main` now runs
release-preparation CI. A new 0.x version receives a signed `-rc.1` preview
tag, while a new 1.x version receives a signed stable tag. A no-version-change
commit runs CI without duplicating a release. Previews publish after automated gates, while stable
artifacts remain in a draft until the project owner approves the protected
publication environment. GitHub Releases is canonical and HP1 is a
non-blocking verified mirror. Published artifacts and numbered documentation
are immutable, and only the latest stable release receives best-effort support.
The initial 1.0.0 documentation stays experimental; its page-by-page audit is
the **first post-1.0 task** and will determine whether any language, tooling,
or documentation changes are needed. Authenticode signing and a clean-machine
run are subsequent Windows acceptance work. The VS Code 0.3.5 language client
is usable today; matching extension tests and compiler capabilities are part
of every release gate.
