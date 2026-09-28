---
title: Modules
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Modules
The tokenizer reserves `module`, `export`, `import`, `from`, and `as`.

**Settled design:** math is automatically available. Physics and rendering are
first-party core libraries that require explicit imports.

**Provisional design:** source files can declare modules, import selected names,
and export declarations.

**Open questions:** module naming, file-to-module mapping, package layout,
resolution and search paths, aliases, visibility, initialization, dependency
cycles, distribution, and the final module names and import granularity of
physics and rendering. No module loader or resolver exists.
