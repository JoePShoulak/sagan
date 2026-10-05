# Codecov after the primary repository transfer

On October 5, 2026, the transferred primary repository's coverage workflow
at commit `4132c8c37f99dc5f89c7141f5780b583b64d192e` was retried as
[run 37274993834, attempt 3](https://github.com/Sagan-Shoulak/sagan/actions/runs/37274993834).
Codecov's [service incident](https://status.codecov.com/incidents/n8kwr2rr2v6v)
had changed to `resolved` at 09:15:54 UTC with frontend and backend
operational. The build, report artifact, and coverage floor passed again.
The Codecov CLI reached its upload endpoint, but returned
`Upload queued for processing failed: {"message":"Repository not found"}`.
The required post-v4.9.5 upload gate correctly failed. Do not bypass it or
rerun the same job without changing the repository integration state.

A read-only GitHub organization installation query returned
`{"apps":[],"total_count":0}` for `Sagan-Shoulak`. Codecov's
[current quick start](https://docs.codecov.com/docs/quick-start) says its
GitHub App must be installed for the organization and the repo set up in
Codecov. This strongly suggests the transferred repo is not yet connected
under its new owner; the exact Codecov-side state still requires an account
inspection. No GitHub App was installed, secret changed, or gate weakened in
this rehearsal.

Next, with owner approval, install or grant the Codecov GitHub App access to
`Sagan-Shoulak/sagan`, sync the organization in Codecov, and set up the new
repository record. Verify that the existing GitHub secret is the correct
upload token for that record without printing it. Then rerun only the
failed coverage job at the pinned commit, require its upload gate to pass,
and verify the new organization badge/report. Keep release and `main`
promotion paused independently of this repair.
