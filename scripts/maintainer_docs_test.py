#!/usr/bin/env python3
"""Check the root maintainer entry points and their local references."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent.parent
REQUIRED = {
    "MAINTAINERS.md": (
        "docs/contributing/maintainer-handoff-roadmap.md",
        "docs/contributing/repository-fracturing-roadmap.md",
        "docs/contributing/change-lifecycle.md",
        "docs/contributing/versioning.md",
        "docs/contributing/release-lifecycle.md",
        "docs/contributing/documentation.md",
        "docs/tooling/language-service-contracts.md",
        "docs/tooling/debug-adapter-contract.md",
    ),
    "CODEX_START.md": ("MAINTAINERS.md",),
}
LINK = re.compile(r"\]\(([^)]+)\)")


def main() -> int:
    failures = []
    for name, required_links in REQUIRED.items():
        path = ROOT / name
        if not path.is_file():
            failures.append(f"missing {name}")
            continue
        content = path.read_text(encoding="utf-8")
        targets = {match.group(1).split("#", 1)[0] for match in LINK.finditer(content)}
        for target in required_links:
            if target not in targets:
                failures.append(f"{name} must link to {target}")
        for target in targets:
            if not target or "://" in target or target.startswith("mailto:"):
                continue
            linked = (path.parent / target).resolve()
            if not linked.is_relative_to(ROOT) or not linked.is_file():
                failures.append(f"{name} has missing or escaping link: {target}")
    if failures:
        for failure in failures:
            print(f"Maintainer documentation check: {failure}", file=sys.stderr)
        return 1
    print("Maintainer documentation entry points and local links passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
