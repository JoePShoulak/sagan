---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support

Sagan provides a VS Code extension that associates `.sagan` files with the
language, supplies tokenizer-aligned TextMate highlighting, and discovers a
compatible Sagan compiler and language server. The extension has its own version number; the VSIX
attached to a Sagan release is built from that same tagged source and is the
recommended match for the release.

There is no Visual Studio Marketplace listing yet. Install the extension from a
release asset or build it from source.

## Install from a Sagan release

1. Open the [HP1 download mirror](https://sagan.shoulak.org/downloads/) or the
   canonical [GitHub Releases](https://github.com/JoePShoulak/sagan/releases)
   page.
2. Select the same Sagan release as the compiler you installed.
3. Download `sagan-language-EXTENSION_VERSION.vsix` and its `.sha256` file.
4. In VS Code, open **Extensions**, select the `...` menu, choose
   **Install from VSIX...**, and select the downloaded file.

Alternatively, install the downloaded asset from Bash:

```bash
code --install-extension sagan-language-VERSION.vsix
```

## Build and install from a repository clone

Use this route when working with Sagan's experimental branch:

```bash
cd editors/vscode-sagan
npm ci
npm test
npm run test:bundle
npm run test:integration
npm run package
code --install-extension sagan-language-*.vsix
```

The repository's extension version is currently 0.3.3. Its version is
independent of the compiler version.

## Verify the installation

1. Open a `.sagan` file and confirm VS Code identifies it as **Sagan**.
2. Confirm comments, declarations, strings, interpolation, keywords, and types
   receive syntax coloring.
3. Confirm `.sagan` files show the Sagan logo when the active file-icon theme
   supports language-provided default icons. A theme-specific icon or generic
   icon may override this contribution.
4. Run **Sagan: Show Tooling Status** from the Command Palette.
5. Confirm the **Sagan** output channel reports that the language server started.
6. Try completion, hover, go to definition, diagnostics, formatting, and rename.
7. If discovery fails, set `sagan.compiler.path` and `sagan.server.path` to the
   matching executables. Relevant setting changes restart the server automatically.

The extension locates the compiler and [language server](../tooling/language-server.md),
then enables only the capabilities advertised during LSP initialization.
TextMate coloring remains lexical and is not proof that code parses or
type-checks. The VSIX does not embed the native language-server executable; a
matching Sagan installation must provide `sagan-lsp` on `PATH`, beside the
configured compiler, in the repository `bin` directory, or through
`sagan.server.path`.

## Troubleshooting

- Open **View: Output**, choose **Sagan**, and look for the resolved server path
  or the first startup error. **Sagan: Restart Language Server** retries startup.
- If discovery fails, configure absolute paths for `sagan.compiler.path` and
  `sagan.server.path`; do not point both settings at the same executable.
- An unsupported `sagan.language-service/1` schema means the extension and
  native Sagan installation are from incompatible releases. Install matching
  artifacts rather than bypassing the check.
- On Windows, a missing runtime DLL or an execution-quarantine warning belongs
  to the native Sagan installation, not the JavaScript extension. Reinstall the
  matching official compiler/server package and verify its checksum.
- Enable `sagan.server.trace` only while diagnosing protocol lifecycle issues.
  It logs sanitized method names to stderr and never source payloads.

## Development-host testing

Open `editors/vscode-sagan/` in VS Code, press `F5`, and choose **Run Sagan
Extension** if prompted. The Extension Development Host opens the bundled
demonstration file. This is intended for extension development, not ordinary
installation.

The opt-in `npm run test:integration` command downloads or reuses a supported
VS Code test runtime and requires a built `bin/sagan-lsp` (or an explicit
`SAGAN_LSP_PATH`). Ordinary `npm test` remains offline and deterministic.
