---
title: Compiler and tooling
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Compiler and tooling
Current tooling is intentionally small:

- the `bin/sagan` tokenizer demonstration;
- Make targets for build, tests, examples, cleanup, and version display;
- GCC/LCOV coverage instrumentation with Codecov reporting in GitHub Actions;
- Bash scripts for compiler tests and documentation;
- an early VS Code syntax-highlighting extension; and
- MkDocs for internal project documentation.

There is no package manager, formatter, debugger integration, language server,
REPL, semantic linter, or completed compiler driver.
