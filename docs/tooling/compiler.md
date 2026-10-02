---
title: Compiler
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler
The `sagan` command can run a program or show what the compiler understands
about it. A source path compiles and runs the program. `--tokens` shows the
smallest pieces recognized from the text, while the AST flags show the parsed
structure as text, DOT, SVG, or interactive HTML. AST means *abstract syntax
tree*: a tree-shaped representation of the program's grammar.
It can also analyze lexical scopes and print symbols and resolved references.
The `--types` mode prints inferred and declared types, including collections,
geometry, units, and callable types.
The `--entry` mode checks the selected root file as an executable program;
no specially named function is required.
The `--emit-cpp` mode emits checked C++, and `--run-package`
runs the entry selected by a package manifest.

```bash
make all
bin/sagan examples/showcase.sagan
bin/sagan --tokens tests/fixtures/syntax/tokenizer.sagan
```

With only a source path, it validates, generates temporary C++, invokes the
native compiler, runs the program, and returns its exit code. Token and AST
flags inspect the source without executing it. Lexical and syntax errors produce
focused, source-located diagnostics.

```bash
bin/sagan --ast examples/ast.sagan
bin/sagan --ast-dot examples/ast.sagan
bin/sagan --ast-svg examples/ast.sagan build/ast.svg
bin/sagan --ast-html examples/ast.sagan build/ast.html
bin/sagan --semantic tests/fixtures/semantic/scopes.sagan
bin/sagan --types tests/fixtures/semantic/types.sagan
bin/sagan --entry tests/fixtures/semantic/entry.sagan
bin/sagan --emit-cpp examples/showcase.sagan build/execution_demo.cpp
bin/sagan --run-package examples/package
```

The HTML output is self-contained and supports button or mouse-wheel zoom and
drag-to-pan navigation.

```bash
bin/sagan --self-test
bin/sagan --version
```

The first runs compiled-in tokenizer, parser, renderer, and defensive checks;
the second prints the Git-derived development build identity. The complete
`bash scripts/test.sh` command also runs the separate CLI integration suite.

!!! note
    Sagan currently generates C++ and uses `g++` as its native backend.
    Compiler discovery and configuration are intentionally small, and the
    generated C++ interface is not a stable public ABI.
