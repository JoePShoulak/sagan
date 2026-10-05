from __future__ import annotations

import csv
import fnmatch
import json
from pathlib import Path
import subprocess
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


def check_primary_transfer() -> None:
    data = read_toml(SEGMENTATION / "primary-transfer.toml")
    if data.get("schema_version") != 1:
        fail("primary-transfer.toml must use schema_version = 1")
    if data.get("source") != "JoePShoulak/sagan":
        fail("primary-transfer.toml has an unexpected source repository")
    if data.get("destination") != "Sagan-Shoulak/sagan":
        fail("primary-transfer.toml has an unexpected destination repository")
    if data.get("required_default_branch") != "dev":
        fail("primary-transfer.toml must preserve dev as the default branch")
    if data.get("required_visibility") != "public":
        fail("primary-transfer.toml must preserve the source repository's public visibility")
    if data.get("split_repository_creation_blocked_until_verified") is not True:
        fail("primary transfer must block split repository creation until verified")

    github = data.get("github", {})
    if github.get("transfer_policy") != "preserve-observed-settings-before-tightening":
        fail("primary transfer must preserve observed settings before tightening")
    if github.get("release_policy") != "paused-until-owner-decides-publication-policy":
        fail("primary transfer must keep all releases paused")
    if github.get("secret_names") != ["SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY"]:
        fail("primary transfer must inventory the observed repository secret")
    if github.get("deferred_signing_secret_names") != ["SAGAN_SIGNTOOL_COMMAND"]:
        fail("primary transfer must record the deferred signing secret separately")
    expected_workflows = {path.name for path in (ROOT / ".github" / "workflows").glob("*.yml")}
    if set(github.get("workflows", [])) != expected_workflows:
        fail("primary-transfer.toml workflow inventory is stale")

    legacy_paths = [entry.get("path") for entry in data.get("legacy_references", [])]
    if len(legacy_paths) != len(set(legacy_paths)):
        fail("primary-transfer.toml contains duplicate legacy reference paths")
    for path in legacy_paths:
        if not isinstance(path, str) or not (ROOT / path).is_file():
            fail(f"primary-transfer.toml legacy reference does not exist: {path}")

    search_roots = [
        ROOT / "README.md",
        ROOT / "MAINTAINERS.md",
        ROOT / "docs",
        ROOT / "packaging",
        ROOT / "deploy",
        ROOT / "scripts",
    ]
    ignored_files = {
        ROOT / "scripts" / "primary_repository_transfer_audit.sh",
        ROOT / "scripts" / "repository_segmentation_check.py",
    }
    discovered_legacy_paths: set[str] = set()
    for search_root in search_roots:
        candidates = [search_root] if search_root.is_file() else search_root.rglob("*")
        for candidate in candidates:
            if not candidate.is_file() or candidate in ignored_files:
                continue
            try:
                contents = candidate.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            if "JoePShoulak/sagan" in contents:
                discovered_legacy_paths.add(candidate.relative_to(ROOT).as_posix())
    if set(legacy_paths) != discovered_legacy_paths:
        fail(
            "primary-transfer.toml legacy reference inventory is stale: "
            f"expected {sorted(discovered_legacy_paths)}, recorded {sorted(legacy_paths)}"
        )

    integration_ids = [
        entry.get("id") for entry in data.get("external_integrations", [])
    ]
    if len(integration_ids) != len(set(integration_ids)):
        fail("primary-transfer.toml contains duplicate external integration IDs")
    if any(not entry.get("verification") for entry in data.get("external_integrations", [])):
        fail("every primary transfer integration requires a verification rule")


def check_vscode_relocation() -> None:
    data = read_toml(SEGMENTATION / "vscode-relocation.toml")
    if data.get("schema_version") != 1 or data.get("repository") != "sagan-vscode":
        fail("vscode-relocation.toml has an invalid version or repository")
    prefix = "editors/vscode-sagan/"
    if data.get("subtree_source") != prefix or data.get("subtree_destination") != "":
        fail("vscode-relocation.toml has an invalid subtree mapping")

    manifest_path = SEGMENTATION / "file-manifests" / "sagan-vscode.txt"
    manifest = set(manifest_path.read_text(encoding="utf-8").splitlines())
    shared_sources = {path for path in manifest if not path.startswith(prefix)}
    mappings = data.get("shared_files", [])
    sources = [entry.get("source") for entry in mappings]
    if len(sources) != len(set(sources)) or set(sources) != shared_sources:
        fail("vscode-relocation.toml must map every owned file outside the extension subtree exactly once")

    destinations = [entry.get("destination") for entry in mappings]
    if len(destinations) != len(set(destinations)):
        fail("vscode-relocation.toml has duplicate destination paths")
    relocated_subtree = {path.removeprefix(prefix) for path in manifest if path.startswith(prefix)}
    if set(destinations) & relocated_subtree:
        fail("vscode-relocation.toml destination collides with the relocated extension subtree")
    for entry in mappings:
        destination = entry.get("destination")
        if not isinstance(destination, str) or destination.startswith("/") or ".." in Path(destination).parts:
            fail(f"vscode-relocation.toml has an unsafe destination: {destination}")
        if not entry.get("rewrite"):
            fail(f"vscode-relocation.toml omits rewrite instructions for {entry.get('source')}")


def check_schemas_and_templates() -> None:
    schema_directory = SEGMENTATION / "schemas"
    schemas = sorted(schema_directory.glob("*.schema.json"))
    expected = {
        "chat-map.schema.json",
        "component-manifest.schema.json",
        "documentation-export.schema.json",
        "extraction-plan.schema.json",
        "primary-transfer.schema.json",
        "readiness.schema.json",
        "vscode-relocation.schema.json",
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


def check_file_manifests() -> None:
    subprocess.run(
        [sys.executable, "scripts/repository_segmentation_manifest.py", "--check"],
        cwd=ROOT,
        check=True,
    )


def main() -> int:
    try:
        check_ecosystem()
        check_chat_map()
        check_inventory()
        check_components_and_readiness()
        check_primary_transfer()
        check_vscode_relocation()
        check_schemas_and_templates()
        check_root_contract()
        check_file_manifests()
    except (OSError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError,
            tomllib.TOMLDecodeError) as error:
        print(f"Repository segmentation check failed: {error}", file=sys.stderr)
        return 1

    print("Repository segmentation contracts passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
