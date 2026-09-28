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

**Implemented in the parser:** the four words are dedicated tokens and produce
exception AST nodes. The grammar is:

```text
statement_body := block | same_line_statement
hope_statement := "hope" statement_body
                  ("unless" expression statement_body)*
                  ("finally" statement_body)?
scream_statement := "scream" expression
```

A `hope` must contain at least one `unless` or `finally` clause. Multiple
handlers are accepted. `finally`, when present, is unique and last. Each body
may be a braced block or one statement on its header line, and clauses may begin
on the next logical line.
Standalone `unless` and `finally`, a clause-free `hope`, and a value-free
`scream` are syntax errors.

**Open questions:** exception types and handler matching, binding the caught
value, propagation, stack unwinding, cleanup ordering,
interaction with return and reference counting, and treatment of runtime errors
such as overflow.

There is no semantic or runtime exception implementation yet.
