---
title: Exceptions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Exceptions
Sagan uses four exception words: `hope` starts protected code, `unless`
introduces a handler, `finally` introduces cleanup, and `scream` raises an
exception. An exception is a value that interrupts the normal path through a
function so another block can handle the problem.

The grammar is:

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

The built-in `RuntimeError` enum converts native failures into ordinary
Sagan exception values. Its current cases are `integer_overflow`,
`division_by_zero`, `modulo_by_zero`, `undefined_exponentiation`,
`negative_integer_exponent`, `index_out_of_bounds`, `invalid_range`,
`invalid_conversion`, and `missing_key`.

```sagan
hope print(values[values_count])
unless RuntimeError.index_out_of_bounds print("No such value")
```

*Nominal* means that `RuntimeError` is its own named type rather than merely a
collection of numbers. The same exact type-and-value matching rules apply, so native failures propagate
through nested `hope` statements and always run `finally` cleanup. Uncaught
native failures retain a readable diagnostic. Binding/destructuring handlers,
declared exception effects, cleanup that itself fails while another exception
is unwinding, and a fully specified process-level uncaught-exception format
remain future work.
