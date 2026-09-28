---
title: Documentation status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation status
## Roadmap snapshot

| Area | Status | Evidence |
| --- | --- | --- |
| Language direction | **Settled enough for early work** | README design and lexical rules |
| Token vocabulary | **Implemented** | `tokens.hpp`, `tokens.cpp` |
| Tokenizer | **Complete for the current lexical specification** | Unicode-aware lexer, comprehensive self-tests, examples |
| Parser and Sagan AST | **Next; not implemented** | roadmap only |
| Semantic analysis | **Planned; not implemented** | roadmap only |
| Runtime and memory model | **Provisional/planned** | design intent only |
| Standard/core libraries | **Model settled; APIs open** | README core-library model |
| C++ code generation | **Planned; not implemented** | roadmap only |
| Deterministic execution | **Goal; contract open** | design intent only |

## Tokenizer verification

The tokenizer build verifies Unicode 17 XID and emoji identifiers, malformed
UTF-8 rejection, NFC normalization, every current keyword and operator,
focused error cases, and randomized byte-input robustness. Interpretation of
newlines inside ambiguous `<...>` and `{...}` constructs belongs to the parser
and is not unfinished tokenizer behavior.

## Major open language questions

Type inference, value/reference behavior, reference-count cycles, interface
defaults and conflict resolution, generics and possible sum types, constructors,
enum and collection semantics, exception propagation, entry points, modules and
packages, and the exact built-in/core-library boundary remain unresolved.
