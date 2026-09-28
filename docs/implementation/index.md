---
title: Implementation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Implementation
The present executable implements the syntactic front end and initial semantic
pass of a future compiler:

```text
UTF-8 source -> tokenizer -> token stream -> parser -> AST -> scope/name analysis
```

The remaining planned pipeline is:

```text
semantic model -> type checking -> C++ generation -> C++ compiler -> native executable
```

The first line is operational. Read about the [architecture](architecture.md),
[tokenizer](tokenizer.md), [parser](parser.md), and
[Schematic-derived foundations](schematic-foundations.md). The
[semantic analyzer](semantic-analysis.md) page records the implemented first
pass and its next boundary. The [code generator](code-generation.md) remains
future work.
