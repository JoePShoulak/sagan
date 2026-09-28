# Prompt for a Sagan documentation-planning chat

```text
I am designing a programming language named Sagan (Simulation Architecture for Geometry, Astrodynamics, and Numerics). Work in the Sagan repository and begin by reading README.md, ROADMAP.md, and the current source tree. Sagan is based on Zachary Westerman's Schematic project, so preserve the attribution and GPLv3 considerations already documented in the README.

I want to begin building professional documentation, but do not create or migrate the documentation immediately. Start by researching and recommending suitable documentation solutions for a new programming language project. Use current authoritative sources for any tools you compare.

Your first response should:

1. Identify the documentation requirements implied by Sagan's current stage and goals.
2. Compare a small, focused set of appropriate solutions, such as MkDocs with Material, Docusaurus, Sphinx, mdBook, or another option you believe is a better fit.
3. Evaluate each option for:
   - clean language-reference and tutorial organization;
   - syntax highlighting for a custom language;
   - versioned documentation;
   - search;
   - API/reference generation possibilities;
   - diagrams and mathematical notation;
   - local authoring on Windows using Bash-oriented commands;
   - GitHub-based hosting and automated deployment;
   - maintenance burden and ecosystem maturity; and
   - the ability to begin simply and grow later.
4. Recommend one solution and explain the tradeoffs.
5. Propose a documentation information architecture, including at least:
   - overview and design philosophy;
   - getting started;
   - language tour;
   - lexical specification;
   - grammar and syntax reference;
   - type system;
   - classes, interfaces, and composition;
   - standard library;
   - compiler and tooling;
   - implementation notes;
   - design decisions or RFCs;
   - examples; and
   - contributor documentation.
6. Identify which current README claims are settled decisions, provisional decisions, or unresolved questions.
7. Ask me to choose or approve a documentation solution before scaffolding it.

After I approve a solution:

- create a clean documentation structure without duplicating contradictory sources of truth;
- keep the README as a concise project introduction and route detailed material into the documentation;
- preserve all settled Sagan decisions;
- label provisional or unresolved behavior honestly;
- do not invent missing language semantics;
- establish a clear location for formal lexical and grammar specifications;
- include a process for recording future language-design decisions;
- add useful local preview and validation instructions using Bash, never PowerShell;
- verify internal links and the generated site or rendered output;
- do not deploy anything unless I explicitly request deployment;
- never create commits for me; instead, recommend a commit message and provide one Bash command that stages the relevant files, commits with that message, and pushes.

Important current design context:

- Sagan is strongly typed and simulation-focused.
- It initially transpiles to C++.
- It favors interface-based composition over inheritance hierarchies.
- Variables are mutable by default.
- Mutating method counterparts may use a trailing ! as part of the method name.
- Reference counting is the intended memory-management model.
- Exceptions use the keywords hope, unless, finally, and scream.
- The lexical design is substantially defined, but parser, semantic, runtime, and standard-library decisions remain.
- Physical units and coordinate frames are not distinguished by the initial type system.
- Parallel execution is deferred.
- Cross-platform deterministic simulation is a goal whose exact contract still needs definition.

Treat README.md as the current high-level design summary, but verify every detail against the repository before reorganizing it.
```
