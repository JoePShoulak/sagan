---
title: Compiler
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler
The current `sagan` executable is a tokenizer driver.

```bash
make all
bin/sagan examples/tokenizer_demo.sagan
```

It loads the named file, tokenizes it, and prints token name, source span, source
text, and decoded values where relevant. It exits with a diagnostic for a
lexical error.

```bash
bin/sagan --self-test
bin/sagan --version
```

The first runs compiled-in tokenizer checks; the second prints the Git-derived
development build identity.

!!! warning
    The executable does not parse, type-check, interpret, emit C++, or invoke a
    native compiler for Sagan source.
