from __future__ import annotations

import csv
import fnmatch
import json
from pathlib import Path
import sys
import tomllib


ROOT = Path(__file__).resolve().parent.parent
SEGMENTATION = ROOT / "repository-segmentation"
EXPECTED_REPOSITORIES = {
    "sagan",
    "sagan-vscode",
    "sagan-physics",
    "sagan-render",
    "sagan-workspace",
    "sagan-docs",
    "sagan-space-game",
}
ALLOWED_INVENTORY_STATUS = {"approved", "proposed", "needs-split", "needs-rewrite"}
ALLOWED_DECISION_STATUS = {"approved", "provisional", "required"}


def fail(message: str) -> None:
    raise ValueError(message)


def read_toml(path: Path) -> dict:
    with path.open("rb") as stream:
        return tomllib.load(stream)


def check_ecosystem() -> None:
    data = read_toml(SEGMENTATION / "ecosystem.toml")
    if data.get("schema_version") != 1:
        fail("ecosystem.toml must use schema_version = 1")

    repositories = data.get("repositories", [])
    ids = [entry.get("id") for entry in repositories]
    if len(ids) != len(set(ids)):
        fail("ecosystem.toml contains duplicate repository IDs")
    if set(ids) != EXPECTED_REPOSITORIES:
        fail(f"ecosystem.toml repositories differ from expected set: {set(ids)}")

    for entry in repositories:
        for dependency in entry.get("depends_on", []):
            if dependency not in EXPECTED_REPOSITORIES:
                fail(f"{entry['id']} depends on unknown repository {dependency}")

    dependency_pairs = {
        (entry["id"], dependency)
        for entry in repositories
        for dependency in entry.get("depends_on", [])
    }
    for rule in data.get("forbidden_dependencies", []):
        pair = (rule.get("from"), rule.get("to"))
        if pair in dependency_pairs:
            fail(f"forbidden dependency is present: {pair[0]} -> {pair[1]}")


def check_chat_map() -> None:
    data = read_toml(SEGMENTATION / "chat-map.toml")
    if data.get("schema_version") != 1:
        fail("chat-map.toml must use schema_version = 1")

    chats = data.get("chats", [])
    ids = [entry.get("id") for entry in chats]
    repositories = [entry.get("repository") for entry in chats]
    if len(ids) != len(set(ids)):
        fail("chat-map.toml contains duplicate chat IDs")
    if len(repositories) != len(set(repositories)):
        fail("chat-map.toml assigns multiple chats to one repository")
    if set(repositories) != EXPECTED_REPOSITORIES:
        fail("chat-map.toml must assign one chat to every repository")


def check_inventory() -> None:
    path = SEGMENTATION / "inventory.tsv"
    with path.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))

    if not rows or set(rows[0]) != {"path", "destination", "status", "notes"}:
        fail("inventory.tsv has an invalid header")

    seen_paths: set[str] = set()
    for row in rows:
        item = row["path"]
        if item in seen_paths:
            fail(f"inventory.tsv contains duplicate path {item}")
        seen_paths.add(item)
        if row["destination"] not in EXPECTED_REPOSITORIES:
            fail(f"inventory path {item} has unknown destination {row['destination']}")
        if row["status"] not in ALLOWED_INVENTORY_STATUS:
            fail(f"inventory path {item} has unsupported status {row['status']}")
        if not any(character in item for character in "*?[") and not (ROOT / item).exists():
            fail(f"inventory path does not exist: {item}")

    def owns(rule: str, tracked_path: str) -> bool:
        if rule.endswith("/"):
            return tracked_path.startswith(rule)
        if any(character in rule for character in "*?["):
            return fnmatch.fnmatchcase(tracked_path, rule)
        return tracked_path == rule

    import subprocess
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True
    )
    tracked_paths = [entry.decode("utf-8") for entry in result.stdout.split(b"\0") if entry]
    unowned = [
        tracked_path
        for tracked_path in tracked_paths
        if not any(owns(row["path"], tracked_path) for row in rows)
    ]
    if unowned:
        preview = ", ".join(unowned[:10])
        fail(f"tracked files lack an inventory owner: {preview}")


def check_components_and_readiness() -> None:
    components = read_toml(SEGMENTATION / "components.toml")
    if components.get("schema_version") != 1:
        fail("components.toml must use schema_version = 1")
    entries = components.get("components", [])
    ids = [entry.get("id") for entry in entries]
    if set(ids) != EXPECTED_REPOSITORIES or len(ids) != len(set(ids)):
        fail("components.toml must describe every repository exactly once")
    phases = [entry.get("phase") for entry in entries]
    if any(not isinstance(phase, int) or phase < 1 for phase in phases):
        fail("components.toml phases must be positive integers")
    for entry in entries:
        for source_root in entry.get("source_roots", []):
            if not (ROOT / source_root).exists():
                fail(f"component {entry['id']} source root does not exist: {source_root}")

    readiness = read_toml(SEGMENTATION / "readiness.toml")
    if readiness.get("schema_version") != 1:
        fail("readiness.toml must use schema_version = 1")
    decisions = readiness.get("decisions", [])
    decision_ids = [entry.get("id") for entry in decisions]
    if len(decision_ids) != len(set(decision_ids)):
        fail("readiness.toml contains duplicate decision IDs")
    for entry in decisions:
        if entry.get("status") not in ALLOWED_DECISION_STATUS:
            fail(f"readiness decision {entry.get('id')} has an invalid status")

    workspace = read_toml(SEGMENTATION / "workspace.toml")
    if workspace.get("schema_version") != 1:
        fail("workspace.toml must use schema_version = 1")
    workspace_entries = workspace.get("repositories", [])
    workspace_ids = [entry.get("id") for entry in workspace_entries]
    if set(workspace_ids) != EXPECTED_REPOSITORIES or len(workspace_ids) != len(set(workspace_ids)):
        fail("workspace.toml must describe every repository exactly once")
    checkouts = [entry.get("checkout") for entry in workspace_entries]
    if len(checkouts) != len(set(checkouts)):
        fail("workspace.toml checkout paths must be unique")
    for entry in workspace_entries:
        expected_url = f"https://github.com/Sagan-Shoulak/{entry['id']}.git"
        if entry.get("url") != expected_url:
            fail(f"workspace repository {entry['id']} has unexpected URL")


def check_schemas_and_templates() -> None:
    schema_directory = SEGMENTATION / "schemas"
    schemas = sorted(schema_directory.glob("*.schema.json"))
    expected = {
        "chat-map.schema.json",
        "component-manifest.schema.json",
        "documentation-export.schema.json",
        "extraction-plan.schema.json",
        "readiness.schema.json",
        "workspace-lock.schema.json",
        "workspace-manifest.schema.json",
    }
    if {path.name for path in schemas} != expected:
        fail("segmentation schema set is incomplete")
    for path in schemas:
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            fail(f"{path.name} does not declare JSON Schema 2020-12")
        if not data.get("$id") or data.get("type") != "object":
            fail(f"{path.name} is missing its ID or root object type")

    for name in ("CODEX_START.md", "MAINTAINERS.md", "TECHNOLOGY.md"):
        if not (SEGMENTATION / "templates" / name).is_file():
            fail(f"missing repository template: {name}")


def check_root_contract() -> None:
    for name in ("README.md", "TECHNOLOGY.md", "MAINTAINERS.md", "CODEX_START.md"):
        if not (ROOT / name).is_file():
            fail(f"current repository is missing root contract file {name}")


def main() -> int:
    try:
        check_ecosystem()
        check_chat_map()
        check_inventory()
        check_components_and_readiness()
        check_schemas_and_templates()
        check_root_contract()
    except (OSError, ValueError, json.JSONDecodeError, tomllib.TOMLDecodeError) as error:
        print(f"Repository segmentation check failed: {error}", file=sys.stderr)
        return 1

    print("Repository segmentation contracts passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
