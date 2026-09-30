---
title: Command line
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Command line
## Executable

```bash
bin/sagan path/to/source.sagan
bin/sagan --tokens path/to/source.sagan
bin/sagan --self-test
bin/sagan --version
bin/sagan --capabilities-json
bin/sagan --diagnostics-json path/to/source.sagan
bin/sagan --ast path/to/source.sagan
bin/sagan --ast-dot path/to/source.sagan
bin/sagan --ast-svg path/to/source.sagan build/tree.svg
bin/sagan --ast-html path/to/source.sagan build/tree.html
bin/sagan --semantic path/to/source.sagan
bin/sagan --types path/to/source.sagan
bin/sagan --entry path/to/source.sagan
bin/sagan --modules path/to/main.sagan
bin/sagan --package path/to/package
bin/sagan --emit-cpp path/to/source.sagan
bin/sagan --emit-cpp path/to/source.sagan build/program.cpp
bin/sagan --emit-cpp-modules path/to/main.sagan build/program.cpp
bin/sagan --emit-cpp-package path/to/package build/program.cpp
bin/sagan --run-package path/to/package
bin/sagan --application-mode path/to/source-or-package
```

With no valid source path, the program reports command usage or a file error.
With a source path and no mode flag, Sagan performs the implemented front-end
checks, emits temporary C++, invokes the native compiler, runs the program, and
returns its exit code. `--tokens` prints the token stream without parsing or
executing. `--ast` prints a readable parsed tree. `--ast-dot`
prints Graphviz DOT without requiring Graphviz itself. SVG and HTML modes write
their final argument; HTML includes both the original input and an embedded
visual tree.

`--semantic` parses the file, performs the implemented scope and name-resolution
pass, and prints scopes, symbols, and resolved references. It reports undefined
or duplicate names, but it does not type-check or execute the program.
`--types` additionally runs the implemented type checker and prints inferred
declaration and expression types.
`--entry` performs name and type checks, then validates the executable `main`
contract. It does not generate or run a program.
`--modules` resolves an entry file's graph. It uses loose sibling mapping when
there is no manifest, or discovers the nearest `sagan.toml` and maps qualified
module names to nested files beneath its source root.
`--package` accepts a package directory or `sagan.toml`, validates its strict
manifest, resolves the configured entry, and prints the dependency-ordered graph.
`--emit-cpp-modules` links selective imports and exported members accessed
through whole-module namespaces into one isolated compilation unit, performs
semantic, type, and entry-point validation, then prints or writes the generated
C++.
`--emit-cpp-package` performs the same linked checks and generation starting
from the package manifest's configured entry module.
`--run-package` performs those package checks, compiles temporary C++, runs the
configured entry, and returns its exit code.
`--application-mode` prints `console` or `windowed`. It reads the named package
manifest or discovers the nearest manifest for a source file; a loose file
prints `console`.
`--capabilities-json` prints the versioned compiler-service schema and granular
implemented/unavailable feature flags. `--diagnostics-json` performs recovering
lexical and syntax analysis followed, for complete syntax, by strict semantic
and type checking through the reusable language-service API. It prints one
machine-readable JSON document, including `recovered` state and multiple
diagnostics where applicable. It exits successfully only for a complete check;
this is an automation/testing interface, not the future
stdio Language Server Protocol transport.
`--emit-cpp` performs the same front-end and entry checks, then prints generated
C++ or writes it to the optional output path. It does not itself invoke a C++
compiler.

On Windows, `sagan-launch.exe` is the File Explorer dispatcher. Double-clicking
a `.sagan` file obeys its package's application mode. Explorer also exposes
**Run in Terminal** and **Run Without Terminal** overrides. Console launches
pause before closing so output remains readable. Windowed launches redirect
diagnostics to `%LOCALAPPDATA%\Sagan\logs\latest-launch.log` and display a native
error dialog when execution fails.

The generated HTML viewer offers zoom-in, zoom-out, fit, and 100% controls.
Mouse-wheel zoom follows the pointer, and the tree can be dragged to pan across
large syntax trees.

For the repository demonstrations, the concrete forms are:

```bash
bin/sagan examples/showcase.sagan
bin/sagan --tokens tests/fixtures/syntax/tokenizer.sagan
bin/sagan --ast examples/ast.sagan
bin/sagan --ast-dot examples/ast.sagan
bin/sagan --ast-svg examples/ast.sagan build/ast.svg
bin/sagan --ast-html examples/ast.sagan build/ast.html
bin/sagan --semantic tests/fixtures/semantic/scopes.sagan
bin/sagan --types tests/fixtures/semantic/types.sagan
bin/sagan --entry tests/fixtures/semantic/entry.sagan
bin/sagan --modules tests/fixtures/modules/module_demo/main.sagan
bin/sagan --emit-cpp examples/showcase.sagan build/execution_demo.cpp
bin/sagan --emit-cpp-modules tests/fixtures/modules/module_demo/main.sagan build/module_demo.cpp
bin/sagan --package examples/package
bin/sagan --emit-cpp-package examples/package build/package_demo.cpp
bin/sagan --run-package examples/package
```

Successful AST output confirms only lexical and syntactic validity. Successful
semantic output additionally confirms the implemented name and scope rules.
Type output confirms the documented scalar/function, generic-function and
generic-sum, collection, class, member, constructor, and face-dispatch subset.
Generated C++ supports the language surface documented under
[code generation](../implementation/code-generation.md), with explicit
limitations listed there.

## Make targets

```bash
make all
make test
make integration-test
make coverage
make tokenizer-inspect
make ast-demo
make package-demo
make run-demo
make geometry-demo
make units-demo
make editor-tooling-demo
make get-version
make clean
```

The wrapper scripts used for the complete local checks are:

```bash
bash scripts/test.sh
bash scripts/ast_demo.sh
bash scripts/ast_demo.sh --no-open
bash tests/integration/run.sh
bash scripts/package_demo.sh
bash scripts/run_demo.sh
bash scripts/geometry_demo.sh
bash scripts/units_demo.sh
bash scripts/docs.sh check
```

`all` builds `bin/sagan`; `test` runs unit, CLI, and integration tests;
`integration-test` runs the fixture-driven end-to-end layer; `coverage` performs
a clean instrumented build and produces LCOV output when `lcov` is installed;
`tokenizer-inspect` prints tokens for the representative syntax fixture;
`ast-demo` writes the visual source-and-tree page to `build/ast-demo.html`;
`get-version` prints the calculated build identity; and `clean` removes compiler
objects and binaries.
`package-demo` prints a strict manifest and nested source tree, resolves dotted
modules, emits C++, compiles it, and runs the resulting native program.
`run-demo` proves that the compiler driver directly runs a source file and a
manifest-backed package without an external build script.
`geometry-demo` executes the affine distinction between points and displacement
vectors, including translation and point subtraction, then demonstrates native
`s(...)` spherical points and `s<...>` spherical vectors.
`units-demo` runs the representative physical-units program.
`editor-tooling-demo` prints deliberately incomplete source, capability
discovery, and the recovered structured diagnostics produced for it.
The former feature-specific demos are regression tests under
`tests/integration/`; their Sagan inputs are organized by compiler phase under
`tests/fixtures/`.

Development versions have the form
`MAJOR.MINOR.PATCH+gREVISION[.dirty]`. Conventional Commit markers after the
configured baseline determine the numeric version, and `.dirty` records tracked
or untracked working-tree changes. Use `bash scripts/version.sh prepare TYPE`
before a patch, minor, or major commit and `bash scripts/version.sh check-badge`
afterward. The complete workflow is documented in
[Versioning](../contributing/versioning.md).
