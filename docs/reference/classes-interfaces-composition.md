---
title: Classes, interfaces, and composition
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes, interfaces, and composition
Interfaces, spelled `face`, are Sagan's primary composition mechanism.
Interfaces may compose interfaces; classes declare conformance with `is` or
`has`; multiple interfaces are comma-separated; and `self` refers to the
current object.

This composition-first direction is **settled design**. Deep inheritance
hierarchies are not the intended reuse mechanism. A leading dot is intended to
mark a private member.

**Implemented lexically:** `face`, `class`, `is`, `has`, `self`, member
dots, and the surrounding punctuation.

**Open questions:** explicit versus structural conformance, default
implementations, conflict resolution, object construction, storage layout,
dispatch, value/reference behavior, visibility semantics, and whether limited
implementation inheritance will exist.
