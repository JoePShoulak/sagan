---
title: About Sagan
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# About Sagan

Sagan is a strongly typed, native programming language built for simulation.
Its name expands to **Simulation Architecture for Geometry, Astrodynamics, and
Numerics**.

“Strongly typed” means the compiler checks which kinds of values an operation
can use before the program runs. “Native” means Sagan currently translates a
checked program to C++ and uses a C++ compiler to create a normal executable.
The goal is to make simulation code pleasant to read without hiding important
units, types, mutation, or failure behavior.

Sagan began from Zachary Westerman's Schematic compiler and retains the required
attribution and GPLv3 licensing. Read the short [project history](project-history.md)
or the detailed [license and attribution](license-and-attribution.md).

For the language itself, begin with [Getting Started](../getting-started/index.md).
For current implementation boundaries, see [Project status](../design/status.md).
