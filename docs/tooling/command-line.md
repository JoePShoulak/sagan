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
bin/sagan --self-test
bin/sagan --version
bin/sagan --ast path/to/source.sagan
bin/sagan --ast-dot path/to/source.sagan
bin/sagan --ast-svg path/to/source.sagan build/tree.svg
bin/sagan --ast-html path/to/source.sagan build/tree.html
bin/sagan --semantic path/to/source.sagan
bin/sagan --types path/to/source.sagan
bin/sagan --entry path/to/source.sagan
bin/sagan --emit-cpp path/to/source.sagan
bin/sagan --emit-cpp path/to/source.sagan build/program.cpp
```

With no valid source path, the program reports command usage or a file error.
With a source path and no mode flag, it prints the token stream; it does not
parse or execute the file. `--ast` prints a readable parsed tree. `--ast-dot`
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
`--emit-cpp` performs the same front-end and entry checks, then prints generated
C++ or writes it to the optional output path. It does not itself invoke a C++
compiler.

The generated HTML viewer offers zoom-in, zoom-out, fit, and 100% controls.
Mouse-wheel zoom follows the pointer, and the tree can be dragged to pan across
large syntax trees.

For the repository demonstrations, the concrete forms are:

```bash
bin/sagan examples/tokenizer_demo.sagan
bin/sagan --ast examples/parser_demo.sagan
bin/sagan --ast-dot examples/parser_demo.sagan
bin/sagan --ast-svg examples/parser_demo.sagan build/ast.svg
bin/sagan --ast-html examples/parser_demo.sagan build/ast.html
bin/sagan --semantic examples/semantic_demo.sagan
bin/sagan --types examples/type_demo.sagan
bin/sagan --entry examples/entry_demo.sagan
bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp
```

Successful AST output confirms only lexical and syntactic validity. Successful
semantic output additionally confirms the implemented name and scope rules.
Type output confirms the documented scalar/function, collection, class, member,
constructor, and face-dispatch subset, but does not yet establish generic annotations.
Generated C++ supports only the initial executable subset documented under
[code generation](../implementation/code-generation.md).

## Make targets

```bash
make all
make test
make coverage
make demo
make parser-demo
make ast-demo
make semantic-demo
make type-demo
make entry-demo
make execution-demo
make get-version
make clean
```

The wrapper scripts used for the complete local checks are:

```bash
bash scripts/test.sh
bash scripts/ast_demo.sh
bash scripts/ast_demo.sh --no-open
bash scripts/semantic_demo.sh
bash scripts/type_demo.sh
bash scripts/entry_demo.sh
bash scripts/execution_demo.sh
bash scripts/docs.sh check
```

`all` builds `bin/sagan`; `test` runs self-tests; `coverage` performs a clean
instrumented build and produces LCOV output when `lcov` is installed; `demo` tokenizes the
repository example; `parser-demo` prints the parser example's AST; `ast-demo`
writes the visual source-and-tree page to `build/ast-demo.html`; `get-version`
prints the calculated build identity; `semantic-demo` prints the successful
semantic model; `type-demo` prints the successful type model; and `clean`
removes compiler objects and the binary. `entry-demo` validates a successful
entry point and focused control-flow failures. The test target exercises
semantic/type/entry/code-generation success plus focused failures.
`execution-demo` shows the Sagan input and generated C++, builds it with `g++`,
runs it, and reports the native exit code.

Development versions have the form
`MAJOR.MINOR.PATCH+gREVISION[.dirty]`. Conventional Commit markers after the
configured baseline determine the numeric version, and `.dirty` records tracked
or untracked working-tree changes. Use `bash scripts/version.sh prepare TYPE`
before a patch, minor, or major commit and `bash scripts/version.sh check-badge`
afterward. The complete workflow is documented in
[Versioning](../contributing/versioning.md).
