---
title: Code generation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Code generation
**Status: planned; not implemented.**

The intended first backend translates semantically validated Sagan into C++ and
then invokes a C++ compiler to produce native executables for Windows, Linux,
and macOS.

Open work includes generated-code structure, runtime interfaces, memory
management, exception lowering, debug information, compiler selection and flags,
standard-library linkage, platform support, optimization, and deterministic
numeric constraints. No current command emits C++ or builds a Sagan program.
