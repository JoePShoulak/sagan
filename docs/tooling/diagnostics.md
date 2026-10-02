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
structured form with schema `sagan.language-service/1`, phase codes and distinct
codes for built-in runtime failures,
severity, owning phase, byte and UTF-16 ranges, message, related-location, note,
and fix containers. Terminal text and JSON are separate renderers. Terminal
diagnostics show `error[CODE]: message`, a source excerpt, related locations,
notes, and known-safe `help:` fixes. Colors appear only on an interactive
terminal; `NO_COLOR` disables them. Paths are project-relative when possible.
Direct `sagan FILE` execution reports independent recoverable lexical and syntax
errors together, up to 50 by default. Set `SAGAN_MAX_ERRORS` to a positive
number up to 1000 to change the display limit. Semantic/type batch checking
still stops at the first error, while the editor-facing analysis path performs
bounded lexical and top-level syntax recovery and can report multiple errors
from one document. Capability discovery reports recovery as available.

```bash
bin/sagan --diagnostics-json tests/fixtures/semantic/type_error.sagan
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
bin/sagan --tokens tests/fixtures/syntax/tokenizer_error.sagan
```

The diagnostic identifies the invalid source region for `1e`.

The parser produces focused syntax diagnostics for malformed
assignments and member access, misplaced or incomplete declarations, invalid
control flow, malformed interpolation and collections, exception constructs,
documentation-comment placement, and other established grammar rules. Run the
complete positive and negative demonstration with:

```bash
bash tests/integration/parser_test.sh
```

These errors establish grammatical validity only. Semantic and type failures
also use the structured presentation in the strict document checker. Direct
execution uses the language-service native operations, so built-in runtime
failures and assertions show Sagan source instead of an uncaught C++ exception.
Runtime codes include `SAG-RUN-0100` (unhandled `scream`), `SAG-RUN-0101`
(integer overflow), `SAG-RUN-0102` (division by zero), and `SAG-RUN-0200`
(assertion failure). Unexpected native exceptions use `SAG-RUN-0999` without
inventing a Sagan source location. Supported integer
overflow diagnostics include the operands. A compact call trace lists Sagan
functions from the failure outward; generated C++ frames are not shown.
Unmapped native build failures do not claim a Sagan source location.

This is an incremental implementation of the unified error design. Distinct
codes for every lexical, syntax, semantic, type, module, and project failure,
complete runtime call-site traces, expanded trace controls, and comprehensive
safe hints are not yet implemented. Generic phase codes are not individual
error identities.

## Agreed direction for all Sagan errors

The intended presentation is precise and calm: a stable code, concise headline,
one highlighted source excerpt at the failure, related declarations when useful,
and a reliable `help:` suggestion only when Sagan knows one. Warnings use the
same layout. Runtime tracebacks put the failure first and list Sagan callers
back toward the root; compiler-generated and native frames stay in an optional
technical view. A future full-trace option should expand source snippets for
each call. Values may appear when they can be captured safely and accurately.
Normal completion and explicit `exit(code)` do not produce an error. User
`scream` messages are the headline. Native tool failures should show relevant
tool output as details, without inventing a Sagan location. The terminal and
editor must consume the same structured compiler facts, with terminal color
only when supported and plain text when redirected.
