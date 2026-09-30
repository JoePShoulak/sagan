---
title: Why Sagan works this way
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Why Sagan works this way

Programming-language choices affect how code feels long before they affect the
compiler. These pages explain Sagan's major choices in ordinary language:

- [Philosophy](philosophy.md) explains the simulation-first focus, explicit
  types, composition, and visible mutation.
- [Goals and non-goals](goals-and-non-goals.md) defines what the initial language
  is trying to solve and what it deliberately leaves for later.
- [Determinism](determinism.md) explains which results Sagan promises to repeat
  and where floating point, operating systems, and future libraries set limits.
- [Native units of measure](units-of-measure.md) explains how the compiler can
  distinguish meters from seconds without storing a unit beside every number.

You can use Sagan without reading these pages first. Return to them when you
want to understand why a rule exists or what tradeoff it creates.
