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
compiler for a node-based language. Schematic supplied early build structure and
foundations for source spans, diagnostics, parser utilities, syntax trees, and
code-generation helpers.

Sagan replaced Schematic's node-language tokens and lexer with its own Unicode
syntax, then grew a parser, semantic analyzer, type checker, module and package
resolver, C++ backend, runtime behavior, installer, and editor-tooling APIs.
Some supporting code still descends from Schematic and remains subject to its
GPLv3 license and attribution requirements.

One playful idea also survived: the compiler knows its development version from
Git history. Sagan extends that idea with Conventional Commits, semantic-version
impact, the abbreviated revision, and dirty-worktree reporting. This makes the
version shown by a build traceable to the source that produced it.

The detailed sequence of implementation milestones belongs in Git history and
the [Project Development](../contributing/index.md) material. The learner-facing
documentation describes the language as it works now.
