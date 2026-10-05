# Space Game seed refresh, October 5, 2026

The shared checkout's `sandbox/src/main.sagan` changed after the earlier
local-only freeze. Its SHA-256 was
`545cf284f287698c45bb50ca4737d6dff9755e45f791d8ad4971bf9e70a51533`,
while the earlier frozen copy remained
`f8886930a4e5455750bdab0c6732c3f29562769d19e162cf2e060df894622c43`.
The other three game seed files matched the earlier freeze. The shared file
remained uncommitted and was **not** staged, reset, or overwritten.

A second local-only copy of all four current game seed files was made at
`/c/Users/joeps/coding/sagan-space-game-sandbox-refresh-2026-10-05`.
The copied source hash matched the shared file immediately after copying;
the old freeze was left intact. The refreshed four-file inventory was:

| File | SHA-256 |
| --- | --- |
| `SPACE_GAME_DESIGN.md` | `31a334bb093f38fdcfd52756d07219c554a4239d8c09fbb9594f8e9b02c74c1f` |
| `sandbox/sagan.toml` | `78ce832242f04fc04fcc49bfc55c3a8ebdffc8b4fd0ba0650036479c0ec33928` |
| `sandbox/sagan.lock` | `8351d486f62322ef32a277bc92b09cf72bc709d8db7c9e2105ec7298fabe7e8b` |
| `sandbox/src/main.sagan` | `545cf284f287698c45bb50ca4737d6dff9755e45f791d8ad4971bf9e70a51533` |

Both copies are on the same machine and cannot survive machine/storage loss.
Neither copies GitHub settings, secrets, issues, or PR discussions. Recheck
the shared file hash before any final game publication; a later user edit
requires a new preservation step, not silent reuse of this snapshot.
