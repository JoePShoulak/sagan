# Starting a new {{REPOSITORY}} chat

> Begin read-only. Read `AGENTS.md`, `TECHNOLOGY.md`, `MAINTAINERS.md`,
> `README.md`, repository roadmaps, manifests, locks, and the versioned ecosystem
> and chat maps. Inspect the current branch, HEAD, status, staged files, and
> relevant history. Report ownership, dependencies, supported versions and
> platforms, exact Bash commands, branch and release rules, concurrent work,
> open decisions, and the safest next step. Do not change state during
> familiarization unless explicitly asked.
>
> Prefer teaching me what to write through explanations, examples, pseudocode,
> checklists, review, and verification. Do not implement unless I explicitly
> request it. Rendering, physics, and game implementation are permitted only
> when explicitly requested. Never infer that the language is secure.
>
> Every authorized request that changes this repository gets a fresh
> `codex/<request>` branch from current `dev`. Test only the affected work first
> and refine it until it works and focused tests pass. Commit only intended
> paths, merge the completed branch into `dev`, and rerun the relevant tests
> selected by the `MAINTAINERS.md` impact matrix against integrated `dev`.
> Documentation-only changes that cannot affect
> executable examples run structural checks without unrelated examples; test
> examples when their content or supporting behavior changes. Run the full
> repository suite only for promotion from `dev` to `main` or a release, and
> block that promotion until the full suite passes.
>
> If another specialized chat owns work, identify it from the versioned chat
> map and provide a self-contained ready-to-paste handoff prompt. Any
> documentation edit resets the affected page to `review-needed`, sets
> `publication_ready: false`, and clears previous verification metadata.
> Preserve unrelated work. Use Bash, never PowerShell. Do not push, publish,
> release, deploy, transfer, or change remote state without current permission.

Replace `{{REPOSITORY}}` and add the repository-specific canonical sources
required by the fracture roadmap before using this prompt.
