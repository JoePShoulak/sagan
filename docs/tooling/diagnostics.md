---
title: Diagnostics
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Diagnostics
**Implemented:** lexical failures use `parser::parse_error` with source spans
and focused messages. Current examples include malformed numeric literals,
invalid digit separators, incomplete scientific exponents, unknown characters,
unknown or malformed escapes, and unterminated strings, interpolation, or block
comments.

Run the deliberate error example:

```bash
bin/sagan --tokens examples/tokenizer_error.sagan
```

The diagnostic identifies the invalid source region for `1e`.

**Implemented:** the parser produces focused syntax diagnostics for malformed
assignments and member access, misplaced or incomplete declarations, invalid
control flow, malformed interpolation and collections, exception constructs,
documentation-comment placement, and other established grammar rules. Run the
complete positive and negative demonstration with:

```bash
bash scripts/parser_demo.sh
```

These errors establish grammatical validity only. **Planned:** semantic
diagnostics for names, types, interfaces, mutation, control flow, and module
resolution. Their formats and recovery behavior are not yet defined.
