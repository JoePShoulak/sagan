from __future__ import annotations

import argparse
import fnmatch
from pathlib import Path
import subprocess
import sys
import tomllib


ROOT = Path(__file__).resolve().parent.parent
SEGMENTATION = ROOT / "repository-segmentation"
OUTPUT = SEGMENTATION / "file-manifests"


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True
    )
    return sorted(entry.decode("utf-8") for entry in result.stdout.split(b"\0") if entry)


def assignments() -> dict[str, list[str]]:
    with (SEGMENTATION / "ownership-rules.toml").open("rb") as stream:
        data = tomllib.load(stream)
    if data.get("schema_version") != 1 or data.get("precedence") != "last-match-wins":
        raise ValueError("ownership-rules.toml has an unsupported contract")
    rules = data.get("rules", [])
    repositories = {
        entry["id"]
        for entry in tomllib.loads((SEGMENTATION / "ecosystem.toml").read_text(encoding="utf-8"))["repositories"]
    }
    result = {repository: [] for repository in repositories}
    for path in tracked_files():
        owner = None
        for rule in rules:
            if fnmatch.fnmatchcase(path, rule["pattern"]):
                owner = rule["repository"]
        if owner is None:
            raise ValueError(f"tracked path has no exact owner: {path}")
        if owner not in result:
            raise ValueError(f"tracked path {path} has unknown owner {owner}")
        result[owner].append(path)
    return result


def rendered_manifests() -> dict[Path, str]:
    return {
        OUTPUT / f"{repository}.txt": "".join(f"{path}\n" for path in paths)
        for repository, paths in sorted(assignments().items())
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    try:
        manifests = rendered_manifests()
        if arguments.check:
            failures = []
            for path, expected in manifests.items():
                if not path.is_file() or path.read_text(encoding="utf-8") != expected:
                    failures.append(path.name)
            unexpected = set(OUTPUT.glob("*.txt")) - set(manifests)
            failures.extend(path.name for path in sorted(unexpected))
            if failures:
                raise ValueError("stale or missing file manifests: " + ", ".join(failures))
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            for path, content in manifests.items():
                path.write_text(content, encoding="utf-8", newline="\n")
    except (OSError, ValueError, subprocess.CalledProcessError, tomllib.TOMLDecodeError) as error:
        print(f"Repository segmentation manifest failed: {error}", file=sys.stderr)
        return 1
    print("Repository segmentation file manifests passed." if arguments.check else
          "Repository segmentation file manifests generated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
