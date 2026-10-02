# Sagan Language for VS Code

<p align="center">
  <img src="images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="180">
</p>

VS Code language support for Sagan. Version 0.3.4 connects to the compiler's tested language server and enables every editor feature it advertises.

The extension associates `.sagan` files with Sagan and provides TextMate highlighting derived from the repository's current tokenizer. It covers declarations, keywords, types, literals, operators, punctuation, comments, documentation comments, strings, interpolation, Unicode identifiers, private members, and mutating method names.

The Sagan galaxy-pie logo is registered as the default `.sagan` language icon. VS Code uses it when the active file-icon theme permits language-provided icons and does not define its own `.sagan` icon.

The extension provides TextMate highlighting plus live diagnostics, hover,
navigation, references, completion, signature help, symbols, semantic tokens,
folding, selection ranges, import links, inlay hints, safe rename and quick
fixes, recovered-source formatting, type/call hierarchies, cancellable
check/build/run commands and tasks, and Sagan Test Explorer integration through
`sagan-lsp`. VS Code registers only the capabilities advertised by the running
server.

Use **Sagan: Check**, **Sagan: Build**, or **Sagan: Run** for the active Sagan
document. The `sagan` task type also provides document and project variants.
When the server advertises the Sagan test contracts, Test Explorer discovers
tests by their compiler-issued stable IDs and can run all or selected tests.
Project test discovery is supported. When the server advertises project-wide
execution, a selection spanning several modules runs as one cancellable project
operation; older compatible servers retain document-grouped execution.

Tests are explicit Sagan declarations, for example:

```sagan
test "the answer" {
  assert(6 * 7 == 42)
}
```

The compiler, not the extension, decides which files belong to a package and
which test IDs are valid. Check/build/run and test requests use the current
document's unsaved text where the server supports an overlay.

The [demo](examples/demo.sagan) includes grouped `let` bindings, simultaneous
Fibonacci reassignment, postfix increment, a brace-free loop, `5.times`,
`PHI ^ n`, and `Int.round(...)`. The compiler and language server determine
their meaning; TextMate supplies only lexical colors before semantic tokens
arrive.
The demo calls its helper from the root file and exits explicitly; Sagan no
longer requires a `main` function.

Run **Sagan: Show Tooling Status** from the Command Palette to inspect the compiler found for the current workspace. If discovery does not find it, set `sagan.compiler.path` to the compiler executable.

The extension locates `sagan-lsp` beside the configured compiler, in the workspace `bin` directory, or on `PATH`. Set `sagan.server.path` to override discovery. Changes to server, compiler, or trace settings restart the client automatically; **Sagan: Restart Language Server** remains available for manual recovery. Optional sanitized protocol-method tracing is controlled by `sagan.server.trace`.

Installed packages and locked dependencies already have compiler-backed import
suggestions and some navigation and hover support. Complete package completion
and automatic import edits are still in development. The compiler also has an
experimental debug adapter, but it is not packaged or advertised as a
supported debugger; this extension does not offer a Sagan debug configuration
yet. The [extension roadmap](../../docs/tooling/vscode-extension-roadmap.md)
tracks those gates.

## Local development

1. Build the repository with `make all bin/sagan-lsp` so both the compiler and
   language server are available.
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
npm run test:bundle
npm run test:integration
```

The tests use VS Code's TextMate and Oniguruma engines to verify syntax scopes and mocked lifecycle tests to verify server discovery, client startup, command registration, and shutdown. `npm run test:bundle` additionally builds and checks the packaged entry point. The opt-in integration test launches a real VS Code Extension Development Host against a built `bin/sagan-lsp`; set `SAGAN_LSP_PATH` to use another compatible server.

The integration test also validates `examples/demo.sagan` with the matching
compiler and requires it to be diagnostic-free. Set `SAGAN_COMPILER_PATH` when
testing against a compiler outside the repository's `bin` directory.

On Linux, run the complete clean-build, test, integration, and packaging gate
from the repository with:

```bash
bash editors/vscode-sagan/scripts/linux-acceptance.sh
```

Pass an output path as the first argument to choose where the VSIX is written.
Automated acceptance does not replace visual verification of theme-dependent
syntax colors, file icons, hover rendering, completion UI, or editor actions.

## Package

From this directory:

```bash
npm run package
```

The resulting `.vsix` can be installed from VS Code's Extensions view or with the `code --install-extension` command.

Official Sagan releases attach the compatible VSIX and its SHA-256 checksum to
the GitHub Release. The same files are available from the HP1 download mirror.
The extension is not currently published to the Visual Studio Marketplace.
The repository [VS Code extension roadmap](../../docs/tooling/vscode-extension-roadmap.md) records remaining release validation and compiler-blocked features.

## Grammar limits

TextMate highlighting is lexical and cannot fully resolve context-sensitive constructs before semantic tokens arrive. In particular, braces may delimit blocks or dictionaries, and angle brackets may delimit vectors or serve as comparison operators. The language server supplies compiler-owned semantic validation and classifications after startup.
