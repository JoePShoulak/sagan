---
title: Diagnostics
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Diagnostics
Diagnostics have a compiler-owned
structured form with schema `sagan.language-service/1`, stable phase codes,
severity, owning phase, byte and UTF-16 ranges, message, related-location, note,
and fix containers. Terminal text and JSON are separate renderers. Strict batch
checking remains fail-fast, while the editor-facing analysis path performs
bounded lexical and top-level syntax recovery and can report multiple errors
from one document. Capability discovery reports recovery as available.

```bash
bin/sagan --diagnostics-json examples/type_error.sagan
bin/sagan --capabilities-json
```

The reusable checker accepts immutable document snapshots and returns
`complete`, `incomplete`, `recovered`, or `cancelled` with the analyzed document
version. Workspace analysis produces `stale` when a document or dependency
changes before an older result can publish, and suppresses that result's value
and diagnostics. Recovered results include a partial top-level AST where
practical; they never make malformed source valid for strict compilation.

Run the recovery demonstration, which prints deliberately incomplete Sagan and
the resulting structured diagnostics:

```bash
make editor-tooling-demo
```

Lexical failures use `parser::parse_error` with source spans
and focused messages. Current examples include malformed numeric literals,
invalid digit separators, incomplete scientific exponents, unknown characters,
unknown or malformed escapes, and unterminated strings, interpolation, or block
comments.

Run the deliberate error example:

```bash
bin/sagan --tokens examples/tokenizer_error.sagan
```

The diagnostic identifies the invalid source region for `1e`.

The parser produces focused syntax diagnostics for malformed
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
