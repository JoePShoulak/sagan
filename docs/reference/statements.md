---
title: Statements
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Statements
**Settled design:** ordinary statements are newline-terminated rather than
semicolon-terminated. The tokenizer emits logical newline tokens while
suppressing continuation newlines in known contexts.

Intended statements include declarations, assignments, expression statements,
`if`/`else`, `match`/`case`, `for`/`in`, `while`, `until`,
`break`, `continue`, `return`, `yield`, and exception constructs.

**Implemented lexically:** all corresponding keywords and delimiters.

**Open questions:** exact statement grammar, where expression statements are
permitted, loop and match semantics, return/yield restrictions, unreachable-code
rules, and parser-informed newline handling. No statement AST exists yet.
