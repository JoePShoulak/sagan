---
title: Grammar and syntax
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Grammar and syntax
No complete normative grammar exists yet.

**Implemented:** the tokenizer supplies tokens and newline boundaries. The
parser implements `let` declarations, primary and string expressions, the
settled precedence table, postfix chains, and collection literals.

**Provisional design:** declarations use words such as `let`, `fun`, `face`,
`class`, and `enum`; braces delimit bodies; newlines normally terminate
statements; `=>` introduces expression bodies; and collection-like delimiters
have the forms shown in the language tour.

The implemented collection grammar is equivalent to this simplified notation:

```text
array       := "[" (expression ("," expression)* ","?)? "]"
dictionary  := "{" (dictionary_entry ("," dictionary_entry)* ","?)? "}"
dictionary_entry := expression ":" expression | spread
vector      := "<" expression "," expression ("," expression)* ","? ">"
coordinate  := "(" expression "," expression ("," expression)* ","? ")"
spread      := "..." expression
group       := "(" expression ")"
```

Dictionary-versus-block interpretation is grammatical: `{...}` in an
expression position is a dictionary, while a brace following a statement form
that requires a body will be a block. Vector-versus-comparison interpretation
is likewise positional. A `<` where a primary expression must begin opens a
vector; after a left operand it is a comparison. At a vector element's top
level, `>` closes the vector, so a greater-than comparison there must be
parenthesized.

**Open questions:** the remaining statement and declaration productions,
error recovery beyond the first syntax error, and block-specific newline rules.

Implemented parser demos are accepted parser input but are not yet executable;
semantic analysis and code generation remain future stages.
