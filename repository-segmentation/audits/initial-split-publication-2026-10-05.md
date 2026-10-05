# Initial split publication — October 5, 2026

The intact language repository is at
[`Sagan-Shoulak/sagan`](https://github.com/Sagan-Shoulak/sagan). The six
initial split repositories were published as separate public Git histories.
The owner approved public visibility, `dev` as the default branch, the primary
repository's initial `main` governance, and deferring hosted CI problems to
get the repositories split.

| Repository | Published `dev` | Published `main` |
| --- | --- | --- |
| [`sagan-workspace`](https://github.com/Sagan-Shoulak/sagan-workspace) | `0276c2065596059039da8e7d3c0ee7fbe45c809f` | `f2962c9fb948e102c2824e85c803d1b2ce318ef9` |
| [`sagan-docs`](https://github.com/Sagan-Shoulak/sagan-docs) | `9cbec0df1920ddca3f93f073e170a1b61f018e52` | `a5d11959634d2397ca4f29db9b2d23364b66208c` |
| [`sagan-vscode`](https://github.com/Sagan-Shoulak/sagan-vscode) | `02f72994e831105cdf208999f49c41b10f828fc7` | `404549122c649fbb82e2d00ebbfd731e9665103c` |
| [`sagan-physics`](https://github.com/Sagan-Shoulak/sagan-physics) | `f9d24d04d628574ce2c8627c0172ba6effc232da` | `7e215bc38c9c25bbe729e1b3b808534a63a78c54` |
| [`sagan-render`](https://github.com/Sagan-Shoulak/sagan-render) | `99a8b97d2f9ef4dfc3412e4a4baeab7d8b9d9b3e` | `e60ca2e95924775c4e6081b8fd2e506bdc1235ef` |
| [`sagan-space-game`](https://github.com/Sagan-Shoulak/sagan-space-game) | `5c38aed8088a09fc35cc6239c13951fdb5647c43` | `72eaa083898fdc4b822f7fe29edd997a7417684e` |

The five new candidates passed local Git object checks and the initial-ref
validator before publication. The candidate-owned source paths had not
changed on primary `dev` since their recorded source bases. GitHub verification
then confirmed all six split repositories were public, defaulted to `dev`,
published exactly `dev` and `main` without copied language release tags,
and had only `JoePShoulak` as an initial collaborator/admin. Each new
`main` branch has administrator-enforced protection: linear history,
conversation resolution, no force-push, and no deletion. The primary language
repository retains its own existing branches and release tags.

This is **publication, not completed independence or cutover**. On the first
post-publication check, the workspace Actions runs had failed (including
cancelled jobs); docs and VS Code Actions were queued; physics, render, and
game had no runs. Hosted CI is intentionally deferred, not marked passing.
The official docs site still publishes from the primary repository. The
workspace lock, package/artifact consumption, documentation aggregation,
owner bootstrap, and per-repository CI need verification before any monorepo
copy is retired. Releases and `main` promotions remain paused. The shared
checkout's edited `sandbox/src/main.sagan` was preserved, and its content was
included in the Space Game candidate.
