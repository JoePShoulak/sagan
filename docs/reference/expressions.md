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
lambdas, and the semantic rules for safe access and assignment expressions.

## String expressions

Ordinary single- and double-quoted strings, raw strings, and triple-double-quoted
multiline strings are parsed as expressions. An interpolated string retains an
ordered sequence of decoded text and embedded expression nodes:

```sagan
let status = "mission ${mission.name}: altitude ${ship.position.altitude}"
let path = r"C:\simulation\${literal_text}"
```

Interpolation accepts a complete Sagan expression and therefore follows the
same precedence rules as an expression outside a string. `${}` is a syntax
error because every interpolation requires an expression. Raw strings never
interpolate; `${literal_text}` in the raw example is ordinary text.

## Implemented precedence

From highest to lowest:

1. calls, indexing, member access, safe member access, and postfix `++` and `--`;
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

Postfix forms chain from left to right. For example,
`fleet[index]?.navigator.course(origin).magnitude()` first indexes `fleet`,
safely accesses `navigator`, accesses and calls `course`, then accesses and
calls `magnitude`.
Evaluation order, short-circuit behavior, assignability, overload resolution,
and coercion remain semantic-analysis work.
