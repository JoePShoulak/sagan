# Space Game seed preservation, October 5, 2026

The shared Sagan checkout remains on `codex/repo-segmentation-manifests`.
`sandbox/src/main.sagan` is tracked but has an uncommitted 183-line diff
(176 additions, 7 deletions). The Git mirror of frozen `dev` does **not**
include this working-tree version. It was neither staged nor modified during
segmentation preparation.

The four-file seed was copied to the new local-only directory
`/c/Users/joeps/coding/sagan-space-game-sandbox-freeze-2026-10-05`.
Hashes of each copy matched the live source at capture:

| File | SHA-256 |
| --- | --- |
| `sandbox/src/main.sagan` | `F8886930A4E5455750BDAB0C6732C3F29562769D19E162CF2E060DF894622C43` |
| `sandbox/sagan.toml` | `78CE832242F04FC04FCC49BFC55C3A8EBDFFC8B4FD0BA0650036479C0EC33928` |
| `sandbox/sagan.lock` | `8351D486F62322EF32A277BC92B09CF72BC709D8DB7C9E2105EC7298FABE7E8B` |
| `SPACE_GAME_DESIGN.md` | `31A334BB093F38FDCFD52756D07219C554A4239D8C09FBB9594F8E9B02C74C1F` |

Before extracting `sagan-space-game`, recheck the live working-tree file.
If it changed, preserve the newer version separately and decide deliberately
which version enters the game repository. Do not silently substitute the
older tracked `dev` copy, stage the shared checkout's file, or discard the
user's uncommitted work. This local copy does not survive loss of this machine.
