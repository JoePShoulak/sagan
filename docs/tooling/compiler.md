---
title: Compiler
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler
The current `sagan` executable is both a native execution driver and a front-end
inspection tool. A source path compiles and runs the program; `--tokens` prints
a token stream, while parser flags render its source-spanned AST as text, DOT,
SVG, or interactive HTML.
It can also analyze lexical scopes and print symbols and resolved references.
The `--types` mode prints the implemented scalar/function type model.
The `--entry` mode validates whether a fully checked unit has a legal executable
entry point.
The `--emit-cpp` mode emits C++ for the executable subset, and `--run-package`
runs the entry selected by a package manifest.

```bash
make all
bin/sagan examples/run_demo.sagan
bin/sagan --tokens examples/tokenizer_demo.sagan
```

With only a source path, it validates, generates temporary C++, invokes the
native compiler, runs the program, and returns its exit code. Token and AST
flags inspect the source without executing it. Lexical and syntax errors produce
focused, source-located diagnostics.

```bash
bin/sagan --ast examples/parser_demo.sagan
bin/sagan --ast-dot examples/parser_demo.sagan
bin/sagan --ast-svg examples/parser_demo.sagan build/ast.svg
bin/sagan --ast-html examples/parser_demo.sagan build/ast.html
bin/sagan --semantic examples/semantic_demo.sagan
bin/sagan --types examples/type_demo.sagan
bin/sagan --entry examples/entry_demo.sagan
bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp
bin/sagan --run-package examples/package_demo
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

!!! warning
    C++ generation and direct execution are limited to the documented native
    subset. Compiler discovery and configuration are still deliberately small.
