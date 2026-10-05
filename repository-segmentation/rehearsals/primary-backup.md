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

The rehearsal used a temporary directory. It is not the durable, off-machine
backup required immediately before transfer. Git history alone also does not
back up GitHub release assets, environment settings, secrets, issues, or pull
requests. Those inventories and the approved recovery location remain part of
the transfer gate.

The read-only release inventory mode of
`bash scripts/primary_release_backup.sh inventory` also passed on October 5.
It found five releases with 33 assets totaling 565,009,480 bytes and validated
that the current asset names, sizes, and publisher SHA-256 digests are present
and safe for the backup script. The asset download and checksum comparison
remain untested until a durable backup destination is selected.
