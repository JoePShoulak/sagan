---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support
The version 0.3.6 VS Code extension in `editors/vscode-sagan/` associates
`.sagan` files with the language and provides a tokenizer-aligned Sagan TextMate
grammar. It covers declarations (including explicit `const` and visibly invalid
`let ALL_CAPS` names), keywords, types, literals, operators, punctuation,
comments, strings and nested interpolation, Unicode identifiers, private
members, and mutating method names.
It also contributes the Sagan logo as the default language icon for `.sagan`
files when the active VS Code file-icon theme accepts language defaults.

It discovers compatible Sagan compiler and language-server executables,
validates the versioned `sagan.language-service/1` capability response, and
exposes **Sagan: Show Tooling Status** and **Sagan: Restart Language Server**.

TextMate remains lexical and cannot resolve whether braces are blocks or
dictionaries, or whether angle brackets are vectors or comparisons.

The extension connects to the standalone [language server](language-server.md)
with VS Code's standard language client. Diagnostics, hover, completion,
signature help, navigation, symbols, semantic tokens, folding, selection
ranges, links, inlay hints, safe edits, formatting, and type/call hierarchies
are enabled according to the server's `initialize` response.
The extension also uses compiler-owned cancellable check/build/run operations
for commands and tasks, and Test Explorer uses stable IDs for explicit
`test "name" { ... }` declarations. Project-wide test runs can span modules.
Full package auto-import and a supported debugger are not advertised yet;
the extension must leave those features off rather than infer support from
the presence of experimental compiler code.
See [getting-started editor support](../getting-started/editor-support.md) for
installation.

Compiler-owned editor infrastructure is separate from the extension. Source
snapshots, diagnostics, recovering syntax, overlays, semantic identities,
position queries, and LSP transport are available. Track the [editor feature readiness checklist](extension-readiness.md)
and [language-server capability contract](language-server-capabilities.md).
The extension must not duplicate Sagan parsing, semantics, project rules, or
standard-library metadata.

After changing compiler syntax or semantics in a repository checkout, rebuild
both `sagan` and `sagan-lsp`, then run **Sagan: Restart Language Server** (or
reload the VS Code window). Reinstalling the VSIX alone does not update the
native server. Check **Sagan: Show Tooling Status** to confirm which compiler
and server paths the editor is using.

The [VS Code extension roadmap](vscode-extension-roadmap.md) records remaining
release validation, distribution, and compiler-blocked integrations.
