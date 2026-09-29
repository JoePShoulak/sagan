"""Refuse released documentation versions until every publication gate passes."""

from __future__ import annotations

from pathlib import Path
import sys

import yaml
from mkdocs.config import load_config


ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "mkdocs.yml"
DOCS = ROOT / "docs"


def main() -> int:
    # Let MkDocs parse its own configuration, including extension-specific
    # Python-name tags that generic YAML loaders cannot resolve safely.
    config = load_config(config_file=str(CONFIG))
    documentation = config.get("extra", {}).get("documentation", {})
    version = documentation.get("version")
    failures: list[str] = []

    if documentation.get("channel") != "released":
        failures.append("documentation channel is not released")
    if version == "experimental":
        failures.append("experimental documentation cannot pass a release gate")

    for path in sorted(DOCS.rglob("*.md")):
        text = path.read_text(encoding="utf-8")
        if not text.startswith("---\n"):
            failures.append(f"{path.relative_to(ROOT)} has no YAML front matter")
            continue
        try:
            _, raw_meta, _ = text.split("---", 2)
            metadata = yaml.safe_load(raw_meta) or {}
        except (ValueError, yaml.YAMLError) as error:
            failures.append(f"{path.relative_to(ROOT)} has invalid front matter: {error}")
            continue
        if metadata.get("status") != "publication-ready":
            failures.append(f"{path.relative_to(ROOT)} is not publication-ready")
        if metadata.get("publication_ready") is not True:
            failures.append(f"{path.relative_to(ROOT)} has publication_ready != true")
        if metadata.get("verified_in") != version:
            failures.append(f"{path.relative_to(ROOT)} is not verified for {version}")
        if not metadata.get("verified_on") or not metadata.get("verified_by"):
            failures.append(f"{path.relative_to(ROOT)} lacks verification evidence")

    if failures:
        print("Documentation release blocked:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(f"Documentation release gates passed for {version}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
