# Exact-lock workspace bootstrap relocation patch

This is a **local-only rehearsal** for the future `sagan-workspace` repository.
It is not a published split, and it must not be applied to the intact Sagan
repository. The path-only filtered base came from source preparation commit
`68d55b003709e28b4e96bdcbd22d8341018c2253` and has filtered HEAD
`47a72ef3a23421248dc2f983ead8d5548a5d473d`. Verify that base before
applying the numbered patch with `git am` in a disposable normal clone of the
filtered repository. The expected draft result is commit
`f80e3fc0de9bb94e89cc24e0c88b53a67370a392`, tree
`793efd2316744e0ed7a935084437cf76344fe2db`.

The patch moves the proposed workspace manifest to root `workspace.toml`,
activates only the already-transferred primary repository, and adds
`workspace.lock` pinned to its frozen `dev` commit
`4132c8c37f99dc5f89c7141f5780b583b64d192e`. All other repository URLs
remain `planned`; there is no claim that they exist. The Bash entry points
clone or inspect ordinary Git checkouts at exact commits without overwriting
dirty, mismatched, or wrong-origin work. The root maintainer, technology,
new-chat, and agent guides are draft split-repo instances, not approved
publication pages. The edited maintainer and technology pages have
`review-needed` metadata.

The offline Python suite passed seven cases on Windows. A real GitHub clone
into the ignored candidate `checkouts/sagan` passed at the locked SHA, and
`bash scripts/status.sh` reported clean. Bash syntax and `git diff --check`
passed. A fresh normal clone of the filtered base replayed the patch with
`git am`, matched the expected tree, passed `git fsck --full`, and reran all
seven offline tests. No split GitHub repository was created or changed, and no CI ran in a
new destination. The candidate still contains monorepo-preparation audit
scripts and archived manifests that require curation; this patch by itself
does not complete independent workspace extraction.

Before publication, re-filter the then-current reviewed `dev`, recompute or
rebase this patch, verify the exact ownership set and history, add the
remaining workspace build/test/update/lock/restore and integration functions,
run Linux/macOS/Windows clean-machine checks, complete the owner handoff and
governance drills, and refresh the lock to reviewed tested commits. The
current release and `main` promotion hold remains in force.
