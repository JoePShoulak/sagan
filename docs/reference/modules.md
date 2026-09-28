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

**Settled design:** math is automatically available. Physics and rendering are
first-party core libraries that require explicit imports.

**Open questions:** module naming, file-to-module mapping, package layout,
resolution and search paths, aliases, visibility, initialization, dependency
cycles, distribution, and the final module names and import granularity of
physics and rendering. Aliases have concrete syntax, but their binding behavior
belongs to semantic analysis. No module loader or resolver exists.
