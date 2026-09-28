---
title: Control flow
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Control flow
The tokenizer reserves `if`, `else`, `match`, `case`, `for`, `in`,
`while`, `until`, `break`, `continue`, `return`, and `yield`.

```sagan
if ready and not failed {
  return
} else {
  continue
}
```

**Settled design:** `and`, `or`, `not`, and `!` provide logical operations.
A conditional expression uses `?` and `;`:

```sagan
let status = active ? "running" ; "stopped"
```

Newlines normally terminate statements; `;` is not an ordinary statement
terminator. The tokenizer already suppresses newlines inside parentheses and
brackets and after tokens that leave an expression incomplete.

**Provisional design:** complete grammar, operator precedence, matching,
iteration protocols, yield semantics, and parser-informed handling of braces
and angle brackets.
