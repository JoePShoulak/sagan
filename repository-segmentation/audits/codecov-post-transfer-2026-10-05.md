# Codecov after the primary repository transfer

On October 5, 2026, the transferred primary repository's coverage workflow
at commit `4132c8c37f99dc5f89c7141f5780b583b64d192e` was retried as
[run 37274993834, attempt 3](https://github.com/Sagan-Shoulak/sagan/actions/runs/37274993834).
Codecov's [service incident](https://status.codecov.com/incidents/n8kwr2rr2v6v)
had changed to `resolved` at 09:15:54 UTC. The build, report artifact, and
coverage floor passed again, but the upload returned
`Upload queued for processing failed: {"message":"Repository not found"}`.
The required upload gate correctly failed then.

A read-only GitHub organization query at that point showed zero GitHub App
installations. Codecov's [quick start](https://docs.codecov.com/docs/quick-start)
describes installing its GitHub App and setting up the repository.
The owner then installed Codecov. A second read-only organization query found
one Codecov GitHub App installation with `selected` repository access and
one selected repository. No secret was printed or gate weakened.

Only the failed coverage job was retried at the same pinned commit as
[run 37274993834, attempt 4](https://github.com/Sagan-Shoulak/sagan/actions/runs/37274993834).
The job passed. Its upload step found one `build/coverage.info`, reported
the [new-organization commit URL](https://app.codecov.io/github/sagan-shoulak/sagan/commit/4132c8c37f99dc5f89c7141f5780b583b64d192e),
said `Upload queued for processing complete`, and concluded success.
The public [new-organization badge](https://codecov.io/gh/Sagan-Shoulak/sagan/graph/badge.svg)
returned HTTP 200 with **92%**, and the commit URL returned HTTP 200.
Thus the post-transfer upload and public badge/report gate passed without
changing the coverage workflow or secret. A new badge link is prepared in
this local segmentation branch; the old link remains on published `dev`
until separately integrated. The Codecov result does not lift the
independent release and `main` promotion pause.
