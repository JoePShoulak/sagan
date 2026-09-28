---
title: Implementation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Implementation
The present executable implements the syntactic front end of a future compiler:

```text
UTF-8 source -> tokenizer -> token stream -> parser -> AST
```

The remaining planned pipeline is:

```text
AST -> semantic analysis -> C++ generation -> C++ compiler -> native executable
```

The first line is operational. Read about the [architecture](architecture.md),
[tokenizer](tokenizer.md), [parser](parser.md), and
[Schematic-derived foundations](schematic-foundations.md). The
[semantic analyzer](semantic-analysis.md) and
[code generator](code-generation.md) pages define future boundaries.
