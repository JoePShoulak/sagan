# Primary repository backup rehearsal

The temporary remote-mirror rehearsal passed on October 5, 2026 using
`bash scripts/rehearse_primary_backup.sh` against
`https://github.com/JoePShoulak/sagan.git`.

- GitHub `HEAD` and restored checkout: `1e1675037ca5273c1381588e6a8dadd395813bb6`
- Remote refs compared before and after cloning: 21, unchanged during the run
- Mirror refs: all 21 matched the remote listing after whitespace normalization
- SHA-256 of both normalized ref listings:
  `7aa81638e81772e5944e24d95421b52fea7fecd63c327ec6b4bce95ac4c85a21`
- `git fsck --full` passed on the mirror and on an independent restored clone
- The restored checkout contained `README.md` and had the same `HEAD` as the mirror

The first rehearsal used a temporary directory. A later local-only backup of
the same source refs and the real release assets passed verification and is
recorded in `../audits/local-backup-2026-10-05.md`. Neither rehearsal is the
final backup of frozen post-merge `dev`. Git history and release assets do not
back up GitHub environment settings, secrets, issues, or pull requests. The
owner accepted that a local-only backup cannot recover from loss of this
machine.

The read-only release inventory mode of
`bash scripts/primary_release_backup.sh inventory` also passed on October 5.
It found five releases with 33 assets totaling 565,009,480 bytes and validated
that the current asset names, sizes, and publisher SHA-256 digests are present
and safe for the backup script. The later local backup downloaded all 33 real
assets and verified publisher digests and sizes. In addition,
`bash scripts/primary_release_backup_test.sh` passed with a mock GitHub
release: it exercised inventory, download, checksum verification, rejection
of a corrupted asset, and refusal to write into a nonempty target.
