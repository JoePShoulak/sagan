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
  scream "engine offline"
} unless "engine offline" {
  print("Using the backup engine")
} finally {
  print("Attempt complete")
}
```

`hope` has a protected block followed by one or more expression-shaped `unless`
handlers and an optional final `finally` block.
A `finally`-only `hope` is also valid. At least one handler or cleanup clause is
required, and `scream` requires an expression. Each clause may use braces or a
same-line statement.

In the executable subset, the first handler with the same checked type and
equal value catches the exception. Nonmatching values continue outward to an
enclosing `hope`. `finally` always runs, including during propagation and before
an early return leaves the protected region.

Cleanup cannot itself `return` or `scream`, and it cannot break or continue an
enclosing loop. This keeps cleanup deterministic while another exception may
already be unwinding.

Handlers do not bind the thrown value yet, and native runtime failures such as
checked arithmetic errors are not currently catchable as Sagan values.
