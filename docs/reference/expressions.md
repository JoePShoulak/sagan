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

**Provisional design:** lambdas and the semantic rules for spread, safe access,
dictionary-key hashability, and assignment expressions.

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

## Collection expressions

The parser implements four collection forms:

```sagan
let values = [1, 2, ...additional_values,]
let metadata = {"name": "Voyager", active_key: true, ...defaults,}
let direction = <1.0, 0.0, 0.0,>
let position = (100.0, 200.0, 300.0,)
```

Arrays and dictionaries may be empty. Dictionary keys accept any expression;
semantic analysis will determine whether a key's type is hashable. Vectors and
coordinates require at least two elements, so `<>`, `<1>`, and `(1,)` are
syntax errors. `(value)` remains a grouped expression.

Calls and all collection forms permit trailing commas. `...value` creates a
spread node; later semantic analysis will validate whether its surrounding call
or collection supports the value being expanded. A spread dictionary entry
does not use a colon.

At the top level of a vector element, `>` closes the vector. Parentheses make a
greater-than comparison explicit inside a vector, as in
`<(left > right), true>`. Outside a vector-opening expression position, `<` and
`>` retain their comparison meanings.

## Implemented precedence

From highest to lowest:

1. calls, indexing, member access, safe member access, and postfix `++` and `--`;
2. right-associative `^`;
3. prefix `++`, `--`, `!`, `not`, `...`, unary `+`, and unary `-`;
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
