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
The workspace semantic index marks installed dependency modules as external.
F2 may rename an alias declared in the current project, but it refuses to
propose source edits to an installed dependency or its exported declarations.
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
The underlying index reader checks cancellation before opening and between
rows and installed-manifest validation. Both custom package queries and
manifest dependency completion propagate cancellation instead of publishing
an incomplete package set.
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
member completion on a value whose declared type comes from an imported
package also uses the workspace's resolved type binding. Public methods carry
their compiler-indexed signatures and owning module; this is tested through
both library queries and standard LSP completion. The workspace snapshot must
match the open document version before its type binding is used. Unqualified
exported-name suggestions carry those signatures and a compiler-produced
import edit. That edit is offered only for strictly parsed documents with a
safe module-header insertion line, preserves the document's line ending, and
avoids names already declared anywhere in the document. Recovered syntax or
an unproven module-header insertion line does not receive an edit. Edits are
produced through the same versioned, previewed `add_missing_import` planner
used by code actions; a candidate is omitted when that planner cannot prove
the import resolves uniquely after insertion. These are
tested slices, not a claim that every contextual completion or import-edit
case is safe yet. When different modules export the same unqualified name,
completion keeps separate, deterministically ordered candidates with explicit
`from` imports; standard LSP `detail` names each source module. Importing a
module as a namespace does not suppress its unqualified auto-import candidate.
An exported overload set is one importable public name: completion provides
one candidate and the safe planner accepts any overload identity in that set
only when the public export and resulting imported binding are unique. Its
completion detail lists the distinct callable signatures in that set.
An incomplete `import` or `from` module path also receives
compiler-owned module suggestions
from the local source tree and locked installed dependencies, even if the
buffer cannot parse. The module discovery scan is bounded; an unavailable or
invalid index produces an error rather than invented candidates. The query
returns a standard LSP `CompletionList` capped at 256 module names;
`isIncomplete` is true when more matching names exist, so the editor can
request a narrower prefix. Local and locked dependency names retain their
deterministic order. This does
not yet cover all package export/member contexts or conflict-safe auto-import edits.
For a partial selective import such as `import orbit_a from orbit_tools.main`,
standard LSP completion now returns the selected module's explicitly exported
names, signatures, documentation, deprecation state, and replacement edit.
This path returns a standard LSP `CompletionList` with at most 256 items and
`isIncomplete` when the export set exceeds that bound.
The compiler resolves the locked dependency and reads an unsaved source overlay
before disk source. Non-exported declarations are excluded. An unreadable or
incomplete imported module reports an error rather than an invented API.
For an unavailable module, standard LSP completion returns an empty list
instead of a protocol error; structured import diagnostics remain available.
This is a tested subset of package completion, not the full `packageCompletion`
capability.
Go-to-definition on a module path in an `import` statement resolves to the
module's source URI and UTF-16 `module` declaration range, including installed
package modules. The same query honors source overlays and prefers the local
module for an unqualified name. Go-to-definition and hover on the exported
name in a selective import also resolve through the locked module's explicit
exports, even when the importing document is otherwise incomplete. Hover uses
the compiler's exported signature and documentation; a missing export has no
invented target. When an import names an unavailable module, standard LSP
definition returns an empty array rather than a broken filesystem URI or a
protocol error; the compiler's structured import diagnostic still explains the
failure. These queries honor the installed source overlay and reject
cancelled or stale document versions. This does not yet cover every package alias,
member, and source-unavailable navigation context, so `packageNavigation`
remains false.

For `sagan.toml`, go-to-definition on the quoted `[package] entry` value now
opens the corresponding local source module. The compiler parses the active
manifest buffer, so an unsaved entry change is honored; invalid manifests and
missing source files yield no guessed target. This is a manifest-document
navigation slice, not navigation for available-only packages.

Go-to-definition on a `[dependencies]` alias or its quoted inline-table
`package` name also opens the exact installed package manifest selected by the
project's lockfile and compiler compatibility rules. This uses the active
unsaved manifest text through the same offline resolver; unavailable,
incompatible, or unresolved dependencies produce no fictitious location.
The lockfile itself is read from disk in this slice. In an incomplete
`[dependencies]` key, standard LSP completion now uses the same configured
installed index and compiler-compatibility rules to suggest importable aliases.
For a package name containing a hyphen, the edit inserts an underscore alias
and the explicit `{ package, version }` form. The suggestion uses the newest
compatible installed version and never claims an available-only package is
locally usable. Inside a quoted requirement for an installed package, including
the inline-table `{ package, version }` alias form, completion can replace the
whole quoted value with the newest compatible caret requirement. Accepting a
new dependency still requires updating the project
lockfile; automatic lockfile edits remain unfinished.

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
