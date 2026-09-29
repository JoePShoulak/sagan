---
title: Documentation status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation status
## Roadmap snapshot

| Area | Status | Evidence |
| --- | --- | --- |
| Language direction | **Settled enough for early work** | README design and lexical rules |
| Token vocabulary | **Implemented** | `tokens.hpp`, `tokens.cpp` |
| Tokenizer | **Complete for the current lexical specification** | Unicode-aware lexer, comprehensive self-tests, examples |
| Parser and Sagan AST | **Complete for the current syntax specification** | modules, imports, exports, declarations, functions, types, composition, expressions, collections, control flow, matching, exceptions, documentation, AST renderers, parser demos |
| Semantic analysis | **Executable-subset foundation implemented** | scopes, names, types, lossless widening, generic call inference, generic sum construction, payload-bearing nominal enums and exhaustive matches, private class state, constructors, face-typed values and `self`, transitive conformance/defaults, collections, dimensions, definite initialization/returns, unreachable code, entry points, demos |
| Runtime and memory model | **Reference ownership model implemented** | shared reference-counted class/face values, dynamic dispatch, explicit `weak let` fields, compile-time rejection of all-strong declaration cycles and strong face fields, optional weak reads, payload matching, safe `?.`, and lazy `??` |
| Standard/core libraries | **Model settled; APIs open** | math is automatic; physics and rendering are explicit first-party imports |
| C++ code generation and execution | **Initial executable subset implemented** | direct `sagan file.sagan`, `--run-package`, temporary native builds, exit propagation, `--emit-cpp`, reference-counted classes/faces, exceptions, lambdas, checked arithmetic, collections, dimensions, control flow, demos |
| Deterministic execution | **Catchable runtime-error foundation implemented** | checked arithmetic and collection lookup failures become nominal `RuntimeError` values, with focused fixtures and a native demo |
| Module and package resolution | **Executable package foundation implemented** | strict manifests, qualified modules mapped to nested files, package-root containment, loose-module compatibility, declaration/export validation, namespaces, aliases, ordering, cycle diagnostics, native package demo |

## Tokenizer verification

The tokenizer build verifies Unicode 17 XID and emoji identifiers, malformed
UTF-8 rejection, NFC normalization, every current keyword and operator,
focused error cases, and randomized byte-input robustness. Interpretation of
newlines inside ambiguous `<...>` and `{...}` constructs belongs to the parser
and is not unfinished tokenizer behavior.

## What can run today

The `bin/sagan` compiler can directly compile and run supported source files or
manifest-backed packages. It can also print tokens, parse the current grammar, emit text,
DOT, SVG, or interactive HTML ASTs, and print the initial semantic model. The
HTML renderer supports zooming and panning. Parser and semantic demonstrations
provide broad successful source files plus focused malformed examples. The
semantic mode validates names and scopes. Type mode additionally validates the
implemented scalar and function rules. The initial backend can emit C++ for a
validated scalar/function/control-flow subset, and the execution demo compiles
that output with `g++` and runs it.
Entry mode performs the complete implemented checks and requires exactly one
parameterless `main` returning `Int` or `Void`.

The live Codecov report tracks tokenizer lifecycle behavior, Unicode and emoji
edge cases, string escapes, malformed input, parser and semantic errors, all AST
renderers, CLI behavior, and defensive invariants.

## Parser verification

The parser currently verifies `let` declarations, core primary expressions,
the settled operator-precedence table, chained calls/indexing/member access,
ordinary and safe access, mutating method calls, ordinary/raw/multiline/
interpolated strings, and array/dictionary/vector/coordinate expressions with
spreads and trailing commas. Text, DOT, SVG, and interactive HTML tree renderers
cover every implemented AST node. Blocks, same-line single-statement bodies,
ordinary and compound assignment, expression statements, and
`if`/`else if`/`else` control flow are also verified. The parser
enforces declaration-only program roots and provides named, typed, block-bodied
functions as statement containers. `for`/`in`, `while`, and `until` loops,
unlabeled `break` and `continue`, and bare or value-bearing `return` statements
are implemented and rendered in every AST format. Braced or same-line
single-statement `match`/`case`
supports expression-shaped patterns plus a unique final `case else`; pattern
meaning and exhaustiveness remain semantic work. The parser and execution demos include
`hope`/`unless`/`finally` and value-bearing `scream`. Exact type-and-value
handlers execute in source order, unmatched values propagate, and cleanup runs
across normal completion, exception paths, and early returns. Successful source plus focused
expression, collection, control-flow, matching, exception, and
unterminated-block errors are demonstrated. Classes now execute with typed
typed fields, checked `new(...)` constructors, default construction, `self`, private field access/mutation, and
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
Typed expression lambdas are callable, may capture local lexical state, and can
be stored in local variables or invoked immediately. Escaping closures and
function-type annotations remain future runtime and type-system work.
Module declarations, import sources and aliases, and standalone exports are
parsed and rendered. The loader resolves loose sibling files or manifest-backed packages transitively,
validates module names and public exports, preserves aliases, and rejects
cycles. Selective imports and exported namespace members are isolated, linked,
semantically analyzed, type-checked, and emitted together for native
cross-module calls. Qualified names map to nested files beneath the manifest
source root. Mutable module initialization, external dependencies, constraints,
lockfiles, registries, and distribution remain future work.
Documentation comments attach to supported declarations with retained text and
source spans and appear in every AST renderer. Focused errors cover orphaned,
same-line, executable-statement, and enum-member placements.
Bare and value-bearing `yield` statements and documented enum members complete
the current parser grammar. Enum cases may carry typed payloads, construct
values, bind payload names in `match`, and establish exhaustiveness. Every case
has a unique signed 64-bit tag, with zero-based implicit sequencing and explicit
assignments that reset the following sequence.

## Major open language questions

Generic sum enums execute with contextual or explicitly qualified type
arguments, and top-level generic functions execute with call-site inference.
Generic classes, generic face defaults, and class-level generic methods execute
with specialization and inference. Explicit function, method, and constructor
type arguments execute, and function/class parameters can require structural
face conformance with `is`. Method-specific face generics, external package
dependencies and distribution, and the concrete math, physics, and rendering APIs
remain unresolved. Math's automatic availability and the explicit-import
status of the first-party physics and rendering libraries are settled.

## Required pre-1.0 design checkpoints

Before declaring the hypercore language stable, reevaluate whether coordinates
should remain a distinct type from vectors. The decision must explicitly cover
their mathematical meaning, valid arithmetic (`coordinate - coordinate`,
`coordinate + vector`, and invalid `coordinate + coordinate`), component access,
conversion rules, generic algorithms, runtime representation, and whether type
separation prevents meaningful simulation errors without creating unnecessary
friction. This review must happen before work begins on math, rendering, or
physics libraries.

The 1.0 release must also provide professional graphical installers for Windows,
macOS, and Linux, plus platform-appropriate command-line installation. The
installed toolchain must place `sagan` on the user's command path, support
`sagan file.sagan`, provide normal install-location, permission, progress, and
uninstall behavior, and register `.sagan` files for native double-click launch.
Whether that launch opens a visible terminal or uses another execution surface
is intentionally deferred until installer design begins. Release artifacts will
be published through GitHub's release/package facilities; HP1 may additionally
host or mirror installer and package data where convenient.

After 1.0 is declared complete, but before the math, rendering, and physics
libraries begin, hold a dedicated lifecycle and release-operations review. It
must settle release cadence and channels, the events that trigger builds,
testing, signing, publication, documentation promotion, rollback, security and
support work, and maintenance responsibilities outside day-to-day language
development. Full-functionality VS Code extension work is planned for the same
transition and awaits its separate requirements.
