# Exact-lock workspace bootstrap relocation patch

This is a **local-only rehearsal** for the future `sagan-workspace` repository.
It is not a published split, and it must not be applied to the intact Sagan
repository. The path-only filtered base came from source preparation commit
`68d55b003709e28b4e96bdcbd22d8341018c2253` and has filtered HEAD
`47a72ef3a23421248dc2f983ead8d5548a5d473d`. Verify that base before
applying the seven numbered patches with `git am` in a disposable normal clone
of the filtered repository. The expected draft result is commit
`014d5ca1da62188be2eb9a41489539eb057604eb`, tree
`8f1cc36f3cb38a4d8125cb840062dcfe88b60af9`.

The first patch moves the proposed workspace manifest to root `workspace.toml`,
activates only the already-transferred primary repository, and adds
`workspace.lock` pinned to its frozen `dev` commit
`4132c8c37f99dc5f89c7141f5780b583b64d192e`. All other repository URLs
remain `planned`; there is no claim that they exist. The Bash entry points
clone, inspect, or explicitly restore ordinary Git checkouts at exact commits
without overwriting dirty or wrong-origin work. The second patch's
restore-lock command refuses ignored-file collisions. The root maintainer, technology,
new-chat, and agent guides are draft split-repo instances, not approved
publication pages. The edited maintainer and technology pages have
`review-needed` metadata.

The third patch adds a generated combined package index for independently
checked-out physics and rendering candidates. Locked mode requires clean,
exact-revision active repos; explicit local-candidate mode does not activate
the planned GitHub remotes. A shared catalog resolved the game and passed
three physics and two Windows rendering checks. Output is root-relative,
path-validated, and never overwrites differing content without `--force`.
The fourth patch drafts offline contract CI for Linux, Windows, and macOS.
It does not assert that independent hosted CI has run or that native
rendering works outside Windows.
The fifth patch adds an ignored VS Code multi-root generator based only on
active, clean, locked checkouts. It refuses a missing or dirty child and
preserves a differing generated file unless `--force` is deliberate.
The sixth patch adds fetch-only `update` and preview-first `lock` commands.
Lock writes require an explicit flag and a clean checkout at a fetched
remote `dev` tip descended from the old pin; extended lock fields are
refused rather than silently discarded.
The seventh patch adds exact-lock build and focused-test orchestration. The
currently active primary builds compiler/LSP and runs only workspace
contracts plus package-catalog/resolution tests. Future components must
provide their own reviewed build/test scripts; none are silently skipped.

The first two patches' offline Python suite passed ten cases on Windows; the
third raised the total to fifteen. A real GitHub clone
into the ignored candidate `checkouts/sagan` passed at the locked SHA, and
`bash scripts/status.sh` and `bash scripts/restore-lock.sh` reported the
expected clean pin. Bash syntax and `git diff --check` passed. A fresh normal
clone of the filtered base replayed all seven patches with `git am`, matched
the expected tree, passed `git fsck --full`, and reran all twenty-three tests. No split
GitHub repository was created or changed, and no CI ran in a
new destination. The candidate still contains monorepo-preparation audit
scripts and archived manifests that require curation; this patch by itself
does not complete independent workspace extraction.

Before publication, re-filter the then-current reviewed `dev`, recompute or
rebase these patches, verify the exact ownership set and history, add the
remaining locked integration and hosted cross-repository functions,
run Linux/macOS/Windows clean-machine checks, complete the owner handoff and
governance drills, and refresh the lock to reviewed tested commits. The
current release and `main` promotion hold remains in force.
