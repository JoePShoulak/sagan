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

**Implemented:** the tokenizer supplies tokens and newline boundaries.

**Provisional design:** declarations use words such as `let`, `fun`, `face`,
`class`, and `enum`; braces delimit bodies; newlines normally terminate
statements; `=>` introduces expression bodies; and collection-like delimiters
have the forms shown in the language tour.

**Open questions:** complete production rules, operator precedence and
associativity, ambiguity between braces as blocks and dictionaries, ambiguity
between angle brackets as comparisons and vectors, error recovery, and how
newlines participate in each grammar context.

The parser stage will establish a formal grammar. Until then, README examples
are design sketches and must not be described as accepted programs.
