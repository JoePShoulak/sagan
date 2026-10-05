---
title: Repository segmentation start gate, October 5 2026
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Repository segmentation start gate, October 5, 2026

The intact primary repository has moved to `Sagan-Shoulak/sagan`; **no split
repository has been created or pushed**. This preparation branch remains
separate from `dev`. The owner said not to merge it until the project is
actually ready to begin segmentation. All future split repositories are to
be public with initially mirrored primary governance. Releases and `main`
promotion remain paused.

## Current evidence

| Component | Local result | Still required before independent publication |
| --- | --- | --- |
| Primary `sagan` | Transfer parity, redirects, backup/restore, canonical links, docs, extension, and installer checks passed. | Successful post-transfer Codecov upload and badge verification. [Codecov incident `n8kwr2rr2v6v`](https://status.codecov.com/incidents/n8kwr2rr2v6v) was still monitoring with backend marked major outage at 08:33 UTC. No upload gate bypass. |
| `sagan-workspace` | Path/history and dev/main-only local-ref rehearsals passed. Two replayable patches passed ten offline lock/bootstrap/restore tests, a real primary clone, and fresh replay. | Curate monorepo-only audit scripts, implement update/build/test/lock-refresh, editor workspace and package-index generation, cross-repo demos, hosted CI, clean-machine owner drill. |
| `sagan-docs` | Path/history and dev/main-only local-ref rehearsals passed. Pinned primary-source assembly, five offline tests, strict MkDocs build, and fresh replay passed. | Wire build/deploy scripts to aggregate, mount component docs, preview/review/version gates, host cutover/rollback, hosted CI, owner drill. Live HP1 site stays on intact repo. |
| `sagan-vscode` | 61-file history extraction and five local relocation patches; Windows unit, bundle, live host, VSIX, and isolated install checks passed. | Build the pinned Sagan source in independent Linux/macOS/Windows hosted CI; complete docs aggregation, release tag policy, governance and owner drill. |
| `sagan-physics` | `--no-ff` filter retained exact 29 files; candidate-local catalog, all three headless numeric suites, and fresh patch replay passed on Windows. | Final package-root relocation, independent hosted CI/platform checks, distribution/compatibility, docs mount, owner drill. |
| `sagan-render` | Exact 21-file filter; candidate-local catalog, auto-closing Win32 window and BMP tests, stock-icon fallback, and fresh patch replay passed. | Final package-root relocation, independent hosted Windows CI, Windows package distribution, docs mount, owner drill. Linux/macOS native backend is explicitly unsupported and must not be advertised. |
| `sagan-space-game` | Exact four-file history and root relocation; refreshed dirty seed preserved separately, imported into a local candidate, replayed, and ran with existing compiler. | Recheck shared sandbox hash at freeze, owner review of seed/design, installed Sagan plus independent physics package, game tests/CI, owner drill. |

The detailed commands, source/ref hashes, test outputs, and limitations live
in `rehearsals/`, `patches/`, `audits/`, and `readiness.toml`. The local-only
backup is not survivable if this machine/storage fails and cannot recreate
GitHub settings, secrets, issues, or PR conversations.

## Do not collapse these gates

1. Wait for Codecov service recovery, rerun only the failed post-transfer
   coverage upload path, and verify the organization badge shows a real
   report. The measured local coverage floor passing is not an upload pass.
2. Refresh source refs and backups at the final extraction freeze. The local
   patch bases are dated rehearsals; re-filter from then-current reviewed
   `dev` and compare exact owned paths. Physics requires `--no-ff` unless a
   newer verified filter procedure supersedes it.
3. Finish each destination's independent functional/CI, docs, governance,
   rollback, and owner-maintainer gates before creating or pushing it. Do not
   transfer inherited Sagan release tags; initial push must name only the
   reviewed `dev` and `main` refs when those refs meet their own rules.
4. Keep the monorepo copies and shared dirty sandbox until the destination
   is verified and a later reviewed removal request occurs. Do not merge the
   preparation branch or open remote split repositories merely because a
   local extraction filter succeeded.

For ordinary `dev` merges, rerun relevant tests after integration, not the
full suite. The complete suite is reserved for reviewed `dev` to `main`
promotion or release, and the current owner hold still applies.
