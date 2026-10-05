---
title: Late-bound face default members checkpoint
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Late-bound `self` in face defaults

Requested behavior: a face default may read, call, or mutate an undeclared
`self.member`. At each adopting class that uses the default, the compiler must
resolve that member against the class, including its private members. An
explicit class override removes the default's dependencies. This is **not yet
implemented** and must not be advertised by editor capabilities or examples as
working behavior.

The current compiler records default signatures in
`src/semantic/type_checker.cpp` (`collect_interface_type`, `resolve_interface`,
`validate_composition`) and type-checks each face method body while `self` has
the face type. A missing member therefore fails before an adopting class is
known. The C++ generator emits a face default body into the face vtable and
also copies it into a class (`interface`, `face_defaults`, `object` in
`src/codegen/cpp_generator.cpp`). A body that directly reads a class-only
member cannot compile in the face itself. Relaxing the first check alone would
produce broken native code.

Implementation order:

1. Collect unresolved `self` accesses from a face default as structured
   dependencies with source ranges, read/call/write mode, argument/result type
   constraints, and originating face/method identity. Explicitly declared face
   members retain their existing eager resolution.
2. During class composition, select defaults after override and conflict
   resolution. Type-check each selected default in the adopting class's
   substituted generic context; allow class-private access there. Report
   missing, incompatible, ambiguous, and conflicting dependencies at the
   original access with the adoption site as a related location. Use stable
   ordering so multiple faces produce deterministic diagnostics.
3. Generate a pure virtual face slot for a late-bound default and a class
   implementation specialized to the adopter. Calls through face references
   must still dispatch to that class implementation. Preserve the current
   behavior for ordinary defaults that use only face-declared members.
4. Index each specialized dependency for hover, definition, references, and
   LSP diagnostics without pretending one face source span has a unique
   class-independent target.
5. Test direct and face-typed calls, inherited/composed faces, generic
   substitution, private reads, method calls, mutation, explicit override
   without dependencies, conflicting faces, and all negative diagnostics.
   Run focused runtime/codegen/LSP tests, then the full compiler, docs, and
   extension gates.

This work needs a distinct object-model increment. Keep it separate from the
inheritance and default-argument commit and preserve concurrent rendering,
physics, and editor-tooling changes while implementing it.
