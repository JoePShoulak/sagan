---
title: Technology stack and change map
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Technology stack and change map

Sagan is not one executable. It is a compiler front end, native C++ backend,
package resolver, reusable language service, Language Server Protocol process,
VS Code client, documentation site, release pipeline, and hosted download
surface. A language change can cross several of those boundaries even when its
implementation begins in one file.

This page is the maintainer's map of those parts. It describes the current
repository, not a promise that every experimental component is release-ready.

## End-to-end system

```text
Sagan source
  -> UTF-8 and Unicode handling
  -> tokenizer and lossless syntax
  -> parser and source-spanned AST
  -> name resolution and semantic index
  -> type, ownership, units, and control-flow checks
  -> C++ generation and Sagan-to-C++ source map
  -> system C++ compiler
  -> native executable

VS Code
  -> JavaScript extension and TextMate grammar
  -> vscode-languageclient
  -> sagan-lsp over framed JSON-RPC/LSP on stdio
  -> the same C++ language-service and compiler library used by the CLI
```

The central maintenance rule is that the C++ compiler owns Sagan syntax and
meaning. The extension presents compiler results; it must not grow a second
parser, type checker, package resolver, or built-in API catalog in JavaScript.

## Repository layers

| Layer | Technology and entry points | Responsibility | Changes that commonly accompany it |
| --- | --- | --- | --- |
| Source model | C++23, `src/source/`, `src/diagnostics/` | Immutable/versioned documents, byte ranges, UTF-16 positions, overlays, cancellation, and structured diagnostics | CLI rendering, LSP conversion, stale-result tests, cross-file provenance |
| Lexical front end | C++23, generated Unicode 17 tables, uni-algo, `src/parser/lex.*`, `tokenizer.*` | UTF-8 validation, normalized identifiers, emoji sequences, comments, literals, operators, and logical newlines | Token names, TextMate grammar, lexical reference, tokenizer fixtures and highlighting tests |
| Syntax front end | Hand-written recursive-descent C++ parser, `src/parser/`, `src/syntax/` | Strict ASTs plus recovering, trivia-preserving syntax for editor use | Grammar/reference docs, AST renderers, formatter, recovery tests, editor folding and selection |
| Semantic system | C++23, `src/semantic/` | Scopes, identities, resolution, types, overloads, generics, faces/classes, ownership, units, and control flow | Diagnostics, semantic index/query results, hover/completion/navigation, code-generation preconditions |
| Modules and packages | C++23, `src/modules/` | Module graphs, manifests, exports, local package index, aliases, and exact lockfile validation | CLI project behavior, workspace invalidation, package catalog, completion/navigation, release compatibility |
| Native backend | C++23, `src/codegen/`, `src/driver/` | Checked C++ emission, runtime helpers, source maps, native compiler invocation, process execution | Generated-code tests, runtime fixtures, compiler/toolchain packaging, mapped build/runtime diagnostics |
| Batch tools | C++ executables `bin/sagan`, `bin/sagan-lsp`, and experimental `bin/sagan-dap` | CLI compilation/inspection, editor protocol server, and experimental debug transport | Command docs, capability negotiation, packaging, protocol and process tests |
| Shared language service | C++23, `src/language_service/` | One compiler-owned API for strict/recovering analysis, workspace state, queries, formatting, refactors, operations, tests, and package information | CLI JSON contracts, LSP handlers, extension commands, schema/version tests |
| Language server | C++23, `src/lsp/`, JSON-RPC 2.0 and LSP over stdio | Converts editor URIs and UTF-16 positions, synchronizes documents, publishes diagnostics, and exposes negotiated features | Capability docs, reliability tests, cancellation/version behavior, client integration tests |
| VS Code extension | JavaScript bundled with esbuild; `vscode-languageclient`; TextMate grammar tested with `vscode-textmate` and `vscode-oniguruma` | Finds compatible binaries, launches the server, exposes commands/tasks/tests, and provides lexical coloring | `package.json` contributions, extension operations, grammar fixtures, unit/bundle/Extension Host tests, VSIX versioning |
| Documentation | Markdown, MkDocs, Material for MkDocs, mike, Pygments, Mermaid | Experimental and immutable released documentation, local search, status metadata, executable examples | Navigation, page status, strict links, version aliases, HP1 deployment |
| Build and release | GNU Make, Bash, GCC/Clang, MSYS2 UCRT64, Inno Setup, npm/vsce, GitHub Actions | Builds and tests binaries/docs/VSIX, creates Windows assets and checksums, and publishes approved releases | Version derivation, manifests/SBOM, signing policy, portable/installer/VSIX smoke tests |
| Hosting and quality | GitHub, Codecov, HP1 self-hosted runner and web origin | Source and release record, coverage reporting, versioned docs, and verified asset mirror | Workflow permissions, deployment scripts, TLS/routing, rollback and mirror verification |

Schematic-derived generator, span, diagnostic, parse-error, and parser-support
foundations remain GPLv3-provenanced as described in the
[attribution documentation](../about/license-and-attribution.md).

## Contracts between parts

These boundaries are easy to break accidentally:

- **Bytes versus editor positions:** compiler spans use UTF-8 byte offsets;
  LSP uses UTF-16 line/character positions. Conversions belong in the shared
  source/LSP layers and must be tested with non-ASCII text.
- **Strict versus recovering trees:** native compilation requires a valid
  strict AST. Editor features may operate on recovering syntax but must label
  incomplete or stale results rather than treating them as executable code.
- **Compiler versus extension:** `sagan.language-service/1`, `sagan-lsp/1`,
  and the LSP `initialize` response determine what the client enables.
- **Source versus generated C++:** generated names and source maps must retain
  enough Sagan identity for useful build errors and debugging.
- **Disk versus overlays:** an editor's unsaved document wins over disk, and a
  changed import invalidates dependent workspace results.
- **Language versus packages:** compiler/language compatibility, package
  versions, manifests, and exact lock pins are separate contracts.
- **Experimental versus released docs:** `experimental` follows development;
  a numbered documentation version is an immutable snapshot of a release.

## Change-impact checklist

Use the rows that match a change; not every change requires every row.

| If you change... | Recheck and update... |
| --- | --- |
| Tokens, keywords, operators, literals, or identifier rules | tokenizer and token names; Unicode behavior; TextMate grammar; lexical and grammar docs; syntax fixtures; extension grammar tests |
| Grammar or AST nodes | strict and recovering parsers; spans/trivia; all AST renderers; formatter; folding/selection; grammar/reference pages; parser and recovery tests |
| Names, types, generics, classes, faces, units, or ownership | analyzer/type checker/index; diagnostics; queries and refactors; backend lowering; language tour/reference/design docs; positive and negative semantic/runtime fixtures |
| A built-in type, member, or runtime error | semantic metadata; hover/completion/signatures; code generation/runtime helper; diagnostics; reference docs; CLI, LSP, and extension integration tests |
| Modules, exports, manifests, or packages | resolver/index/lockfile; overlay invalidation; CLI project commands; package catalog; import completion/navigation; compatibility and release docs |
| A language-service result or schema | C++ API and tests; CLI JSON surface; LSP conversion/advertisement; extension capability handling; contract documentation |
| An LSP feature | server handler; UTF-16 and stale/cancel behavior; capability response; protocol/reliability tests; extension registration and Extension Host tests |
| Extension behavior | source and esbuild bundle; `package.json`; unit, bundle, and integration tests; extension version/changelog; install and release docs |
| Native output or toolchain assumptions | generated C++; source map; GCC and Clang builds; isolated runtime tests; installer/portable contents; SBOM and release notes |
| A user-visible example | executable `.sagan` source and adjacent `.stdout`; the page embedding it; `bash scripts/docs.sh check` |

## Minimum validation routes

Run the broad compiler and documentation gates from Bash:

```bash
bash scripts/test.sh
bash scripts/docs.sh check
```

For extension-facing work, also run:

```bash
cd editors/vscode-sagan
npm ci
npm test
npm run test:bundle
npm run test:integration
```

The Extension Host integration test requires a built `bin/sagan` and
`bin/sagan-lsp`. Linux CI supplies a virtual display; Windows and macOS run the
host directly. Coverage, release packaging, installer isolation, and HP1
deployment have separate gates documented under [contributing](../contributing/index.md).
