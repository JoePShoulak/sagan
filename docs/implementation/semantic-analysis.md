---
title: Semantic analysis
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Semantic analysis
**Status: planned; not implemented.**

Semantic analysis will determine whether a parsed program is meaningful. Planned
responsibilities include name and scope resolution, duplicate-name detection,
type inference and checking, lossless-conversion validation, overload
resolution, interface conformance, mutation rules, control-flow checks, and
semantic diagnostics.

Semantic analysis is the next implementation stage now that the current parser
grammar is complete. Exact passes, symbol-table structure, type representation,
inference algorithm, generic model, and semantic error-recovery strategy remain
open.
