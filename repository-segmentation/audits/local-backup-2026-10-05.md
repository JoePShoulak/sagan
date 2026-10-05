# Local primary-transfer backup rehearsal — October 5, 2026

The owner selected a backup on this machine only. The verified rehearsal is
outside the checkout at:

```text
C:\Users\joeps\coding\sagan-pretransfer-rehearsal-2026-10-05
```

This is **not** the final transfer-freeze backup. The source remote's `dev`
was `1e1675037ca5273c1381588e6a8dadd395813bb6` when the mirror was made;
the preparation branch and local `dev` had not yet been merged and pushed.
Refresh the backup from frozen remote refs immediately before transfer.

- Source refs before and after backup: 21, unchanged and identical to mirror
  refs; normalized listing SHA-256:
  `7aa81638e81772e5944e24d95421b52fea7fecd63c327ec6b4bce95ac4c85a21`.
- Five releases and 33 assets were downloaded. Each file matched GitHub's
  recorded SHA-256 digest and byte size. The local asset checksum manifest
  `ASSETS.sha256` has SHA-256
  `052be56781b3e405dc8d99a930a14fdf92bab15831794f18e601d054d3cd530a`.
- The local backup occupies about 548 MiB. `git fsck --full` passed on its
  mirror. `bash scripts/verify_primary_local_backup.sh
  /c/Users/joeps/coding/sagan-pretransfer-rehearsal-2026-10-05` passed every
  asset checksum, stable release metadata, ref comparison, and a fresh
  restored clone with matching default-branch HEAD and `README.md`.
- The independent restored checkout was retained under the Windows temporary
  directory, not inside the backup. The backup itself is the named directory
  above; do not run `make clean` or other broad cleanup against it.

An attempted HP4 copy failed before transferring any data; the exact empty
rehearsal directory created there was removed with `rmdir` after the owner
changed the target to local-only. No HP4 copy is claimed.

This backup can help recover from a GitHub transfer mistake, but not from loss
or corruption of this computer's storage. The owner accepted that limitation
for this transfer. GitHub issues, settings, and secrets are not contained in
the Git mirror or release-asset files; the separate settings audit and
post-transfer parity checks remain required.

## Clean-checkout technical rehearsal

An independent disposable clone of preparation commit `05ddb39` passed the
segmentation contract, documentation-structure check after running the
documented `bash scripts/docs.sh setup` prerequisite, and the local-backup
verifier. Read-only GitHub authentication and source repository identity
checks passed with the owner's account. The sandbox initially blocked Python
package downloads and then permitted the setup in an isolated elevated run;
this was a sandbox network restriction, not a project test failure. The
owner-performed comprehension and recovery drill remains **not run**, and the
backup still predates the final frozen `dev` baseline.

The separate read-only parity checker also passed against the still-current
source repository, matching its refs, releases, visibility, and default
branch to this backup. Its destination and redirect branches remain untested
until the transfer occurs.

## End-to-end backup command rehearsal

The committed `create_primary_local_backup.sh` command subsequently ran from
start to finish into a new directory outside the checkout:

```text
C:\Users\joeps\coding\sagan-pretransfer-script-rehearsal-2026-10-05
```

It fetched the remote mirror and all five releases, verified 21 stable refs
and 33 assets against GitHub's digests and sizes, and restored a separate
checkout. The ref-list and asset-manifest SHA-256 hashes match the first
rehearsal above; the new backup occupies about 548 MiB. The source-only parity
check also passed against this second backup. Both copies are retained; neither
contains the future post-merge frozen `dev` baseline or protects against
losing this machine.
