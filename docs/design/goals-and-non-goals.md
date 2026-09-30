---
title: Goals and non-goals
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Goals and non-goals

## What Sagan is trying to do

Sagan aims to:

- make geometry, units, astrodynamics, numerical work, physics, and rendering
  natural to express;
- catch incompatible types and units before a program runs;
- favor small composable interfaces, called `face`s, over deep class trees;
- make mutation recognizable without forcing every value to be immutable;
- produce native programs by translating checked Sagan to C++;
- give exact core operations predictable, documented behavior; and
- grow into a practical simulation platform without forcing physics or
  rendering into small command-line programs.

## What 1.0 deliberately does not try to do

The initial language does not provide parallel execution, bitwise operators,
binary/octal/hexadecimal literals, coordinate-frame types, direct C or C++
interoperability, or cross-platform installers. Physics and rendering are not
automatically included.

Sagan also does not promise that floating-point simulations produce identical
bits on every processor. The narrower guarantee is explained in
[Determinism](determinism.md).

## What comes after the language core

Math is the always-available core library. Rendering and physics will be
closely integrated but explicitly imported. Their APIs, coordinate-frame model,
and numerical tolerances will be designed after the language release rather
than guessed in advance.
