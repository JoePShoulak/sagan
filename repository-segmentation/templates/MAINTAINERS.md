# Maintaining {{REPOSITORY}}

Document the repository's ownership and non-goals, prerequisites, exact Bash
bootstrap/build/test/document/release commands, manifests and locks, supported
platforms, external services, secrets without values, diagnosis, rollback, and
clean-machine recovery.

## Change workflow

Every authorized Codex change request starts from current `dev` on a dedicated
`codex/<request>` branch. Test only the affected work and refine until focused
tests pass. Commit intended paths, merge into `dev`, and run the post-merge
suite selected by the impact matrix. Reserve `main` for reviewed publication.

## Impact-based test matrix

Replace this section with exact focused and post-merge Bash commands. Do not run
unrelated executable documentation examples for a documentation-only change
that cannot affect them. Run affected examples whenever their content or
supporting behavior changes.

## Release and recovery

Record artifact production, checksums, compatibility declarations, promotion,
deployment, rollback, credential ownership, partial-failure recovery, and the
last verified owner-survivability drill.
