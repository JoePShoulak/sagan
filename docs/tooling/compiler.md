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
It can also analyze lexical scopes and print symbols and resolved references.
The `--types` mode prints the implemented scalar/function type model.

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
bin/sagan --semantic examples/semantic_demo.sagan
bin/sagan --types examples/type_demo.sagan
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
    The type mode does not yet resolve members, generic annotations, interface
    conformance, or every user-defined relationship. The executable does not
    perform runtime execution, C++ generation, or native compiler invocation.
