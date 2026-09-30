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
- an early VS Code syntax-highlighting extension; and
- MkDocs and Mike for experimental and released versioned documentation.

There is no package registry, formatter, debugger integration, language server,
REPL, or reusable editor-facing language-service layer yet. The
[readiness audit](language-service-audit.md), [proposed contracts](language-service-contracts.md),
[implementation roadmap](language-service-roadmap.md), and
[extension readiness checklist](extension-readiness.md) define the work needed
without duplicating compiler logic in an editor extension.
