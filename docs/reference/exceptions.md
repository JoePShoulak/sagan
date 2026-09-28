---
title: Exceptions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Exceptions
**Settled design vocabulary:** `hope` starts protected code, `unless`
introduces a handler, `finally` introduces cleanup, and `scream` raises an
exception. Exceptions are intended to be the primary error mechanism.

**Implemented lexically:** the four words are dedicated keyword tokens.

**Open questions:** grammar for handler clauses, exception types and matching,
binding the caught value, propagation, stack unwinding, cleanup ordering,
interaction with return and reference counting, and treatment of runtime errors
such as overflow.

There is no parser or runtime exception implementation.
