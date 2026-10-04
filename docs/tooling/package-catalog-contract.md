---
title: Package catalog contract
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Package catalog contract

The compiler owns a local, offline package index and an installed-source
catalog. Editors should not crawl package directories or parse manifests.
Installed packages can now be imported through a locked dependency alias.
The complete contextual completion contract is still unfinished, so
`packageCompletion`, `packageNavigation`, and `packageAutoImport` remain false.

A compiler library selector accepts a project manifest, local index,
compiler version, and optional `sagan.lock`. Its lockfile format is
`sagan-package-lock-v1` on the first line, followed by sorted tab-separated
`name<TAB>exact.version` rows. It chooses the highest compatible installed
version without a lock and exact pinned versions with one, traverses
transitive dependencies, and returns explicit missing, incompatible,
unavailable, conflict, invalid-index, or invalid-lock states. It performs no
network access or project writes. Normal project resolution and linking use
this selector and require `sagan.lock` when dependencies are declared.
`SAGAN_PACKAGE_INDEX` points to the local index. Editors must not generate
or parse the lockfile themselves.

Manifest dependencies use either `physics = "^1.2.3"` or an import-safe alias
for a hyphenated package:

```toml
[dependencies]
orbit_tools = { package = "orbit-tools", version = "^0.1.0" }
```

`import orbit_tools.main` then refers to the installed dependency's `main`
module. Unqualified `import orbit_tools` prefers a local workspace module;
the qualified alias is explicit. Transitive package imports resolve within
their own package's dependency context, not the consumer's aliases.

The library query `query_package_catalog` in
`src/language_service/package_catalog.hpp` returns
`sagan-package-catalog-v1`. It accepts an index path, compiler version, name
prefix, result limit, and cancellation token. It distinguishes index
`ready`, `unavailable`, and `invalid`; cancellation discards partial results.
The index format itself remains `sagan-package-index-v1`.

Each result has a stable `name@version` identity, compatibility requirement
and evaluated compatibility, install state, and manifest URI. For installed
packages whose source resolves, the compiler's workspace semantic index
provides module names and real exported declarations: symbol identity,
public/local names, kind, callable signature, summary documentation,
deprecation flag, source URI, and UTF-16 range. The package's `error` field
explains why an installed source tree could not be indexed; it is not silently
treated as an empty API. Available-only rows have no source-derived exports.
This version does not claim complete nested-member, generic-constraint,
transitive-resolution, or documentation-location metadata.

Standard LSP completion, hover, and definition use compiler-owned workspace
symbols for *already imported* installed package declarations. Namespace
completion uses public export names (including aliases) and semantic-index
callable signatures rather than private implementation names. Unqualified
exported-name suggestions carry those signatures and a compiler-produced
import edit. These are tested slices, not a claim that every contextual
completion or import-edit case is safe yet. An incomplete `import` or `from`
module path also receives compiler-owned module suggestions
from the local source tree and locked installed dependencies, even if the
buffer cannot parse. The module discovery scan is bounded; an unavailable or
invalid index produces an error rather than invented candidates. This does
not yet cover all package export/member contexts or conflict-safe auto-import edits.
For a partial selective import such as `import orbit_a from orbit_tools.main`,
standard LSP completion now returns the selected module's explicitly exported
names, signatures, documentation, deprecation state, and replacement edit.
This path returns a standard LSP `CompletionList` with at most 256 items and
`isIncomplete` when the export set exceeds that bound.
The compiler resolves the locked dependency and reads an unsaved source overlay
before disk source. Non-exported declarations are excluded. An unreadable or
incomplete imported module reports an error rather than an invented API.
This is a tested subset of package completion, not the full `packageCompletion`
capability.
Go-to-definition on a module path in an `import` statement resolves to the
module's source URI and UTF-16 `module` declaration range, including installed
package modules. The same query honors source overlays and prefers the local
module for an unqualified name. Go-to-definition and hover on the exported
name in a selective import also resolve through the locked module's explicit
exports, even when the importing document is otherwise incomplete. Hover uses
the compiler's exported signature and documentation; a missing export has no
invented target. These queries honor the installed source overlay and reject
cancelled or stale document versions. This does not yet cover every package alias,
member, and source-unavailable navigation context, so `packageNavigation`
remains false.

The thin LSP request is `sagan/packages/catalog`:

```json
{"indexUri":"file:///path/to/index.tsv","prefix":"orbit","limit":100}
```

The response contains `schema`, `state`, `message`, and `packages`; each
package contains `modules`, each module contains `exports`. The request
inherits standard LSP cancellation. `limit` must be between 0 and 1000;
the default is 100. Omitting `indexUri` uses `SAGAN_PACKAGE_INDEX`, and a
missing index returns `unavailable`. Neither this request nor the existing
`sagan/packages/query` should be used as a substitute for standard LSP
completion.
