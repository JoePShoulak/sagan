# Starting a new Sagan maintenance chat

Paste the following into a new chat when returning to this repository:

> Work in the Sagan repository. Begin read-only. Read `TECHNOLOGY.md`,
> `MAINTAINERS.md`, `README.md`,
> `docs/contributing/active-roadmap.md`, and
> `docs/contributing/repository-fracturing-roadmap.md` first. Inspect the current
> branch, HEAD, status, staged files, and version before proposing a change.
> Preserve unrelated and concurrent work; do not push, merge, tag, publish, or
> deploy without my current instruction. Do not transfer the primary repository
> or create split repositories without my explicit instruction and all checked-in
> transfer gates passing. The compiler and language service own
> language semantics; do not replicate them in the editor. Distinguish what
> currently works from proposed multi-repository architecture. Explain work
> in small, teaching-first milestones with a runnable example and a completion
> test. Ask me to decide language or release tradeoffs instead of silently
> choosing them. I use Bash, not PowerShell. Show the relevant repository-local
> workflow and its impact on the whole Sagan ecosystem before implementing a
> significant change. Do not write code unless I request implementation.
> For every authorized request that changes the repository, start from current
> `dev` on a new short-lived `codex/<request>` branch. Test only the affected
> work first and refine it until it works and all focused tests pass. Commit
> only intended paths, merge the completed branch into `dev`, then rerun those
> relevant tests against the integrated state. Documentation-only
> changes that cannot affect executable examples require structural docs checks
> but not every example; test affected examples whenever their content or
> supporting behavior changes. Resolve integration failures before completion.
> Run the full repository suite only when promoting `dev` to `main` or preparing
> a release; promotion is contingent on that full suite passing.

The [maintainer entry point](MAINTAINERS.md) is the navigation index for
compiler, editor, libraries, tests, documentation, release, and recovery.
This prompt is intentionally not a substitute for that guide or a claim that
the owner-survivability drill has passed.
