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
  overlays, and a compiler-owned semantic index used by the language server;
- structured, cancellable check/build/run and document/project test operations,
  generated-code source maps, and debugger metadata;
- a dedicated [Sagan language server](language-server.md) for editor clients;
- a VS Code extension with tokenizer-aligned highlighting, LSP-backed
  language features, tasks, and Test Explorer;
- offline installed-package and lockfile resolution, plus an experimental
  [debug adapter](debug-adapter-contract.md); and
- MkDocs and Mike for experimental and released versioned documentation.

There is no package registry or REPL yet. Recovered-source formatting changes
only regions the compiler can prove safe; it refuses uncertain edits. The
debug adapter is a development probe, not a supported extension feature:
reliable values, exception mapping, and packaged GDB dependencies are still
missing. Complete package completion and safe auto-import are also gated.
The [historical readiness audit](language-service-audit.md),
[contracts](language-service-contracts.md), [implementation roadmap](language-service-roadmap.md),
[formatting/edit safety guide](formatting-and-edits.md), and
[extension readiness checklist](extension-readiness.md) track what remains.
The [VS Code extension roadmap](vscode-extension-roadmap.md) separately tracks
client validation, distribution, and integrations blocked on compiler services.
