#!/usr/bin/env python3
"""Read public Sagan releases and download their assets without GitHub CLI."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen


API = "https://api.github.com/repos/JoePShoulak/sagan"


def api_json(path: str) -> object:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "sagan-hp1-release-mirror",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urlopen(Request(f"{API}{path}", headers=headers), timeout=60) as response:
        return json.load(response)


def list_releases() -> None:
    releases = api_json("/releases?per_page=100")
    if not isinstance(releases, list):
        raise RuntimeError("GitHub returned an invalid release list")
    for release in releases:
        if isinstance(release, dict) and not release.get("draft"):
            tag = release.get("tag_name")
            if isinstance(tag, str):
                print(tag)


def download_release(tag: str, destination: Path) -> None:
    release = api_json(f"/releases/tags/{quote(tag, safe='')}")
    if not isinstance(release, dict) or release.get("draft"):
        raise RuntimeError(f"GitHub release {tag} is unavailable")
    assets = release.get("assets")
    if not isinstance(assets, list) or not assets:
        raise RuntimeError(f"GitHub release {tag} has no downloadable assets")

    destination.mkdir(parents=True, exist_ok=True)
    for asset in assets:
        if not isinstance(asset, dict):
            raise RuntimeError(f"GitHub release {tag} contains invalid asset metadata")
        name = asset.get("name")
        url = asset.get("browser_download_url")
        if not isinstance(name, str) or Path(name).name != name or not isinstance(url, str):
            raise RuntimeError(f"GitHub release {tag} contains an unsafe asset")
        target = destination / name
        temporary = destination / f".{name}.part"
        request = Request(url, headers={"User-Agent": "sagan-hp1-release-mirror"})
        with urlopen(request, timeout=300) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        temporary.replace(target)
        print(f"Downloaded {name}", file=sys.stderr)


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("list")
    download = subparsers.add_parser("download")
    download.add_argument("tag")
    download.add_argument("destination", type=Path)
    args = parser.parse_args()

    if args.command == "list":
        list_releases()
    else:
        download_release(args.tag, args.destination)


if __name__ == "__main__":
    main()
