#!/usr/bin/env python3
"""Verify mirrored release assets against their published SHA-256 sidecars."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path, PurePosixPath


CHECKSUM_LINE = re.compile(r"([0-9a-fA-F]{64}) ([ *])(.+)")


def verify(root: Path) -> None:
    sidecars = sorted(root.glob("*.sha256"))
    if not sidecars:
        raise ValueError("No SHA-256 sidecars found")

    for sidecar in sidecars:
        lines = sidecar.read_text(encoding="utf-8").splitlines()
        if len(lines) != 1:
            raise ValueError(f"{sidecar.name}: expected exactly one checksum line")
        match = CHECKSUM_LINE.fullmatch(lines[0])
        if match is None:
            raise ValueError(f"{sidecar.name}: invalid SHA-256 checksum line")

        asset_name = sidecar.name.removesuffix(".sha256")
        recorded_name = PurePosixPath(match[3].replace("\\", "/")).name
        if recorded_name != asset_name:
            raise ValueError(f"{sidecar.name}: checksum names a different asset")

        asset = root / asset_name
        if not asset.is_file() or asset.is_symlink():
            raise ValueError(f"{sidecar.name}: asset is missing or not a regular file")
        digest = hashlib.sha256()
        with asset.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
        if digest.hexdigest() != match[1].lower():
            raise ValueError(f"{asset_name}: SHA-256 mismatch")
        print(f"{asset_name}: OK")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    try:
        verify(args.root)
    except (OSError, UnicodeError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
