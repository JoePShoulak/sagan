# Sagan Language for VS Code

<p align="center">
  <img src="images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="180">
</p>

VS Code language support for Sagan. Version 0.3.0 connects to the compiler's tested language server and enables every editor feature it advertises.

The extension associates `.sagan` files with Sagan and provides TextMate highlighting derived from the repository's current tokenizer. It covers declarations, keywords, types, literals, operators, punctuation, comments, documentation comments, strings, interpolation, Unicode identifiers, private members, and mutating method names.

The extension provides TextMate highlighting plus live diagnostics, hover, navigation, references, completion, signature help, symbols, semantic tokens, folding, selection ranges, import links, inlay hints, safe local rename and quick fixes, formatting, and type/call hierarchies through `sagan-lsp`. VS Code registers only the capabilities advertised by the running server.

Run **Sagan: Show Tooling Status** from the Command Palette to inspect the compiler found for the current workspace. If discovery does not find it, set `sagan.compiler.path` to the compiler executable.

The extension locates `sagan-lsp` beside the configured compiler, in the workspace `bin` directory, or on `PATH`. Set `sagan.server.path` to override discovery, and use **Sagan: Restart Language Server** after changing the configured executable. Optional sanitized protocol-method tracing is controlled by `sagan.server.trace`.

## Local development

1. Build the repository so `bin/sagan-lsp` is available.
2. Run `npm ci` and `npm run build` in `editors/vscode-sagan`.
3. Open the Sagan repository in VS Code.
4. Press `F5` and choose **Run Sagan Extension** if prompted.
5. The Extension Development Host opens `examples/demo.sagan` from this extension.
6. Confirm the status bar identifies the file as **Sagan** and the Sagan output channel reports that the language server started.

TextMate grammar or bundled-client changes require rebuilding and restarting the Extension Development Host.

## Test

Install the pinned development dependencies and run the grammar against focused fixtures plus the repository's comprehensive parser demo:

```bash
npm ci
npm test
```

The tests use VS Code's TextMate and Oniguruma engines to verify syntax scopes and mocked lifecycle tests to verify server discovery, client startup, command registration, and shutdown. `npm run test:bundle` additionally builds and checks the packaged entry point.

## Package

From this directory:

```bash
npm run package
```

The resulting `.vsix` can be installed from VS Code's Extensions view or with the `code --install-extension` command.

Official Sagan releases attach the compatible VSIX and its SHA-256 checksum to
the GitHub Release. The same files are available from the HP1 download mirror.
The extension is not currently published to the Visual Studio Marketplace.

## Grammar limits

TextMate highlighting is lexical and cannot fully resolve context-sensitive constructs before semantic tokens arrive. In particular, braces may delimit blocks or dictionaries, and angle brackets may delimit vectors or serve as comparison operators. The language server supplies compiler-owned semantic validation and classifications after startup.
