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

The compiler now has a standalone [language server](language-server.md), but
this extension has not yet connected to it. In the extension, IntelliSense,
completion, hover, go-to-definition, formatting, refactoring, and live
diagnostics remain unavailable. Evaluate the current extension as basic syntax
coloring and compiler discovery only.
See [getting-started editor support](../getting-started/editor-support.md) for
installation.

Compiler-owned editor infrastructure is separate from the extension. Source
snapshots, diagnostics, recovering syntax, overlays, semantic identities,
position queries, and LSP transport are available. Track the [editor feature readiness checklist](extension-readiness.md)
and [language-server capability contract](language-server-capabilities.md).
The extension must not duplicate Sagan parsing, semantics, project rules, or
standard-library metadata.
