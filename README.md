<p align="center">
  <img src="docs/assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="280">
</p>

# Sagan

[![Development version 0.17.0](https://img.shields.io/badge/development-0.17.0-2563eb)](docs/contributing/versioning.md)
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
and dictionaries plus dimensioned vectors and coordinates are inferred. Generic
annotations, member typing, the runtime, and the standard library are not yet
implemented. An initial C++ emitter can compile the validated scalar/control-
flow subset into a native executable.

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
- cross-platform deterministic simulation as a goal, with its exact contract
  still to be defined.

The [design philosophy](docs/design/philosophy.md),
[goals and non-goals](docs/design/goals-and-non-goals.md), and
[language reference](docs/reference/index.md) distinguish settled direction
from provisional and unresolved behavior.

## Build and explore

Current Windows development uses Git Bash with an MSYS2 UCRT64 toolchain.

```bash
bash scripts/test.sh
make parser-demo
bash scripts/semantic_demo.sh
bash scripts/type_demo.sh
bash scripts/entry_demo.sh
bash scripts/execution_demo.sh
bash scripts/ast_demo.sh
bash scripts/ast_demo.sh --no-open
make coverage
bash scripts/docs.sh check
```

Useful compiler commands include:

```bash
bin/sagan examples/parser_demo.sagan
bin/sagan --ast examples/parser_demo.sagan
bin/sagan --ast-dot examples/parser_demo.sagan
bin/sagan --ast-svg examples/parser_demo.sagan build/ast.svg
bin/sagan --ast-html examples/parser_demo.sagan build/ast.html
bin/sagan --semantic examples/semantic_demo.sagan
bin/sagan --types examples/type_demo.sagan
bin/sagan --entry examples/entry_demo.sagan
bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp
bin/sagan --version
```

See [development setup](docs/contributing/development-setup.md) and the
[command-line reference](docs/tooling/command-line.md) for prerequisites,
coverage, demonstrations, and all supported output modes.

## Documentation

The documentation is available at **[sagan.shoulak.org](https://sagan.shoulak.org/)**.
It is versioned independently and marks unfinished or unverified pages as work
in progress.

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
