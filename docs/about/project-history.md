---
title: Project history
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Project history

Sagan began from Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic) compiler work. Among
the inherited ideas is dynamic in-app version numbering: Schematic constructs a
major and minor version manually and derives its patch number from the commits
after a chosen cutoff commit.

Sagan retains that playful, traceable connection between the compiler and its
Git history while adding the exact abbreviated revision, dirty-worktree
reporting, and unconditional build-time regeneration to prevent stale version
information.
