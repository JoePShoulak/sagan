#!/usr/bin/env python3
"""Build the public HTML and JSON indexes for the HP1 release mirror."""

from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path
from urllib.parse import quote


VERSION = re.compile(
    r"^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-rc\.([1-9][0-9]*))?$"
)


def version_key(path: Path) -> tuple[int, int, int, int, int]:
    match = VERSION.fullmatch(path.name)
    if match is None:
        raise ValueError(f"not a stable release directory: {path.name}")
    major, minor, patch, candidate = match.groups()
    return int(major), int(minor), int(patch), candidate is None, int(candidate or 0)


def asset_kind(name: str) -> str:
    lowered = name.lower()
    if lowered.endswith(".exe"):
        return "Windows installer"
    if lowered.endswith(".vsix"):
        return "VS Code extension"
    if lowered.endswith(".zip"):
        return "Portable archive"
    if lowered.endswith(".sha256"):
        return "SHA-256 checksum"
    if lowered.endswith(".spdx.json"):
        return "Software bill of materials"
    if lowered.endswith(".json"):
        return "Release metadata"
    return "Release asset"


def collect(root: Path) -> list[dict[str, object]]:
    releases: list[dict[str, object]] = []
    directories = sorted(
        (path for path in root.iterdir() if path.is_dir() and VERSION.fullmatch(path.name)),
        key=version_key,
        reverse=True,
    )
    for directory in directories:
        assets = []
        for path in sorted((path for path in directory.iterdir() if path.is_file()), key=lambda item: item.name):
            assets.append(
                {
                    "name": path.name,
                    "kind": asset_kind(path.name),
                    "bytes": path.stat().st_size,
                    "url": f"{quote(directory.name)}/{quote(path.name)}",
                }
            )
        releases.append({"tag": directory.name, "version": directory.name[1:], "assets": assets})
    return releases


def format_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024 or unit == "GiB":
            return f"{value:.0f} {unit}" if unit == "B" else f"{value:.1f} {unit}"
        value /= 1024
    raise AssertionError("unreachable")


def render(releases: list[dict[str, object]]) -> str:
    sections = []
    for index, release in enumerate(releases):
        tag = str(release["tag"])
        badges = []
        if index == 0:
            badges.append('<span class="badge">Latest</span>')
        if "-rc." in tag:
            badges.append('<span class="badge preview">Preview</span>')
        rows = []
        for asset in release["assets"]:  # type: ignore[union-attr]
            name = str(asset["name"])
            rows.append(
                "<li><a href=\"{url}\">{name}</a>"
                "<span>{kind} · {size}</span></li>".format(
                    url=html.escape(str(asset["url"]), quote=True),
                    name=html.escape(name),
                    kind=html.escape(str(asset["kind"])),
                    size=format_size(int(asset["bytes"])),
                )
            )
        sections.append(
            f'<section><h2>{html.escape(tag)} {" ".join(badges)}</h2><ul>{"".join(rows)}</ul></section>'
        )

    empty = "<p>No stable Sagan releases have been mirrored yet.</p>" if not releases else ""
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="robots" content="noindex,nofollow,noarchive">
  <title>Sagan downloads</title>
  <style>
    :root {{ color-scheme: light dark; font: 16px/1.5 system-ui, sans-serif; }}
    body {{ max-width: 58rem; margin: 0 auto; padding: 3rem 1.25rem; }}
    header {{ margin-bottom: 2.5rem; }}
    h1 {{ margin-bottom: .4rem; }}
    section {{ border-top: 1px solid #8886; padding: 1.2rem 0; }}
    h2 {{ display: flex; align-items: center; gap: .7rem; }}
    ul {{ list-style: none; padding: 0; }}
    li {{ display: flex; justify-content: space-between; gap: 1rem; padding: .55rem 0; }}
    li span {{ color: #777; text-align: right; }}
    .badge {{ font-size: .72rem; padding: .15rem .45rem; border-radius: 999px; background: #2563eb; color: white; }}
    .badge.preview {{ background: #7c3aed; }}
    a {{ color: #2563eb; }}
    nav a {{ margin-right: 1rem; }}
    @media (max-width: 38rem) {{ li {{ display: block; }} li span {{ display: block; text-align: left; }} }}
  </style>
</head>
<body>
  <nav><a href="/">Documentation</a><a href="https://github.com/Sagan-Shoulak/sagan/releases">GitHub releases</a></nav>
  <header><h1>Sagan downloads</h1><p>Windows installers, portable archives, VS Code extensions, checksums, and release metadata mirrored on HP1.</p></header>
  {empty}{''.join(sections)}
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=True)

    releases = collect(root)
    payload = {"schema": "sagan.release-mirror/1", "releases": releases}
    (root / "releases.json.tmp").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    (root / "index.html.tmp").write_text(render(releases), encoding="utf-8")
    (root / "releases.json.tmp").replace(root / "releases.json")
    (root / "index.html.tmp").replace(root / "index.html")


if __name__ == "__main__":
    main()
