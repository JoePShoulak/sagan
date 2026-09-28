---
title: Compiler
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler
The current `sagan` executable is a front-end inspection driver. It can print a
token stream or parse source into a source-spanned AST rendered as text, DOT,
SVG, or interactive HTML.

```bash
make all
bin/sagan examples/tokenizer_demo.sagan
```

With only a source path, it prints token name, source span, source text, and
decoded values where relevant. AST flags parse the same source and render the
tree. Lexical and syntax errors produce focused, source-located diagnostics.

```bash
bin/sagan --ast examples/parser_demo.sagan
bin/sagan --ast-dot examples/parser_demo.sagan
bin/sagan --ast-svg examples/parser_demo.sagan build/ast.svg
bin/sagan --ast-html examples/parser_demo.sagan build/ast.html
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
    Parsing is structural only. The executable does not perform name resolution,
    type checking, interface conformance, runtime execution, C++ generation, or
    native compiler invocation.
