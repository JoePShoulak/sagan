---
title: Compiler and tooling
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler and tooling
Current tooling includes:

- the `bin/sagan` tokenizer and parser inspection driver;
- text, DOT, SVG, and interactive HTML AST renderers;
- Make targets for build, tests, examples, cleanup, and version display;
- GCC/LCOV coverage instrumentation with Codecov reporting in GitHub Actions;
- Bash scripts for compiler tests and documentation;
- semantic analysis, type checking, C++ generation, and direct native execution;
- package/module discovery and a self-contained Windows installer;
- a reusable compiler library, recovering document analysis, workspace
  overlays, and a compiler-owned semantic index for future editor features;
- an early VS Code syntax-highlighting extension; and
- MkDocs and Mike for experimental and released versioned documentation.

There is no package registry, complete style formatter, debugger integration,
Language Server Protocol server, or REPL yet. Read-only compiler-library queries
are available, and Phase 6 now has a conservative layout formatter and safe edit
preview. These are not editor protocol features yet. The [readiness audit](language-service-audit.md),
[contracts](language-service-contracts.md), [implementation roadmap](language-service-roadmap.md),
[formatting/edit safety guide](formatting-and-edits.md), and
[extension readiness checklist](extension-readiness.md) track what remains.
