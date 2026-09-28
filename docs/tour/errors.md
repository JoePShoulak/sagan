---
title: Errors
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Errors
Sagan uses distinctive exception keywords:

- `hope` begins protected code;
- `unless` introduces a handler;
- `finally` introduces unconditional cleanup; and
- `scream` raises an exception.

```sagan
hope {
  update()
} unless error {
  scream error
} finally {
  cleanup()
}
```

**Implemented in the parser:** `hope` has a protected block followed by one or
more expression-shaped `unless` handlers and an optional final `finally` block.
A `finally`-only `hope` is also valid. At least one handler or cleanup clause is
required, and `scream` requires an expression. Each clause follows the preceding
`}` without an intervening logical newline.

The AST records handler patterns without assigning them binding or matching
semantics. The examples parse but are not yet executable because semantic
analysis, exception lowering, and a runtime do not exist.

**Open questions:** exception types, matching, handler binding, propagation,
stack unwinding, cleanup ordering, interaction with return and reference
counting, and whether all failures use exceptions.
