---
title: Modules
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Modules
The tokenizer reserves `module`, `export`, `import`, `from`, and `as`, and the
parser implements the following declaration forms:

```sagan
module orbital_demo

import math
import Vector from math
import Renderer from rendering as SceneRenderer
import physics as simulation_physics

export ExplorerShip
export GuidanceStatus as Status
```

A source file may contain at most one `module` declaration. It is optional, but
must precede every other declaration. Imports and exports are top-level
declarations. `export` names a declaration separately instead of modifying the
declaration itself.

## Initial resolver contract

The module resolver is implemented for a deliberately small, deterministic
layout. The directory containing the entry file is the source root, and each
imported module name maps to a sibling `<name>.sagan` file. Imported files must
declare the same module name as their filename:

```text
simulation/main.sagan       -> module main
simulation/guidance.sagan   -> module guidance
simulation/telemetry.sagan  -> module telemetry
```

`import value from guidance` selects a public export named `value` from
`guidance.sagan`. `as` changes the local binding name. `export local as public`
publishes a local top-level variable, function, or type under `public`.
Selecting a private or missing name is an error.

Selective imports are linked into a single checked compilation unit. Each
dependency's top-level declarations receive a deterministic module-qualified
identity before name and type analysis, preventing private declarations in
different modules from colliding. Imported aliases resolve to those linked
identities. The C++ backend then emits dependency declarations before their
consumers, allowing cross-module functions and types to execute natively.

`import guidance` loads the whole module and reserves `guidance` (or its `as`
alias) as the future module-namespace binding. Namespace member lookup is not
semantically linked yet; executable cross-file code currently uses selective
imports. Dependencies are resolved transitively in dependency
order, and cycles are rejected with the complete cycle path.

Inspect the resolved graph with:

```bash
bin/sagan --modules examples/module_demo/main.sagan
bin/sagan --emit-cpp-modules examples/module_demo/main.sagan build/modules.cpp
```

**Settled design:** math is automatically available. Physics and rendering are
first-party core libraries that require explicit imports.

**Open work:** namespace member access, mutable module-level initialization,
hierarchical package layout, additional search paths, distribution, and the final module names and import
granularity of physics and rendering. The flat filename mapping is the settled
initial behavior, not yet a complete package system.
