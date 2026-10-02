---
title: Contributing
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Contributing
Contributions should preserve the boundary between implemented behavior and
language design.

Before changing code or documentation:

- use the [technology stack and change map](../implementation/technology-stack.md)
  to identify every affected compiler, tooling, test, documentation, and
  release surface;
- read the [development setup](development-setup.md);
- follow the [change and publication lifecycle](change-lifecycle.md) for
  branches, pull requests, integration, promotion, and publication;
- run the [tests](testing.md);
- follow the [documentation workflow](documentation.md);
- use the [post-1.0 documentation audit](documentation-roadmaps.md) as the
  first task after the initial stable release, leaving pages work-in-progress
  until reviewed;
- preserve the [Windows installer release gate](windows-installer-release.md);
- follow the [release lifecycle](release-lifecycle.md) for public tags and artifacts;
- record unsettled design as provisional or open rather than inventing semantics;
- preserve [licensing and attribution](licensing-and-attribution.md); and
- do not treat successful tokenization as successful parsing or execution.

Design changes should eventually be recorded through the RFC and decision
structure already present in this documentation.
