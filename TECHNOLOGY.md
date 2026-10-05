# Sagan technology overview

This repository is currently the Sagan monorepo. It contains the language
toolchain, editor integration, first-party physics and rendering packages,
documentation, distribution machinery, and cross-component examples. The
[repository fracture roadmap](docs/contributing/repository-fracturing-roadmap.md)
defines how those responsibilities will move into separately versioned
repositories. Until an extraction passes its migration gate, this repository
remains canonical.

## System shape

```text
Sagan source
    |
    v
compiler and semantic services ----> LSP / DAP ----> VS Code extension
    |
    v
generated C++ ----> native toolchain ----> executable
    ^
    |
package resolver ----> math / physics / rendering packages

component documentation ----> MkDocs assembly ----> official documentation
release metadata -----------> Windows artifacts and release mirrors
```

The compiler, CLI, language server, and debug adapter live under `src/`. The
compiler owns syntax and semantics; editor clients consume its protocols rather
than implementing a second parser or type system. C++23 is the implementation
language, and generated C++ is compiled by the platform-native toolchain.

First-party Sagan packages live under `libraries/`. Math and units are
language-owned foundations. Physics is an explicit package that depends on the
language but not rendering. Rendering is an explicit package with a native C++
bridge and does not depend on physics. Cross-component applications coordinate
these packages without making either library application-specific.

The VS Code extension under `editors/vscode-sagan/` is a JavaScript client for
compiler-owned LSP, DAP, formatting, testing, and package metadata. Its grammar
provides lexical presentation only; compiler diagnostics remain authoritative.

## Important flows

1. The tokenizer, parser, semantic analyzer, and type checker validate Sagan.
2. The C++ generator emits native source and source mappings.
3. The native runner invokes the external compiler and executes the result.
4. The package resolver reads `sagan.toml`, the catalog, and lock information.
5. LSP and DAP expose compiler and debugger behavior to editor clients.
6. Tests exercise individual layers and end-to-end generated programs.
7. Documentation sources are checked, assembled, and published separately from
   product releases.

## Generated and external material

`obj/`, `bin/`, `build/`, extension bundles, VSIX files, and documentation site
output are generated artifacts. `third_party/` contains vendored dependencies.
Release artifacts and installed toolchains are outputs or external state, not
canonical source. Exact operational commands and recovery procedures belong in
[MAINTAINERS.md](MAINTAINERS.md).

## Repository-fracture boundaries

The planned repositories are `sagan`, `sagan-vscode`, `sagan-physics`,
`sagan-render`, `sagan-workspace`, `sagan-docs`, and `sagan-space-game`.
The intact `sagan` repository moves to `Sagan-Shoulak` before any of the other
six repositories is created, so extraction work begins inside the permanent
organization boundary. That transfer changes hosting and integration state,
not source ownership; extraction remains a later gated operation.
Component repositories own their implementation and documentation sources;
the workspace locks tested commits together; the documentation repository
assembles approved exports into one site. Released and workspace-locked modes
must both work, and sibling source paths must never become undeclared runtime
dependencies.

See the [concise execution plan](docs/contributing/repository-fracturing-execution.md)
for sequencing and `repository-segmentation/inventory.tsv` for the initial
ownership inventory. The primary transfer surface and recovery procedure are
recorded in `repository-segmentation/primary-transfer.toml` and
`repository-segmentation/primary-repository-transfer.md`.
