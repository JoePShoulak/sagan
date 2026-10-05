# Owner drill before the preparation branch merges

This drill is a **read-only transfer-start rehearsal**, not permission to
transfer the repository. The project owner performs it from Git Bash in a
fresh checkout of the committed preparation branch. It verifies that the
instructions and local backup can be used without this chat or its workspace
state. Do not run it against an uncommitted branch.

## Fresh checkout and focused checks

Choose a unique temporary directory and clone from the current local checkout.
This preserves the unmerged preparation branch without pushing it:

```bash
drill_root="$(mktemp -d /tmp/sagan-owner-drill.XXXXXX)"
git clone --no-local --branch codex/repo-segmentation-manifests /c/Users/joeps/coding/sagan "$drill_root/sagan"
cd "$drill_root/sagan"
git status --short --branch
git rev-parse HEAD
bash scripts/repository_segmentation_check.sh
bash scripts/docs.sh setup
bash scripts/docs.sh check-structure
```

Read `MAINTAINERS.md`, `TECHNOLOGY.md`, this guide, and
`repository-segmentation/primary-repository-transfer.md`. Without referring
back to a Codex answer, identify the source and destination repositories, the
release hold, the transfer-freeze sequence, and the conditions that stop the
transfer. The rehearsal backup is not the final frozen-`dev` backup.

## Independently verify the local recovery material

```bash
bash scripts/verify_primary_local_backup.sh /c/Users/joeps/coding/sagan-pretransfer-rehearsal-2026-10-05
gh auth status
gh repo view JoePShoulak/sagan --json nameWithOwner,visibility,defaultBranchRef
bash scripts/verify_primary_transfer_parity.sh /c/Users/joeps/coding/sagan-pretransfer-rehearsal-2026-10-05 JoePShoulak/sagan
```

The backup verifier checks the mirror and all downloaded asset checksums, compares
the saved ref and release inventories, and clones a separate disposable
checkout from the mirror. Do not delete the backup or restored checkout during
this drill. The source-only parity check compares the current remote refs and
release metadata to that backup but cannot prove destination redirects.
All commands above are read-only with respect to GitHub; do not print secret
values. Do **not** run the live transfer or any release workflow.

## Record the result

Tell the preparation chat the clean-checkout HEAD, whether the focused checks
and backup verifier passed, and the path of the restored checkout it printed.
State in your own words what the local backup cannot recover (including loss
of this computer and GitHub settings/secrets) and which decision is still
needed before releases resume. The chat records that evidence in a dated
audit. If any command or instruction fails, keep the branch unmerged and fix
the guide or tooling, then repeat the drill.

This is only the primary-transfer start drill. Each extracted repository will
still need its own owner-survivability drill before extraction is accepted.
