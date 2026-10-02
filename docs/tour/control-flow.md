---
title: Control flow
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Control flow
Control flow decides which statement runs next. Sagan implements `if`/`else`,
`for`/`in`, `while`, `until`, `break`, `continue`, `return`, and `match`/`case`.
The parser also reserves `yield` for future generators.

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

When a branch or loop contains exactly one statement, it may remain on the
header line without braces:

```sagan
for item in items process(item)
if simulation.complete return simulation.result
else return fallback
```

Moving the body to a later line requires braces; indentation alone never
creates a block.

Here is a compact, executable Fibonacci function using several independent
rules together:

```sagan
--8<-- "docs/examples/executable/fibonacci.sagan"
```

`let i, a, b = 0, 0, 1` creates three `Int64` variables. The postfix `i++`
uses the old counter value for the comparison and then increments it. On each
loop iteration, both right-hand values in `a, b = b, a + b` are calculated
before either variable changes. `fast_fibonacci(10)` prints `144` with this
loop condition; change `<=` to `<` if you want one fewer iteration.

`break` and `continue` are unlabeled and valid only inside a loop. A `return`
may carry an expression or stand alone to return without a value.

`yield` has the same two syntactic forms:

```sagan
fun telemetry_samples(samples) {
  for sample in samples {
    yield sample
  }
}
```

`yield` can be parsed and represented in the syntax tree, but generator typing,
suspension, execution, and iteration are not implemented. Do not use it in an
executable program yet.

A match statement contains one or more cases. Single-statement cases can use
the same compact form:

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

`case else` is optional, unique, and must be last. Value patterns compare with
the match subject. Enum cases may match nominal values, and payload-enum cases
may bind their contained values. Covering every case of an enum makes the match
exhaustive without `case else`.

`and`, `or`, `not`, and `!` provide logical operations.
A conditional expression uses `?` and `;`:

```sagan
let status = active ? "running" ; "stopped"
```

Newlines normally terminate statements; `;` is not an ordinary statement
terminator. The tokenizer already suppresses newlines inside parentheses and
brackets and after tokens that leave an expression incomplete.

The checker rejects unreachable statements after unconditional control
transfer. Custom iteration protocols and executable `yield` semantics remain
future work.
