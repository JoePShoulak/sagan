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
bin/sagan examples/tokenizer_error.sagan
```

The diagnostic identifies the invalid source region for `1e`.

**Planned:** syntax diagnostics from the parser and semantic diagnostics for
names, types, interfaces, mutation, and control flow. Their formats and recovery
behavior are not defined.
