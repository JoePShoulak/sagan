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
- structured check/build/run library operations, generated-code source maps,
  and debugger metadata for future integrations;
- a dedicated [Sagan language server](language-server.md) for editor clients;
- an early VS Code syntax-highlighting extension; and
- MkDocs and Mike for experimental and released versioned documentation.

There is no package registry, formatter for incomplete source, debugger integration,
or REPL yet. The language server exposes the tested read-only queries and safe
edit actions, but does not provide a debugger or build/run task protocol. The [readiness audit](language-service-audit.md),
[contracts](language-service-contracts.md), [implementation roadmap](language-service-roadmap.md),
[formatting/edit safety guide](formatting-and-edits.md), and
[extension readiness checklist](extension-readiness.md) track what remains.
