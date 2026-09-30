---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support
The version 0.1.0 VS Code extension in `editors/vscode-sagan/` associates
`.sagan` files with the language and provides a tokenizer-aligned Sagan TextMate
grammar. It covers declarations, keywords, types, literals, operators,
punctuation, comments, strings and nested interpolation, Unicode identifiers,
private members, and mutating method names.

TextMate remains lexical and cannot resolve whether braces are blocks or
dictionaries, or whether angle brackets are vectors or comparisons.

There is no parser integration, language server, IntelliSense, completion,
hover information, go-to-definition, formatter, refactoring, or semantic
diagnostics. The extension should be evaluated as basic syntax coloring only.
See [getting-started editor support](../getting-started/editor-support.md) for
installation.

Compiler-owned editor infrastructure is being prepared separately from the
extension. Track the [editor feature readiness checklist](extension-readiness.md)
and [language-server capability contract](language-server-capabilities.md).
The extension must not duplicate Sagan parsing, semantics, project rules, or
standard-library metadata.
