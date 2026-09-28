---
title: Standard library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Standard library
Sagan's intended core ecosystem has three layers:

1. **Math** is built in and automatically available.
2. **Physics** is a tightly integrated, first-party core library requiring an import.
3. **Rendering** is a tightly integrated, first-party core library requiring an import.

Math is intended to provide the shared scalar, vector, matrix, quaternion,
coordinate, and numerical vocabulary used by the language and other core
libraries. Explicit imports keep physics and rendering from imposing cost on
programs that do not need them.

This architecture is **settled design**. Concrete APIs, module names, package
layout, import granularity, and implementations are not finalized.

Sagan's planned core libraries are math, physics, and rendering. They are
designed together around shared numerical and geometric types, but they do not
all have the same inclusion policy.

## Math: automatically available

Core math is built into the language environment and available without an
import. It supplies the numerical vocabulary on which Sagan programs and the
other core libraries rely, including planned vector, matrix, quaternion,
coordinate, and related operations.

## Physics: explicit core library

Physics is a first-party core library, tightly coupled to Sagan's math types and
designed for its simulation use cases. Programs must import it explicitly. This
keeps the physics runtime and API out of applications that only need math or
other lightweight language facilities.

## Rendering: explicit core library

Rendering follows the same model as physics: first-party, deeply integrated
with Sagan's shared math and simulation vocabulary, and explicitly imported.
Programs that do not render should not incur its dependencies or runtime costs.

The exact public APIs, module names, package subdivision, and linking behavior
remain to be designed. The inclusion hierarchy itself is settled:

```text
automatically available: math
explicit core imports:   physics, rendering
```
