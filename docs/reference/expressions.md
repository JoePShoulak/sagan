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

Compound assignments are parsed as statements rather than expressions. Unlike
the value-producing `:=`, they do not participate in expression precedence or
chaining.

**Settled design:** `^` means exponentiation; `and`, `or`, `not`, and
`!` are logical operators; `? ... ; ...` is the conditional expression; and
prefix/postfix increment return new/old values respectively.

**Implemented semantics:** `++` and `--` require an assignable numeric operand.
The prefix form mutates before producing its value; the postfix form produces
the old value and then mutates.

`^` is mathematical exponentiation, and `^=` assigns its result; neither is a
bitwise operation. Integer exponentiation requires a non-negative exponent and
raises a runtime error when the result overflows its checked type. Floating-
point bases permit negative exponents. `0 ^ 0` is a runtime error for every
numeric type.

The native executable subset checks signed integer addition, subtraction,
multiplication, division, remainder, unary negation, increment, and decrement.
Overflow raises a runtime error. Division and remainder by zero raise runtime
errors for integer and floating-point operands. Compound assignments use the
same checks. Floating-point remainder follows `fmod` semantics.

**Implemented syntax:** anonymous lambdas use `fun(parameters) => expression`,
with optional parameter and return annotations. A lambda is a primary expression
and can participate in postfix chains, including immediate calls when grouped.

**Provisional semantics:** lambda capture behavior and the semantic rules for
spread outside array and dictionary literals, safe access, dictionary-key
hashability, and assignment expressions.

**Settled dictionary-spread behavior:** dictionary entries are applied from
left to right. When an explicit entry or later spread repeats an existing key,
the later value replaces the earlier value. Each spread operand is evaluated
once.

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
