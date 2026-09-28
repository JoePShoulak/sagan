---
title: Self-hosting
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Self-hosting

The planned public URL is `https://sagan.shoulak.org/`. Hosting is prepared but
not active.

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
