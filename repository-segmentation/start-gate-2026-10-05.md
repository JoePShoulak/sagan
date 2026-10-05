---
title: Repository segmentation start gate, October 5 2026
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Repository segmentation start gate, October 5, 2026

The intact primary repository moved to `Sagan-Shoulak/sagan`. The preparation
branch was fast-forwarded into `dev` at `221d0c5` after focused contracts
and a strict documentation build passed. The first public split repository,
`Sagan-Shoulak/sagan-workspace`, was created with the transfer/audit archive
retained by the owner's choice. Its initial refs were only `dev` and `main`,
with no inherited language tags. The other destinations remain uncreated.
All split repositories are public with initially mirrored primary governance.
Releases and `main` promotion remain paused.

## Current evidence

| Component | Local result | Still required before independent publication |
| --- | --- | --- |
| Primary `sagan` | Transfer parity, redirects, backup/restore, canonical links, docs, extension, and installer checks passed. The Codecov upload gate passed and the organization badge returned 92%. The post-integration local-only backup at `221d0c5` verified the Git mirror, 33 release assets, and independent restore. | A local backup cannot survive this machine's loss or restore GitHub settings/history. Releases and `main` remain paused. |
| `sagan-workspace` | Public repo created with `dev` default, only `dev`/`main` heads, no tags, primary-equivalent `main` protection, and only the owner's admin access. The archive was retained. A fresh public clone bootstrapped the exact primary lock, built compiler/LSP, and passed focused package tests. Initial hosted CI found a Windows/macOS path-alias bug; a regression-tested fix reached `dev` at `9d2fa5d`. | The hosted retry remains queued without runners as of this record; its result, locked integration with later component repositories, and owner handoff/rollback drills remain gates before claiming independence. |
| `sagan-docs` | Path/history and dev/main-only local-ref rehearsals passed. Four patches passed twelve offline tests and clean replay. Strict Windows MkDocs builds passed with pinned local extension/physics/rendering mounts and, separately, with a real locked primary clone through the new automatic active-source bootstrap. Canonical component remotes remain planned; three-platform CI is drafted but not hosted-tested. | Wire build/deploy scripts to aggregate, add preview/review/version gates, host cutover/rollback, hosted CI, owner drill. Live HP1 site stays on intact repo. |
| `sagan-vscode` | 61-file history extraction and five local relocation patches; Windows unit, bundle, live host, VSIX, and isolated install checks passed. | Build the pinned Sagan source in independent Linux/macOS/Windows hosted CI; complete docs aggregation, release tag policy, governance and owner drill. |
| `sagan-physics` | `--no-ff` filter retained exact 29 files; candidate-local catalog, all three headless numeric suites, and fresh patch replay passed on Windows. | Final package-root relocation, independent hosted CI/platform checks, distribution/compatibility, docs mount, owner drill. |
| `sagan-render` | Exact 21-file filter; candidate-local catalog, auto-closing Win32 window and BMP tests, stock-icon fallback, and fresh patch replay passed. | Final package-root relocation, independent hosted Windows CI, Windows package distribution, docs mount, owner drill. Linux/macOS native backend is explicitly unsupported and must not be advertised. |
| `sagan-space-game` | Exact four-file history and root relocation; refreshed dirty seed preserved separately, imported into a local candidate, replayed, and ran with existing compiler. | Recheck shared sandbox hash at freeze, owner review of seed/design, installed Sagan plus independent physics package, game tests/CI, owner drill. |

The detailed commands, source/ref hashes, test outputs, and limitations live
in `rehearsals/`, `patches/`, `audits/`, and `readiness.toml`. The local-only
backup is not survivable if this machine/storage fails and cannot recreate
GitHub settings, secrets, issues, or PR conversations.

## Three distinct gates

1. **Before creating each remaining destination:** finish its ownership and path map,
   locally runnable candidate, focused tests, maintainer/chat/technology
   guides, public visibility and initial governance plan, explicit rollback
   plan, and owner review of anything public. Refresh source refs and backups
   at the final extraction freeze; re-filter from then-current reviewed `dev`
   and compare exact owned paths. Physics requires `--no-ff` unless a newer
   verified procedure supersedes it. Keep later extraction work on focused
   branches until the corresponding pre-creation gate is ready.
2. **After creation, before calling a destination independent:** push only
   reviewed `dev` and `main` heads with explicit refspecs and no inherited
   language release tags; set the approved default/protection/access; run
   hosted CI in that actual repository; verify package consumption, docs
   aggregation, and the owner-maintainer/rollback drills. Hosted CI cannot
   pass before its destination exists. A failed post-creation check leaves
   the monorepo source and current site in service while the destination is
   repaired; do not publish releases or claim cutover.
3. **Before retiring a monorepo copy or changing the live docs site:** pass
   cross-repository integration at exact workspace locks, docs review and
   deployment/rollback checks, and a separately reviewed removal or cutover
   request. Keep the shared dirty sandbox intact. The complete suite is a
   `main` promotion or release gate, not a routine `dev` integration gate.

For ordinary `dev` merges, rerun relevant tests after integration, not the
full suite. The complete suite is reserved for reviewed `dev` to `main`
promotion or release, and the current owner hold still applies.
