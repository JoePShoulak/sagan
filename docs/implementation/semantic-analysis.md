---
title: Semantic analysis
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Semantic analysis
**Status: initial name-resolution pass implemented.**

The first semantic pass consumes the source-spanned AST and produces a printable
semantic model. It creates program and nested lexical scopes, installs built-in
type names, collects declarations, resolves identifier references, and reports
duplicate declarations or undefined names with source locations.

Functions form overload groups, while other duplicate names in one scope are
rejected. Function and lambda parameters, local declarations, loop variables,
type members, enum members, imports, exports, composition references, and
`self` participate in the current traversal. Built-in type symbols currently
include `Bool`, `Coordinate`, `Float`, `Frame`, `Int`, `String`, `Vector`, and
`Void`.

This pass deliberately does not establish initialization order, overload
signatures, match or exception-pattern binding, types, conversions, interface
conformance, mutation rules, or control-flow correctness. Top-level names are
collected before bodies are visited, so name resolution alone permits forward
and self references; later passes must decide whether those uses are valid.

The next semantic milestone is a type representation plus expression and
declaration type checking. The inference algorithm, generic model, overload
selection, and multi-error recovery strategy remain open.
