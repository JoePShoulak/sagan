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

**Implemented in the parser, type checker, and C++ backend:** the four words are
dedicated tokens and produce exception AST nodes. The grammar is:

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

`scream expression` throws that value. Each `unless` expression is evaluated in
source order and matches only when its checked Sagan type and value are both
equal to the thrown value. The first match runs; if none match, the value
propagates to an enclosing `hope`. A handler pattern is an ordinary expression,
not a binding declaration, so this initial model does not introduce a caught
value name.

`finally` runs once after normal completion, a handled exception, an unmatched
exception, or an early `return`. Cleanup occurs before an unmatched value is
observed by an enclosing handler. Sagan-thrown class and face values retain
their shared reference-counted identity while propagating.

To keep cleanup behavior deterministic during stack unwinding, a `finally`
body cannot `return`, `scream`, or use `break`/`continue` to control a loop
outside that cleanup body. Loops wholly inside the cleanup body may use their
own `break` and `continue` statements.

The initial backend catches only values raised by `scream`. Native runtime
failures currently used for checked arithmetic and collection bounds are not
yet converted into Sagan values and therefore cannot be selected by `unless`.
Binding/destructuring handlers, declared exception effects, cleanup that itself
fails while another exception is unwinding, and a stable uncaught-exception
report remain future work.
