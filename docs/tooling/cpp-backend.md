---
title: C++ backend
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# C++ backend
**Status: planned; not implemented.**

The first complete compiler is intended to emit C++ and use a C++ compiler to
produce a native executable. Windows, Linux, and macOS are initial target
platforms.

No ABI, generated-code layout, compiler invocation protocol, runtime boundary,
foreign-function interface, optimization strategy, or deterministic compiler
configuration has been selected. The repository's C++ sources implement the
compiler prototype itself; they are not generated from Sagan.
