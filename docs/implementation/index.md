---
title: Implementation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Implementation
The present executable implements the front edge of a future compiler:

```text
UTF-8 source -> tokenizer -> token stream
```

The remaining planned pipeline is:

```text
token stream -> parser/AST -> semantic analysis -> C++ generation
             -> C++ compiler -> native executable
```

Only the first line is operational. Read about the [architecture](architecture.md),
[tokenizer](tokenizer.md), and [Schematic-derived foundations](schematic-foundations.md).
The [parser](parser.md), [semantic analyzer](semantic-analysis.md), and
[code generator](code-generation.md) pages define future boundaries.
