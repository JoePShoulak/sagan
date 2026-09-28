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

To inspect another file:

```bash
bin/sagan path/to/program.sagan
```

The output is a stream such as token name, half-open source span, original token
text, and decoded value where applicable.

!!! warning "Tokenizer example, not executable Sagan"
    The demo expresses intended Sagan syntax and is verified as tokenizer input.
    No parser or evaluator exists, so it cannot yet be compiled or run.

To see a deliberate lexical failure:

```bash
bin/sagan examples/tokenizer_error.sagan
```

That file contains `1e`, which fails because scientific notation requires
exponent digits.
