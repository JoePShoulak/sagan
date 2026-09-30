---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support
The repository contains an early VS Code extension under
`editors/vscode-sagan/` and a packaged `.vsix` file.

Version 0.1.0 associates `.sagan` files with Sagan and uses a
tokenizer-aligned TextMate grammar for declarations, keywords, literals,
operators, punctuation, comments, strings and interpolation, Unicode
identifiers, private members, and mutating method names.

This is lexical coloring only: it colors text according to token-shaped
patterns. There is no language server, IntelliSense, parser-backed validation,
formatting engine, refactoring, or semantic highlighting yet. The compiler now
contains reusable document, diagnostic, workspace, and semantic-index
foundations for those later features, so the extension will not need to
reimplement Sagan's rules.

Run the extension in an Extension Development Host with `F5`, or package it from
`editors/vscode-sagan/` with `npx --yes @vscode/vsce package` and install the
resulting VSIX. Treat coloring as an editing aid, not as proof that a construct
is supported by the compiler.
