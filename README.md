<p align="center">
  <img src="docs/assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="280">
</p>

# Sagan

[![Development version 0.71.6](https://img.shields.io/badge/development-0.71.6-2563eb)](docs/contributing/versioning.md)
[![Documentation](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml/badge.svg)](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml)
[![Codecov](https://codecov.io/gh/JoePShoulak/sagan/graph/badge.svg)](https://codecov.io/gh/JoePShoulak/sagan)

Sagan (**Simulation Architecture for Geometry, Astrodynamics, and Numerics**) is
an experimental, strongly typed programming language for scientific and
real-time simulation. It is designed to make geometry, orbital mechanics,
physics, rendering, and multi-entity systems natural to express without giving
up explicit types or predictable behavior.

> [!WARNING]
> Sagan is under active language and compiler development. It is not ready for
> general use, and syntax or semantics may change.

## Current status

The tokenizer and parser are complete for the current lexical and syntax
specifications. The semantic front end now resolves names, infers core scalar
types, checks calls and returns, validates core operators and conditions, and
enforces lossless numeric widening. It also validates type annotations,
all-path returns, definite initialization, unreachable code, and executable
entry points. Sagan can print semantic and type models as
well as text, DOT, SVG, and interactive HTML syntax trees. Homogeneous arrays
and dictionaries plus dimensioned Cartesian and spherical vectors and points
are inferred. Nominal
enums can be selected, compared, matched, interpolated, and printed. Cases may
carry typed payloads, construct values like `Success(42)`, and bind their
contents through exhaustive `match` branches. Every case also has a unique
signed 64-bit numeric tag. Tags begin at zero and increment implicitly, while
`Case = 200` sets an explicit tag and resets the continuation sequence; payload
cases use the same syntax.
Class methods declared with a leading dot are private to their declaring class.
Top-level generic functions such as `identity<T>(value: T): T` infer type
arguments from their calls and execute natively, or accept explicit arguments
such as `identity<String>("signal")`. Generic sum enums support both
contextual construction and explicit qualification such as
`Result<Int, String>.Failure("problem")`. Generic classes infer their type
arguments from constructors—or accept `Box<Int>(42)` explicitly—preserve them through fields and methods, and may
conform to specialized generic faces such as `Readable<T>`. Generic face
defaults and class-level generic methods execute with call-site inference or
explicit calls such as `box.echo<String>("signal")`. Function and class type
parameters may use face constraints such as `T is Readable<Int>`; every concrete
type argument is checked for conformance. Method-specific generics on faces,
the full runtime, and the standard library are not yet implemented.
The built-in `Optional<T>` type,
`Some(value)`, `None`, payload matching, safe `?.` access, and lazy `??`
fallback execute natively. An initial C++ emitter can compile the validated scalar/control-
flow subset into a native executable, including visible output through the
built-in `print(value)` function. The executable subset also supports typed
array literals and spreads, checked indexing, and `for … in` iteration.
Interpolated strings and mutable `while`/`until` loops also execute in the
current native subset.
Homogeneous dictionary literals, left-to-right spreads, and checked key lookup
are executable as well.
Expression-pattern `match` statements execute with ordered cases and a fallback.
Control-flow bodies may use braces or a single statement on the same line.
Prefix and postfix numeric increment and decrement expressions also execute.
Integer arithmetic is checked: overflow, division by zero, and modulo by zero
raise catchable `RuntimeError` values instead of inheriting undefined native
behavior. Collection bounds and missing dictionary keys use the same nominal
error model.
Dimensioned vectors and points execute as distinct runtime values with
indexing, iteration, printing, and interpolation. Cartesian values use
`<x, y, z>` vectors and `(x, y, z)` points; exactly three-dimensional spherical
values use `s<magnitude, inclination, azimuth>` vectors and
`s(radius, inclination, azimuth)` points, with angles measured in radians.
The `s` prefix must be adjacent, so `s (...)` remains an ordinary call.
Cartesian vectors and points also support same-family spread construction.
Vectors additionally support checked addition, subtraction, negation, scalar
multiplication/division, equality, and corresponding compound assignments.
Points are affine locations rather than interchangeable tuples: adding or
subtracting a vector translates a point, while subtracting two points produces
their displacement vector. Adding points is a type error.
Cartesian values expose dimension-checked `.x`, `.y`, `.z`, and `.w`
components for reading and mutation when that component exists.
Spherical points expose `.radius`, `.inclination`, and `.azimuth`; spherical
vectors expose `.magnitude`, `.inclination`, and `.azimuth`. Spherical
arithmetic and Cartesian conversion are intentionally deferred to the core math
API so their conventions are explicit rather than silently inferred.
Function types use `(Parameter, ...) => Result`. Typed expression lambdas can
be stored, passed, returned, or invoked immediately. Captured local variables
and parameters use shared reference-counted cells, so escaping closures remain
valid and copies observe the same mutations. Run `make closure-demo` for the
executable source and output.
Classes support typed fields, overloaded `new(...)` constructors, default
zero-argument construction when every field has a default, `self`,
field mutation, ordinary methods, and `!`-suffixed mutating methods.
Leading-dot fields and methods are private to their declaring class. Class and
face values use shared reference-counted storage in the native subset. Explicit
`weak let` class fields break ownership cycles; assignment accepts a strong
class or face value, while reads produce `Optional<T>` and become `None` after
the target expires. Declaration-level all-strong cycles are rejected, including
reference types nested inside optionals and generic type annotations. Face-typed fields must
be weak because their concrete target cannot be proven acyclic. A conforming
class converts to a composed face for runtime method dispatch.
Classes declaring `is` or `has` a face are checked structurally for every
required method and exact signature; neither spelling creates inheritance.
Unambiguous face defaults are composed into the class, class methods override
them, and conflicting defaults require an explicit class override.
Faces may compose other faces transitively; inherited requirements and defaults
flow through the chain, while cyclic composition is rejected.
Value-bearing `scream` exceptions execute in the native subset. `unless`
handlers test exact type-and-value matches in source order, unmatched values
propagate outward, and `finally` cleanup runs during normal completion,
propagation, handled exceptions, and early returns. Native failures can be
selected with cases such as `RuntimeError.integer_overflow` and
`RuntimeError.index_out_of_bounds`.
The module pipeline supports loose sibling modules and manifest-backed
packages. A strict `sagan.toml` selects the package name, semantic version,
source root, and entry module. Qualified names such as `navigation.guidance`
map deterministically to nested source files. Resolution validates declarations
and public exports, applies aliases, isolates private symbols, orders
dependencies, and rejects cycles. Selective imports and whole-module namespaces
participate in semantic/type checking and native execution.
`^` and `^=` perform checked mathematical exponentiation rather than bitwise XOR.
Native units of measure are part of the static type system. Numeric, vector,
and point values accept linear or affine units; compatible assignments,
arguments, returns, and explicit `as` conversions emit native scale/offset
operations without storing unit metadata per value. Functions, methods,
constructors, lambdas, fields, locals, generics, and face signatures can require
specific units. Custom dimensions, quantities, base/derived units, aliases,
prefixes, and affine units are supported with duplicate, consistency, and cycle
checks. The built-in scientific catalog covers SI base and named derived units,
all current SI prefixes, common time/angle/astronomical units, and selected
laboratory and customary units.

```text
UTF-8 source -> tokenizer -> parser -> AST -> scope/name analysis -> type checking
                                  |                    |                    |
                                  |                    +-> semantic model   +-> type model
                                  +-> text / DOT / SVG / HTML

Initial subset: typed program -> C++ generation -> native executable
```

The detailed and continuously maintained status lives in the
[documentation status](docs/design/status.md) and
[implementation overview](docs/implementation/index.md).
See the live Codecov badge and [testing](docs/contributing/testing.md) for the
currently measured compiler, semantic-analysis, and CLI coverage.

## Design direction

- simulation-focused mathematical and geometric programming;
- strong static typing with only provably lossless implicit conversions;
- interface-based composition in preference to inheritance hierarchies;
- mutable variables by default, with trailing `!` naming mutating methods;
- reference-counted memory management;
- native compilation through an initial C++ backend; and
- a deterministic exact-value hypercore contract, with floating-point and
  future library guarantees specified separately.

The [design philosophy](docs/design/philosophy.md),
[goals and non-goals](docs/design/goals-and-non-goals.md), and
[language reference](docs/reference/index.md) distinguish settled direction
from provisional and unresolved behavior.

The pre-1.0 geometry review names locations `Point` and keeps them distinct from
displacement `Vector` values. Cartesian and spherical forms have equally direct
literal syntax, while reference-frame tracking remains deferred. This prevents
invalid point arithmetic and prepares transform semantics without adding runtime
cost. Cartesian/spherical conversion will be the first geometry contract taken
up with core math after 1.0. Math, rendering, and physics
library work remains paused until the hypercore and installation work are complete.
The pre-1.0 unit checkpoint is implemented as a native static type-system
feature rather than an interface convention. It tracks dimensions, named
quantities, concrete units, and affine point/difference categories through
arithmetic and call boundaries, while erasing that metadata from runtime
values. See the [unit design](docs/design/units-of-measure.md) and
[built-in catalog](docs/reference/units.md).

Windows is the initial supported installation platform. The Windows installer
foundation bundles the UCRT64 backend toolchain, configures PATH, supplies
normal location/permission/progress/uninstall behavior, and registers `.sagan`
files. The compiler and Explorer launcher statically link the GCC/C++ support
needed to start, so installed commands do not depend on MSYS2 runtime DLLs being
present on the user's PATH. Packages choose `console` or `windowed` Explorer behavior through an
optional `[application]` manifest section; loose files default to `console`, and
Explorer supplies per-launch overrides. macOS and Linux remain explicit future
targets rather than claimed current support. Release artifacts
should use GitHub Releases and package facilities, with HP1 available as a
self-hosted distribution or mirror when useful. Before 1.0 is declared or
published, the project will hold a dedicated release-lifecycle review covering
release cadence, channels, automation triggers, signing, publishing, support,
rollback, and maintenance. The resulting policy governs the 1.0 release itself;
1.0 is not complete merely because the implementation is feature-complete.
Full VS Code extension work is paused and is not a 1.0 release gate.

The 1.0 installer contract is Windows x64, self-contained and offline, with
in-place upgrades, downgrade refusal, checksums, and Authenticode signing for
public releases. Automated isolated-path tests cover implementation; the exact
signed release candidate must also pass a clean Windows x64 machine or VM before
publication.

The release lifecycle is now defined: verified signed tags on `main` create
immutable signed preview releases or stable drafts, stable publication requires
the clean-machine record and project-owner approval, and GitHub Releases is
canonical with a non-blocking HP1 mirror. Each Windows release includes the
installer plus a non-mutating portable CLI ZIP, checksums, a release manifest,
an SBOM, and reviewed notes. Support is best effort for the latest stable only.

The compiler-owned editor-tooling track has begun without changing the held VS
Code extension. The compiler builds as a reusable static library and exposes
versioned `sagan.language-service/1` contracts for document identity, UTF-8
byte and UTF-16 positions, structured diagnostics, cancellation, strict and
recovering document analysis, JSON diagnostics, and capability discovery. Its
lossless syntax representation retains comments and whitespace, and bounded
top-level recovery returns partial trees and multiple diagnostics for
incomplete editor input. A thread-safe document store now handles versioned
open/change/save/close overlays, and module imports resolve unsaved buffers
before disk files. Workspace analysis caches current results, invalidates
transitive dependents, cancels obsolete requests, and rejects stale
publication. Semantic queries, formatting, refactoring, and LSP transport
remain capability-disabled until their roadmap phases land. The semantic layer
now assigns deterministic opaque identities to source, imported, built-in, and
generated symbols; distinguishes typed symbol kinds, visibility, and origin;
and exposes reusable document and workspace semantic indexes. Export/import
aliases and namespace-member references now link across modules by identity;
overloads, explicit and inferred generic specializations, face conformances,
receiver members, documentation, canonical types, and recovered partial trees
have structured records. Position-based editor queries remain forthcoming.

## Build and explore

Current Windows development uses Git Bash with an MSYS2 UCRT64 toolchain.

```bash
bash scripts/test.sh
make parser-demo
bash scripts/module_demo.sh
make package-demo
make run-demo
make geometry-demo
make units-demo
make closure-demo
bin/sagan --capabilities-json
bin/sagan --diagnostics-json examples/type_error.sagan
make editor-tooling-demo
make workspace-demo
make semantic-index-demo
bash scripts/optional_demo.sh
bash scripts/weak_demo.sh
make ownership-demo
bash scripts/payload_enum_demo.sh
bash scripts/generic_sum_demo.sh
bash scripts/generic_class_demo.sh
bash scripts/semantic_demo.sh
bash scripts/type_demo.sh
bash scripts/entry_demo.sh
bash scripts/execution_demo.sh
bash scripts/ast_demo.sh
bash scripts/ast_demo.sh --no-open
make coverage
bash scripts/docs.sh check
bash scripts/windows/build_installer.sh
bash scripts/windows/test_installer.sh
```

Useful compiler commands include:

```bash
bin/sagan examples/run_demo.sagan
bin/sagan --tokens examples/tokenizer_demo.sagan
bin/sagan --ast examples/parser_demo.sagan
bin/sagan --ast-dot examples/parser_demo.sagan
bin/sagan --ast-svg examples/parser_demo.sagan build/ast.svg
bin/sagan --ast-html examples/parser_demo.sagan build/ast.html
bin/sagan --semantic examples/semantic_demo.sagan
bin/sagan --types examples/type_demo.sagan
bin/sagan --entry examples/entry_demo.sagan
bin/sagan --modules examples/module_demo/main.sagan
bin/sagan --emit-cpp-modules examples/module_demo/main.sagan build/modules.cpp
bin/sagan --package examples/package_demo
bin/sagan --emit-cpp-package examples/package_demo build/package.cpp
bin/sagan --run-package examples/package_demo
bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp
bin/sagan --version
```

See [development setup](docs/contributing/development-setup.md) and the
[command-line reference](docs/tooling/command-line.md) for prerequisites,
coverage, demonstrations, and all supported output modes.

## Documentation

The documentation is available at **[sagan.shoulak.org](https://sagan.shoulak.org/)**.
The site defaults to the newest released documentation, archives specific
release versions, and also provides a selectable **experimental** version for
the live repository. Unfinished or unverified pages are marked as work in
progress.

- [Getting started](docs/getting-started/index.md)
- [Language tour](docs/tour/index.md)
- [Language reference](docs/reference/index.md)
- [Compiler and tooling](docs/tooling/index.md)
- [Implementation notes](docs/implementation/index.md)
- [Design decisions](docs/decisions/index.md) and [RFCs](docs/rfcs/index.md)
- [Contributor documentation](docs/contributing/index.md)

Preview and validate the site locally with:

```bash
bash scripts/docs.sh setup
bash scripts/docs.sh serve
bash scripts/docs.sh check
```

## Versioning and contributing

Development builds use a Git-derived identity such as
`0.MINOR.PATCH+g1a2b3c4d[.dirty]`. Conventional Commit declarations determine semantic
version impact after the configured baseline. Documentation and maintenance
commits do not change the language version.

Read the [versioning workflow](docs/contributing/versioning.md) before preparing
a version-changing commit, and see the [contributor guide](docs/contributing/index.md)
for development, testing, documentation, and licensing practices.

## Origins and license

Sagan is based on Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic), which provided the
initial build structure and compiler foundations. Schematic is licensed under
the GNU General Public License version 3, and Sagan preserves the applicable
attribution and GPLv3 obligations for derived code.

See [project history](docs/about/project-history.md) and
[license and attribution](docs/about/license-and-attribution.md) for the full
provenance and licensing record.
