---
title: Implementation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Implementation
The compiler implements the full path from source text to a native executable:

```text
UTF-8 source -> tokenizer -> parser -> AST -> name analysis -> type checking -> C++ emission
```

The executable path is:

```text
validated program -> generated C++ -> g++ -> native executable
```

This path is operational for the documented language. Begin with the
[technology stack and change map](technology-stack.md) for the complete system
and maintenance checklist, then read about the [architecture](architecture.md),
[tokenizer](tokenizer.md), [parser](parser.md), and
[Schematic-derived foundations](schematic-foundations.md). The
[semantic analyzer](semantic-analysis.md) page explains name, type, and
control-flow checking. The [code generator](code-generation.md) records the
native backend and its deliberate limitations.

Editor-facing analysis reuses the same compiler logic through immutable source
snapshots, recovering syntax, unsaved overlays, structured diagnostics, and a
semantic index. The C++ language server exposes position queries, formatting,
safe edits, test and native-operation contracts, and other negotiated features
to the VS Code extension. The debug adapter remains experimental. See the
[language-server documentation](../tooling/language-server.md) for the exact
implemented boundary.
