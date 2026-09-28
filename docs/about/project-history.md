---
title: Project history
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Project history
Sagan began from Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic), a work-in-progress
compiler for a node-based language. Schematic provided the initial build
structure and foundations for tokenizer state, spans, diagnostics, generators,
parser utilities, and AST support.

The inherited node-language token table and lexer have since been replaced by
Sagan's initial token vocabulary and tokenizer. Some supporting source files
still descend from Schematic and must retain appropriate attribution and
licensing treatment.

Development currently proceeds through five broad stages:

1. language definition — sufficiently defined for early implementation;
2. tokenizer — complete for the current lexical specification;
3. parser and syntax tree — complete for the current syntax specification;
4. semantic analysis — planned; and
5. C++ code generation — planned.

See the [implementation overview](../implementation/index.md) for the present
boundary between working code and intended architecture. Among the ideas
inherited from Schematic is dynamic in-app version numbering: Schematic
constructs a major and minor version manually and derives its patch number from
the commits after a chosen cutoff commit.

Sagan retains that playful, traceable connection between the compiler and its
Git history while adding Conventional Commit declarations for semantic-version
impact, the exact abbreviated revision, dirty-worktree reporting, and
unconditional build-time regeneration to prevent stale version information.
