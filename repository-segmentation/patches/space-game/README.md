# Space Game relocation and current seed patches

This is a **local-only rehearsal**, not a published game repository. First
filter a disposable mirror of source preparation commit
`3c90a111f504279b456736bb0c858af993c98463` with the exact
`file-manifests/sagan-space-game.txt` paths and relocate `sandbox/` to the
root with `--path-rename sandbox/:`. The expected filtered base is
`93be64700db529af5107d373bd2049a8fed93825`, with four root files and
seven surviving commits. Never apply these patches to the intact Sagan repo.

Apply the two numbered patches in order with `git am` in a disposable normal
clone. Expected result: commit
`5c38aed8088a09fc35cc6239c13951fdb5647c43`, tree
`4f6d6f54480a36e8b8445dc73de51225c2fa05d6`. The first patch imports
the refreshed, uncommitted game seed snapshot with `src/main.sagan` SHA-256
`545cf284f287698c45bb50ca4737d6dff9755e45f791d8ad4971bf9e70a51533`.
It retains one original trailing space in a comment to preserve the owner's
file byte-for-byte; `git am` warns but succeeds. The second adds draft
maintainer, technology, and chat guides. The game chat derives goals only
from `SPACE_GAME_DESIGN.md` and the owner's clarifying answers, not old chats
or prototype details. New guide pages are `review-needed` and not
publication-ready.

A fresh-clone `git am` replay matched the result tree and source hash,
passed `git fsck --full`, and the imported print-only Sagan program ran to
completion on Windows with the existing compiler and MinGW PATH. It printed
the six body summaries and distances. This is not proof of independent
installed Sagan/physics packages, Linux/macOS support, or a working game.

The shared dirty sandbox and both local backups remain untouched. Recheck
the shared sandbox hash at the final extraction freeze; this patch is a
dated seed, not an instruction to overwrite later owner work. No split
remote was created or pushed. The project-wide release and `main` promotion
hold continues.
