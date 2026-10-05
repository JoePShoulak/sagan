"""Read-only exact-tree and integrity check for a filtered split candidate."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def git(repository: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        capture_output=True, check=False,
    )
    if result.returncode:
        detail = (result.stderr or result.stdout).decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git {' '.join(arguments)} failed: {detail}")
    return result.stdout


def verify(repository: Path, manifest: Path, ref: str) -> None:
    expected = set(manifest.read_text(encoding="utf-8").splitlines())
    if not expected or "" in expected:
        raise ValueError("ownership manifest is empty or contains a blank path")
    raw = git(repository, "ls-tree", "-r", "-z", "--name-only", ref)
    actual = {path.decode("utf-8") for path in raw.split(b"\0") if path}
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing or extra:
        raise ValueError(
            f"filtered tree differs from ownership manifest: missing={missing}, extra={extra}"
        )
    git(repository, "fsck", "--full")
    head = git(repository, "rev-parse", ref).decode("ascii").strip()
    count = git(repository, "rev-list", "--count", ref).decode("ascii").strip()
    print(f"Filtered tree passed: {len(actual)} exact files, {count} commits, {head}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("repository", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--ref", default="refs/heads/codex/segmentation-public-visibility")
    arguments = parser.parse_args()
    try:
        verify(arguments.repository, arguments.manifest, arguments.ref)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Filtered component verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
