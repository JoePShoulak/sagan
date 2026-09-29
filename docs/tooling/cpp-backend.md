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

`bin/sagan --emit-cpp` emits C++ for the checked executable subset. Passing a
source path directly compiles and runs it, while `--run-package` does the same
for a manifest-backed package. The backend includes checked
numeric helpers, collections, dimensioned values, reference-counted classes,
weak class fields with optional reads,
native face interfaces, and runtime face dispatch. Windows is exercised by the
local demo and Linux by CI; broader platform guarantees remain future work.

The driver uses `CXX` when set and otherwise invokes `g++`. `SAGAN_CXXFLAGS`
may replace its strict C++23 development flags. Generated source and binaries
live in a per-run temporary directory and are removed afterward; Sagan also
redirects compiler temporary files there, avoiding unwritable system temp
directories on Windows. No stable ABI, foreign-function interface,
optimization strategy, or complete deterministic compiler configuration has
been selected.
