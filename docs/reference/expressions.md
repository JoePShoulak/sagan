---
title: Expressions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Expressions
The tokenizer recognizes identifiers, literals, grouping and collection
delimiters, member access, safe access `?.`, calls, spread `...`, assignment
with value `:=`, `=>`, comparisons, arithmetic, logical words, increment,
decrement, and compound-assignment tokens.

**Settled design:** `^` means exponentiation; `and`, `or`, `not`, and
`!` are logical operators; `? ... ; ...` is the conditional expression; and
prefix/postfix increment return new/old values respectively.

**Provisional design:** collection literals, vector and coordinate construction,
calls, lambdas, chaining, safe access, and assignment expressions.

## Implemented precedence

From highest to lowest:

1. postfix `++` and `--`;
2. right-associative `^`;
3. prefix `++`, `--`, `!`, `not`, unary `+`, and unary `-`;
4. `*`, `/`, `%`;
5. `+`, `-`;
6. `<`, `<=`, `>`, `>=`, `is`;
7. `==`, `!=`;
8. `and`;
9. `or`;
10. right-associative `? ... ; ...`;
11. right-associative `:=`.

Exponentiation binds more tightly than unary minus. Chained comparisons are a
syntax error and must be written as separate comparisons joined with `and`.

Calls, indexing, and member access are planned to join the highest postfix tier.
Evaluation order, short-circuit behavior, assignability, overload resolution,
and coercion remain semantic-analysis work.
