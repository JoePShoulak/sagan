# Changelog

## Unreleased

- Add capability-gated, cancellable check/build/run commands and Sagan tasks.
- Add Test Explorer discovery and selected document test execution using the
  compiler-owned `sagan-tests-v1` contract.
- Use authoritative project-wide test execution when `testProjectRun` is
  advertised, including selections spanning multiple documents.
- Consume recovered-source formatting through the existing LSP providers.
- Use the compiler's root-file script entry model in the bundled executable
  demo; `main` is now an ordinary function name.
- Highlight every binding in a parallel `let` declaration while preserving
  individual type annotations.
- Refresh the compiler-valid demo and grammar fixture for compact Fibonacci,
  integer `.times`, floating-point exponentiation, and `Int.round`.
- Recognize `test` declarations and executable root statements in the base
  grammar used before semantic tokens arrive.

## 0.3.4

- Replace the parser-only demonstration with a compiler-valid, diagnostic-free Sagan program.
- Validate the packaged demonstration with the matching compiler during integration tests.
- Add a repeatable Linux build, integration, and packaging acceptance script.
- Record the clean Pop!_OS acceptance results and clarify that `sagan-lsp` is a separate build target.
- Expand real VS Code integration coverage for references, highlights, workspace symbols, folding, selection ranges, semantic tokens, local-variable rename, and safe entry-point rename refusal.

## 0.3.3

- Register the Sagan galaxy-pie logo as the default light and dark `.sagan` file icon.
- Verify the icon contribution and packaged asset during bundle tests.
- Exercise safe non-exported function rename through a real VS Code Extension Development Host.

## 0.3.2

- Add an opt-in real VS Code Extension Development Host integration suite.
- Exercise activation, diagnostics, hover, definition, completion, signature help, symbols, and formatting against `sagan-lsp`.
- Document server discovery, schema mismatch, native runtime, and tracing troubleshooting.

## 0.3.1

- Verify PATH-based language-server discovery before reporting success.
- Restart the language client after relevant configuration changes.
- Add a working Extension Development Host launch configuration.
- Refresh editor documentation and record the remaining extension roadmap.

## 0.3.0

- Connect to `sagan-lsp` with the standard VS Code Language Client.
- Enable diagnostics, hover, navigation, references, completion, signature help, symbols, semantic tokens, folding, selection ranges, links, hints, safe edits, formatting, and hierarchies according to server-advertised capabilities.
- Discover the server beside the compiler, in workspace `bin`, or on `PATH`, with an explicit override setting.
- Add a language-server restart command and optional sanitized server tracing.
- Add focused server-discovery and extension-lifecycle tests.
- Bundle the standard language client into the VSIX for a self-contained installation.

## 0.2.1

- Highlight `dimension`, `quantity`, `unit`, and `affine unit` declarations with contextual declaration-name scopes.
- Exercise native measurement declarations, measured annotations, suffixes, and conversions in focused fixtures and the demonstration file.

## 0.2.0

- Add capability-aware extension activation and compiler discovery.
- Validate the versioned `sagan.language-service/1` response before using compiler tooling.
- Add **Sagan: Show Tooling Status**, an output channel, and a configurable compiler path.
- Keep semantic providers disabled until the compiler advertises a language server.
- Add focused capability parsing and discovery tests.

## 0.1.2

- Highlight the current `weak` declaration modifier and `??` optional-coalescing operator.
- Recognize `None` as Sagan's absence value.
- Exercise generic declarations, payload enum cases, weak fields, and optional expressions in the grammar fixtures and demonstration file.
- Describe the compiler's language-service foundation without claiming unavailable semantic editor features.

## 0.1.1

- Align syntax scopes and the demonstration program with Sagan 0.13.0's complete tokenizer and parser grammar.
- Add contextual module, import, export, enum-member, property, parameter, and mutating-method scopes.
- Tighten Unicode identifier highlighting while retaining emoji identifiers and sequences.
- Add indentation behavior for brace-delimited bodies.
- Add TextMate tokenization tests covering the current vocabulary, strings, comments, Unicode, parser demo, and removed Schematic constructs.

## 0.1.0

- Replace the JavaScript fallback with a tokenizer-aligned Sagan TextMate grammar.
- Add Sagan declarations, keywords, literals, operators, punctuation, comments, and identifier scopes.
- Add ordinary, raw, multiline, and interpolated strings with nested Sagan expressions.
- Add recursive nested block-comment highlighting.
- Add Unicode, emoji, private-member, and mutating-method highlighting.
- Expand and package the comprehensive syntax demonstration file.

## 0.0.1

- Register the Sagan language and `.sagan` source-file extension.
- Add comment, bracket, auto-closing, surrounding-pair, and folding behavior.
- Highlight the current draft Sagan keywords over VS Code's JavaScript TextMate grammar baseline.
