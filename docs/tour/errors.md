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

**Implemented lexically:** all four words have dedicated keyword tokens.

!!! warning "Design syntax"
    The example is tokenizer input, not executable exception handling.

**Open questions:** exception types, matching, handler binding, propagation,
stack unwinding, interaction with reference counting, and whether all failures
use exceptions.
