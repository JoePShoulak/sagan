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
bin/sagan --modules path/to/main.sagan
bin/sagan --package path/to/package
bin/sagan --emit-cpp path/to/source.sagan
bin/sagan --emit-cpp path/to/source.sagan build/program.cpp
bin/sagan --emit-cpp-modules path/to/main.sagan build/program.cpp
bin/sagan --emit-cpp-package path/to/package build/program.cpp
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
bin/sagan --modules examples/module_demo/main.sagan
bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp
bin/sagan --emit-cpp-modules examples/module_demo/main.sagan build/module_demo.cpp
bin/sagan --package examples/package_demo
bin/sagan --emit-cpp-package examples/package_demo build/package_demo.cpp
```

Successful AST output confirms only lexical and syntactic validity. Successful
semantic output additionally confirms the implemented name and scope rules.
Type output confirms the documented scalar/function, generic-function and
generic-sum, collection, class, member, constructor, and face-dispatch subset.
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
make module-demo
make package-demo
make optional-demo
make weak-demo
make payload-enum-demo
make generic-sum-demo
make generic-class-demo
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
bash scripts/module_demo.sh
bash scripts/package_demo.sh
bash scripts/optional_demo.sh
bash scripts/weak_demo.sh
bash scripts/payload_enum_demo.sh
bash scripts/generic_sum_demo.sh
bash scripts/generic_class_demo.sh
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
`module-demo` prints a three-module dependency graph, executes selective and
namespace imports, and verifies export visibility, filename, and cycle diagnostics.
`package-demo` prints a strict manifest and nested source tree, resolves dotted
modules, emits C++, compiles it, and runs the resulting native program.
`optional-demo` prints its Sagan source and generated C++, then executes typed
optional construction, payload matching, safe access, and lazy fallback chains.
`weak-demo` shows a live weak reference resolving to `Some`, lets its strong
owner leave scope, then shows the expired reference resolving to `None`.
`payload-enum-demo` constructs typed cases, binds their payloads in an
exhaustive match, shows the tagged-variant C++ excerpts, and runs the result.
`generic-sum-demo` infers a top-level `identity<T>` function at its call sites,
specializes a generic result enum from an expected type and explicit
`Result<Int, String>.Failure(...)` qualification, then executes both payload matches.
`generic-class-demo` infers `Box<T>` from constructor arguments, checks typed
fields and methods, converts it to `Readable<T>`, executes an inherited generic
face default with virtual `self` dispatch, infers a class method's independent
`U`, and performs typed mutation through generated C++ templates.
`execution-demo` shows the Sagan input and generated C++, builds it with `g++`,
runs it, and reports the native exit code.

Development versions have the form
`MAJOR.MINOR.PATCH+gREVISION[.dirty]`. Conventional Commit markers after the
configured baseline determine the numeric version, and `.dirty` records tracked
or untracked working-tree changes. Use `bash scripts/version.sh prepare TYPE`
before a patch, minor, or major commit and `bash scripts/version.sh check-badge`
afterward. The complete workflow is documented in
[Versioning](../contributing/versioning.md).
