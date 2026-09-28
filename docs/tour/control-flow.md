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
`continue`, `return`, `match`, and `case`. The tokenizer also reserves `yield`
for a later milestone.

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

A match statement contains one or more block-bodied cases:

```sagan
match simulation.status {
  case "ready" {
    simulation.run!()
  }
  case expected_status {
    simulation.prepare!()
  }
  case else {
    simulation.wait!()
  }
}
```

`case else` is optional, unique, and must be last. The parser accepts
expression-shaped case patterns. Whether an identifier denotes a value, binds a
new name, or participates in destructuring will be decided by semantic analysis.

**Settled design:** `and`, `or`, `not`, and `!` provide logical operations.
A conditional expression uses `?` and `;`:

```sagan
let status = active ? "running" ; "stopped"
```

Newlines normally terminate statements; `;` is not an ordinary statement
terminator. The tokenizer already suppresses newlines inside parentheses and
brackets and after tokens that leave an expression incomplete.

**Provisional design:** match-pattern meaning and exhaustiveness, iteration
protocols, yield semantics, unreachable-code analysis, and the runtime meaning
of returned values.
