---
title: Self-hosting
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Self-hosting

The canonical URL is `https://sagan.shoulak.org/`. The repository supports an
internal documentation origin on HP1 and an independently managed public route.

## Hermes ecosystem route

```text
public DNS
  -> Frontdoor VIP 192.168.20.10
  -> active FDR Apache vhost and TLS termination
  -> HP1 origin 192.168.20.21:8781
  -> user-owned static server on port 8781
  -> files in ~/.local/share/sagan-docs/current
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
- `deploy/frontdoor/sagan.shoulak.org.conf` is the Apache route to integrate
  into the Frontdoor project on both peers.
- `deploy/docs/manage.sh` builds and packages a release only after publication
  gates pass.
- `.github/workflows/documentation.yml` validates documentation on GitHub-hosted
  infrastructure and deploys trusted `main` updates on HP1.

## Publication gate

Public packaging is blocked while either condition is true:

- `extra.documentation.internal_only` is not explicitly `false`; or
- any Markdown page is not publication-ready and verified for the current
  documentation version.

The site currently fails this gate by design. Check it with:

```bash
bash scripts/docs.sh release-check
```

An internal-only build can be staged on HP1 without weakening the public
release gate:

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

That command builds an internal archive, atomically advances the origin's
`current` release symlink, restarts the unprivileged server, and verifies its
loopback health endpoint. The lower-level local installer refuses to run unless
the deployment wrapper supplies its explicit safety flag.

The public-route update exports a clean copy of the sibling Frontdoor
repository's committed `HEAD`, overlays `deploy/frontdoor/frontdoor-sagan.patch`
in a temporary directory, and deploys that package. Local uncommitted Frontdoor
work is neither changed nor deployed. The update uses that project's existing
passwordless, narrowly scoped remote helpers; it does not prompt for `sudo`.

## Future activation sequence

When the documentation is approved for publication:

1. review every page and record its verifier, date, and documentation version;
2. set the documentation channel to `public` and `internal_only` to `false`;
3. build and package with `bash deploy/docs/manage.sh package`;
4. install an atomic release and the user-owned origin on HP1;
5. add the Apache route and `sagan.shoulak.org` certificate name to Frontdoor;
6. verify the origin, both direct FDR peers, and the VIP;
7. create the public DNS record; and
8. verify HTTPS from outside the LAN.

Internal files can be installed on HP1 without system privileges. DNS,
certificate issuance, and Frontdoor changes remain separate public-activation
steps.

## Automatic deployment from GitHub Actions

The `Documentation` workflow has two deliberately separate jobs:

1. `validate` runs for matching pushes and pull requests on a disposable
   GitHub-hosted runner.
2. `deploy` runs only for `main` pushes or manual dispatches, only after
   validation succeeds, and only on a runner carrying the custom
   `sagan-docs-hp1` label.

Deployments share a concurrency group and do not cancel an installation already
in progress. The deployment job has read-only repository permissions, targets
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

Installed releases remain under `~/.local/share/sagan-docs/releases`. If a
rollback is needed, point `~/.local/share/sagan-docs/current` at a known-good
release and restart the server. Do not rerun the public-route deployment for an
origin-only problem.
