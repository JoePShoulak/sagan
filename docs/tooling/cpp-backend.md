---
title: C++ backend
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# C++ backend
**Status: initial executable subset implemented.**

`bin/sagan --emit-cpp` emits C++ for the checked executable subset, and
`scripts/execution_demo.sh` compiles and runs it. The backend includes checked
numeric helpers, collections, dimensioned values, reference-counted classes,
weak class fields with optional reads,
native face interfaces, and runtime face dispatch. Windows is exercised by the
local demo and Linux by CI; broader platform guarantees remain future work.

No stable ABI, foreign-function interface, optimization strategy, or complete
deterministic compiler configuration has been selected. Native compiler
invocation remains in the demo script rather than the compiler executable.
