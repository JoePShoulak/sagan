---
title: Control flow
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Control flow
The parser implements `if`, `else`, `for`, `in`, `while`, `until`, `break`,
`continue`, and `return`. The tokenizer also reserves `match`, `case`, and
`yield` for later milestones.

```sagan
for item in items {
  if item.invalid {
    continue
  }
  while item.pending {
    item.poll()
    if item.failed {
      break
    }
  }
}

until simulation.complete {
  simulation.step!()
}

return simulation.result
```

`break` and `continue` are unlabeled and valid only inside a loop. A `return`
may carry an expression or stand alone to return without a value.

**Settled design:** `and`, `or`, `not`, and `!` provide logical operations.
A conditional expression uses `?` and `;`:

```sagan
let status = active ? "running" ; "stopped"
```

Newlines normally terminate statements; `;` is not an ordinary statement
terminator. The tokenizer already suppresses newlines inside parentheses and
brackets and after tokens that leave an expression incomplete.

**Provisional design:** matching, iteration protocols, yield semantics,
unreachable-code analysis, and the runtime meaning of returned values.
