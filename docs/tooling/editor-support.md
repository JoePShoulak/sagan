---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support
The version 0.2.0 VS Code extension in `editors/vscode-sagan/` associates
`.sagan` files with the language and provides a tokenizer-aligned Sagan TextMate
grammar. It covers declarations (including explicit `const` and visibly invalid
`let ALL_CAPS` names), keywords, types, literals, operators, punctuation,
comments, strings and nested interpolation, Unicode identifiers, private
members, and mutating method names.

It also discovers a Sagan compiler, validates the versioned
`sagan.language-service/1` capability response, and exposes **Sagan: Show
Tooling Status** without enabling capabilities the compiler does not advertise.

TextMate remains lexical and cannot resolve whether braces are blocks or
dictionaries, or whether angle brackets are vectors or comparisons.

There is no active language server, IntelliSense, completion,
hover information, go-to-definition, formatter, refactoring, or semantic
diagnostics. The extension should be evaluated as basic syntax coloring only.
See [getting-started editor support](../getting-started/editor-support.md) for
installation.

Compiler-owned editor infrastructure is being built separately from the
extension. Source snapshots, diagnostics, recovering syntax, workspace
overlays, and stable semantic identities are available; position-based queries
and LSP transport are not. Track the [editor feature readiness checklist](extension-readiness.md)
and [language-server capability contract](language-server-capabilities.md).
The extension must not duplicate Sagan parsing, semantics, project rules, or
standard-library metadata.
