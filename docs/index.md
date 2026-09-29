---
title: Sagan Documentation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Sagan Documentation

<p align="center">
  <img src="assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="240">
</p>

Sagan is an experimental, strongly typed programming language for geometry,
astrodynamics, numerical work, and real-time simulation.

!!! info "Current implementation"
    The repository contains a complete **tokenizer and parser** for the current
    specifications, an executable-subset semantic/type checker, and an initial
    C++ backend. It executes collections, classes, private state, constructors,
    reference-counted face values, dynamic dispatch, and the documented control-flow
    and numeric subset through the native demo.

## Where to begin

- [Getting started](getting-started/index.md) explains how to build, test, and
  run the tokenizer and parser demonstrations.
- [Language tour](tour/index.md) introduces the intended language while clearly
  separating parsed syntax from unresolved semantics.
- [Lexical specification](reference/lexical-specification.md) records the
  tokenizer's implemented UTF-8, Unicode, literal, comment, and operator rules.
- [Project status](design/status.md) separates implemented behavior, settled
  design, provisional design, planned work, and open questions.
- [Implementation](implementation/index.md) describes the compiler roadmap and
  current source layout.
- [Documentation roadmaps](contributing/documentation-roadmaps.md) define the
  post-1.0 expansion and ordered language-confirmation process.

## Documentation maturity

This is the **experimental** documentation selected from the current repository.
Pages are work-in-progress unless their banner explicitly says they are
publication-ready. Content being present does not mean it has completed review.

The version selector also exposes archived documentation for each published
Sagan release. The site root defaults to the newest released documentation;
choose **experimental** when working from a clone of the live repository or
when contributing to the project.
