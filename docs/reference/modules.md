---
title: Modules and packages
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Modules and packages

A *module* is one named source file. A *package* is a directory that groups
modules into one application and records how to build and launch it.

Sagan source files declare their qualified module name and may selectively
import an exported symbol or bind a whole module as a namespace:

```sagan
module navigation.guidance

import offset from telemetry.flight
import telemetry.flight as telemetry

fun calculate(value: Int): Int => value + offset()

export calculate as course
```

A file may contain at most one `module` declaration. It is optional only for a
loose entry file; package and imported files require it. It must precede every other
declaration. Imports and exports are top-level declarations. `export local as
public` publishes a top-level variable, function, or type under a public name;
unexported declarations remain private to the module.

The loose file named on the command line, or the package's configured `entry`
file, is the executable root. Only that file may contain executable top-level
statements. Imported modules remain declaration-only and do not run code as a
side effect of import. The entry file no longer needs `fun main()`.

## Packages

A package is a directory containing a strict `sagan.toml` manifest:

```toml
[package]
name = "mission-control"
version = "0.1.0"
source = "src"
entry = "main"

[application]
mode = "console"
```

All four `[package]` quoted-string fields are required. `[application]` is
optional. Its `mode` may be `"console"` or `"windowed"`; omission defaults to
`"console"`. Console applications show and preserve terminal output when
launched from Explorer. Windowed applications launch without a terminal, while
startup failures are written to Sagan's local diagnostic log and shown in a
native error dialog. Rendering imports do not select the application mode.
The package version uses
`MAJOR.MINOR.PATCH`. `source` must name an existing directory contained by the
package root. `entry` is a qualified module name and maps to a source path by
replacing dots with directory separators and adding `.sagan`:

```text
main                  -> src/main.sagan
navigation.guidance   -> src/navigation/guidance.sagan
telemetry.flight      -> src/telemetry/flight.sagan
```

Every declared module name must agree with this source-root-relative path.
Imports use the same deterministic mapping and cannot search outside the
package source root. Unknown manifest sections or keys, duplicate keys,
malformed values, invalid package names or versions, missing source roots, and
missing entries are errors.

`[dependencies]` entries use quoted exact or caret versions. A package name
that cannot be imported as a Sagan identifier needs an explicit alias:

```toml
[dependencies]
physics = "^1.2.3"
orbit_tools = { package = "orbit-tools", version = "^0.1.0" }
```

Set `SAGAN_PACKAGE_INDEX` to a local package index path. A package with
dependencies must have `sagan.lock` beside its manifest, pinning the exact
versions of direct and transitive dependencies. The compiler validates the
lock against the index and installed manifests before linking; it does not
download packages or rewrite the lockfile. A compiler-library index reader validates the
`sagan-package-index-v1` schema, local installed manifests, package prefixes,
and exact/caret compiler compatibility without network access. Its tab-separated
rows are `name`, `version`, `compiler requirement`, `installed|available`, and
manifest path (`-` when only available). `sagan.lock` begins with
`sagan-package-lock-v1`, then sorted tab-separated `name` and exact version
rows. The library selector can produce lockfile text, but installation and
lockfile-writing commands remain future work. Ordinary packages without
dependencies need neither index nor lockfile.

`import orbit_tools.main` uses the dependency alias to select the installed
package's `main` module. A plain `import orbit_tools` prefers a local module
of that name. Aliases are scoped to the declaring package, so transitive
dependencies do not leak into the root package's import namespace.

Loose `.sagan` files do not require a manifest. They use loose-module resolution
and the safe `console` launch default. A manifest is recommended for a named
application, especially when selecting windowed launch behavior.

Inspect or compile a package with:

```bash
bin/sagan --package examples/package
bin/sagan --emit-cpp-package examples/package build/package.cpp
make package-demo
```

Both commands accept either the package directory or its `sagan.toml` path.
When `--modules` is given a source file inside a package, Sagan discovers its
nearest ancestor manifest and applies the package source-root rules. Projects
without a manifest retain the original loose-module behavior: the entry file's
directory is the source root, and simple imports map to sibling `.sagan` files.

## Linking and visibility

Selective and whole-module imports link into one checked compilation unit.
Dependency declarations receive deterministic module-qualified identities,
aliases resolve to those identities, and dependencies are emitted before their
consumers. Namespace access exposes only exported members. Missing/private
exports and dependency cycles are rejected, including a complete cycle path.

The core math vocabulary is automatically available. Physics and rendering are
planned first-party libraries that will require explicit imports.

Mutable module-level initialization, package installation/distribution,
complete package-aware editor queries, and the final import granularity of
physics and rendering remain future work.
