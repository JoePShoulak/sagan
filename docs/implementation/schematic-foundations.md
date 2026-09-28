---
title: Schematic-derived foundations
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Schematic-derived foundations
Sagan began with compiler infrastructure from Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic).

Retained or adapted foundations include build structure, generator-backed token
iteration, tokenizer state, source spans, diagnostics, parse-error support,
parser utilities, and AST foundations. The former node-language token table and
lexer have been replaced by Sagan's token vocabulary and lexical scanner.

This distinction matters technically and legally:

- generic support code does not mean Sagan grammar parsing is implemented;
- new Sagan behavior should be identified separately from inherited machinery;
- attribution must remain visible; and
- applicable GPLv3 requirements must be honored before redistribution.

See [license and attribution](../about/license-and-attribution.md).
