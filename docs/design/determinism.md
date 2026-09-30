---
title: Determinism
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Determinism

Sagan defines a **deterministic hypercore profile**. *Deterministic* means that
the same program, compiler version, configuration, and explicit inputs produce
the same language-level result. This promise is deliberately narrower than
claiming that every simulation produces identical bits on every machine.

For identical source, package configuration, explicit program inputs, and Sagan
compiler version, the following implemented operations have deterministic
language results on supported targets:

- booleans, strings, enums, optionals, and checked signed integer operations;
- arrays, fixed-size vectors and points, whose elements are evaluated and
  retained in source order;
- dictionary construction and lookup, including source-ordered spread
  replacement (dictionary iteration is not implemented);
- source-ordered control flow, matching, exception handlers, and cleanup;
- deterministic module resolution and Unicode/emoji identifier encoding; and
- reference-counted class/face identity and shared closure captures, excluding
  the timing of otherwise unobservable destruction.

Integer overflow, invalid integer exponentiation, zero division/remainder,
out-of-range indexing, and missing dictionary keys raise defined nominal
`RuntimeError` cases instead of inheriting undefined C++ behavior.

The deterministic profile does **not** promise cross-platform bit identity
for floating-point arithmetic, NaN payloads, signed zero, future transcendental
math, host/compiler diagnostics, filesystem or process behavior, allocation
addresses, wall-clock timing, or destruction timing. Programs whose result
depends on the evaluation order of separate side-effecting call arguments are
also outside the profile until the backend sequences every argument explicitly.
Console newline encoding is a host presentation detail; the logical printed
lines and UTF-8 text are the language result.

Concurrency, random-number sources, clocks, networking, and unsafe/foreign
interfaces are absent from the hypercore. When introduced, each must define
its own reproducibility contract before it can participate in deterministic
mode. The core math, physics, and rendering libraries will separately specify
whether floating operations are bit-exact, implementation-pinned, or governed
by documented numerical tolerances.

This contract guarantees reproducible semantics for exact hypercore programs;
it does not claim reproducible binaries, a stable C++ ABI, or identical native
toolchain output.
