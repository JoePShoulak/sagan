---
title: Self-hosting
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Self-hosting

The canonical URL is `https://sagan.shoulak.org/`. HP1 serves released
documentation archives, the continuously updated `experimental` version, and
a verified stable-release mirror at `/downloads/` through an independently
managed public route.

## Hermes ecosystem route

```text
public DNS
  -> Frontdoor VIP 192.168.20.10
  -> active FDR Apache vhost and TLS termination
  -> HP1 origin 192.168.20.21:8781
  -> user-owned static server on port 8781
  -> versioned files in ~/.local/share/sagan-docs/current
```

HP1 owns the origin because it is the ecosystem's application host. The Sagan
origin is an unprivileged Python static server; HP1's shared Nginx configuration
does not need to change.
The dedicated `hermes` host retains the HERMES runtime, while HP4 remains
scoped to backup, storage, and shared assets. The redundant Frontdoor peers
own public routing, certificates, and the public VIP.

## Repository artifacts

- `deploy/hp1/server.py` defines the private static origin.
- `deploy/hp1/sagan-docs` starts, stops, and inspects the user-owned process.
- `deploy/hp1/install-user.sh` atomically installs a staged release and adds a
  user crontab entry so it starts after reboot.
- `deploy/releases/mirror.sh` verifies one immutable GitHub release and rebuilds
  the public download catalog.
- `deploy/releases/sync.sh` reconciles every published stable GitHub release
  into the HP1 mirror.
- `deploy/releases/index.py` creates `/downloads/index.html` and the
  machine-readable `/downloads/releases.json` catalog.
- `deploy/frontdoor/sagan.shoulak.org.conf` is the Apache route to integrate
  into the Frontdoor project on both peers.
- `scripts/docs_versions.sh` publishes `experimental` or an approved immutable
  release to the generated `docs-site` branch using Mike.
- `deploy/docs/manage.sh` packages the complete generated branch so HP1 receives
  the selector, default redirect, experimental version, and release archives.
- `.github/workflows/documentation.yml` validates documentation on GitHub-hosted
  infrastructure and deploys trusted `main` updates on HP1.

## Publication gate

Publishing a released version is blocked while either condition is true:

- the documentation channel is not `released`; or
- any Markdown page is not publication-ready and verified for the current
  documentation version.

The experimental channel fails this gate by design. Check a release candidate
with `SAGAN_DOCS_VERSION` and `SAGAN_DOCS_CHANNEL=released` set:

```bash
SAGAN_DOCS_VERSION=1.0.0 \
SAGAN_DOCS_CHANNEL=released \
  bash scripts/docs.sh release-check
```

The complete generated versioned site can be staged on HP1 without weakening
the release gate:

```bash
bash deploy/docs/manage.sh stage-hp1
ssh hp1
cd ~/sagan-docs-staging
bash install-user.sh
```

No command in this HP1 installation path requires `sudo`.

For routine hosting updates from the Sagan repository, use:

```bash
# Refresh only the HP1 origin.
bash deploy/hosting/update.sh origin

# Refresh the origin, Frontdoor route, and HTTPS certificate.
bash deploy/hosting/update.sh all
```

On HP1 itself, the CI runner uses the local-only route:

```bash
bash deploy/hosting/update.sh hp1-local
```

That command first reconciles stable GitHub releases, packages the generated
`docs-site` branch, atomically advances the origin's `current` deployment
symlink, exposes the verified mirror through that snapshot's `/downloads/`
symlink, restarts the unprivileged server, and verifies its loopback health
endpoint. The lower-level local installer refuses to run unless
the deployment wrapper supplies its explicit safety flag. The origin launcher
also removes GitHub's job-tracking marker from the long-lived server process so
the runner does not clean it up when the deployment job finishes.

The public-route update exports a clean copy of the sibling Frontdoor
repository's committed `HEAD`, overlays `deploy/frontdoor/frontdoor-sagan.patch`
in a temporary directory, and deploys that package. Local uncommitted Frontdoor
work is neither changed nor deployed. The update uses that project's existing
passwordless, narrowly scoped remote helpers; it does not prompt for `sudo`.

## Versioned publication sequence

Every validated push to `main` updates only `experimental`. When a Sagan release
and its documentation are approved:

1. review every page and record its verifier, date, and release documentation
   version;
2. manually run the Documentation workflow from the matching release ref with
   `release_version` set to `MAJOR.MINOR.PATCH`;
3. allow the release gate to create that immutable version, move `latest`, and
   set `latest` as the site-root default;
4. allow the HP1 job to install the complete versioned site atomically; and
5. verify the root, `latest`, the numbered release, `experimental`, and
   `/downloads/` through the origin and public route.

Versioned files can be installed on HP1 without system privileges. DNS,
certificate issuance, and Frontdoor changes remain separate hosting steps.

## Automatic deployment from GitHub Actions

The `Documentation` workflow has three deliberately separate jobs:

1. `validate` runs for matching pushes and pull requests on a disposable
   GitHub-hosted runner.
2. `publish-version` updates `experimental` on ordinary `main` pushes or creates
   an explicitly requested released version on manual dispatch.
3. `deploy` runs only after validation and publication succeed and only on a runner carrying the custom
   `sagan-docs-hp1` label.

Deployments share concurrency groups and do not cancel an installation already
in progress. Only the version-publishing job can update `docs-site`. The deployment job has read-only repository permissions, targets
the `documentation` GitHub environment, and refreshes only the HP1 origin. It
does not change Frontdoor, DNS, or certificates.

### Register the HP1 runner

Use a dedicated, unprivileged HP1 account for the runner and documentation
server. In the GitHub repository, open **Settings -> Actions -> Runners -> New
self-hosted runner**, select Linux and HP1's architecture, and run the displayed
download and registration commands on HP1. The registration token is temporary,
so use the commands generated by GitHub rather than storing it in this
repository.

During registration, add the custom label:

```bash
./config.sh --url https://github.com/JoePShoulak/sagan \
  --token YOUR_TEMPORARY_REGISTRATION_TOKEN \
  --name hp1-sagan-docs \
  --labels sagan-docs-hp1 \
  --unattended
```

After registration, copy `deploy/hp1/sagan-docs-actions-runner` and
`deploy/hp1/install-actions-runner-user.sh` to HP1, then run the installer from
the directory containing both files:

```bash
bash install-actions-runner-user.sh
```

This installs a user-owned runner manager and an `@reboot` crontab entry, so it
does not require `sudo`. Confirm that `hp1-sagan-docs` is online and has the
`sagan-docs-hp1` label before manually dispatching the workflow once.

The HP1 account needs `python3`, Python virtual-environment support, `curl`,
`tar`, and `crontab`. It does not need repository write permission or Frontdoor
credentials.

!!! warning "Keep untrusted work away from HP1"

    Never add a pull-request job that targets the HP1 runner. GitHub warns that
    self-hosted runners can be persistently compromised by untrusted workflow
    code, especially for public repositories. Keep this runner repository-scoped,
    dedicated to Sagan documentation, unprivileged, and free of unrelated
    credentials. Protect `main` and restrict who may change workflow files.

### Verification and recovery

To inspect a deployment, open the workflow run and confirm that both jobs passed,
then check:

```bash
curl --fail http://127.0.0.1:8781/healthz
~/.local/bin/sagan-docs status
~/.local/bin/sagan-docs logs
~/.local/bin/sagan-docs-actions-runner status
~/.local/bin/sagan-docs-actions-runner logs
```

Installed site snapshots remain under `~/.local/share/sagan-docs/releases`.
Each snapshot contains every documentation version present in `docs-site` at
deployment time. If a rollback is needed, point
`~/.local/share/sagan-docs/current` at a known-good snapshot and restart the
server. Do not rerun the public-route deployment for an origin-only problem.

Mirrored binaries live separately under `~/.local/share/sagan-releases/vVERSION`
so documentation snapshot rotation cannot remove them. The `latest` symlink
tracks the highest mirrored stable semantic version. A mirror run never
replaces an existing version directory; publish a new Sagan version to correct
an asset. Published SHA-256 files are verified. For legacy releases that lack
one, HP1 generates and serves its own checksum without modifying the downloaded
asset.
