---
title: First program
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# First program
The repository includes `examples/tokenizer_demo.sagan`, which exercises
keywords, interfaces, classes, numbers, collections, strings, interpolation,
exception vocabulary, and an emoji identifier.

Build and tokenize it:

```bash
make all
make demo
```

To run another executable file:

```bash
bin/sagan path/to/program.sagan
```

To inspect its token stream without executing it:

```bash
bin/sagan --tokens path/to/program.sagan
```

!!! warning "Tokenizer example, not executable Sagan"
    This particular demo is a broad lexical fixture. The repository also has a
    complete parser for the current syntax specification and an expanding
    semantically checked native subset. Successful tokenization or parsing
    alone does not prove that a construct is in that executable subset; use the
    execution demo below for runnable coverage.

Inspect the parser demonstration as a text tree or interactive HTML tree:

```bash
bin/sagan --ast examples/parser_demo.sagan
bash scripts/ast_demo.sh --no-open
```

To print, compile, and run the current executable showcase—including value
exceptions, propagation, and guaranteed cleanup—run:

```bash
make execution-demo
```

To see a deliberate lexical failure:

```bash
bin/sagan --tokens examples/tokenizer_error.sagan
```

That file contains `1e`, which fails because scientific notation requires
exponent digits.
