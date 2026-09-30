---
title: Diagnostics
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Diagnostics
**Implemented tooling foundation:** diagnostics have a compiler-owned
structured form with schema `sagan.language-service/1`, stable phase codes,
severity, owning phase, byte and UTF-16 ranges, message, related-location, note,
and fix containers. Terminal text and JSON are separate renderers. The current
strict checker reports one diagnostic because recovering analysis is a later
roadmap phase; capability discovery reports that limitation explicitly.

```bash
bin/sagan --diagnostics-json examples/type_error.sagan
bin/sagan --capabilities-json
```

The reusable checker accepts immutable document snapshots and returns
`complete`, `incomplete`, or `cancelled` with the analyzed document version.
`recovered` and `stale` are reserved result states whose producing subsystems
are not implemented yet.

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

These errors establish grammatical validity only. Semantic and type failures
also use the structured presentation in the strict document checker. Module,
project, build, entry-point, and runtime diagnostic phases are reserved in the
schema and will migrate to the model as their reusable operations land.
