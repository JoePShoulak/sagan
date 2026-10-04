"""End-to-end stdio protocol fixture for the compiled Sagan language server."""

import json
import os
from pathlib import Path, PureWindowsPath
from queue import Queue
import subprocess
import sys
from threading import Thread


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_SERVER = os.name == "nt" or bool(os.environ.get("MSYSTEM"))
SERVER = Path(os.environ["SAGAN_LSP_BINARY"]) if os.environ.get("SAGAN_LSP_BINARY") else (
    ROOT / "bin" / ("sagan-lsp.exe" if WINDOWS_SERVER else "sagan-lsp")
)


def server_file_uri(path):
    """Use the native server's Windows paths when Python runs inside MSYS2."""
    resolved = path.resolve()
    if WINDOWS_SERVER and os.name != "nt":
        windows_path = subprocess.check_output(
            ["cygpath", "-w", str(resolved)], text=True
        ).strip()
        return PureWindowsPath(windows_path).as_uri()
    return resolved.as_uri()


def send(process, message):
    body = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    process.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)
    process.stdin.flush()


def read_frames(stream, received):
    try:
        while True:
            length = None
            while True:
                line = stream.readline()
                if not line:
                    received.put(EOFError("Server closed protocol stdout"))
                    return
                if line == b"\r\n":
                    break
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip())
                else:
                    raise AssertionError(f"Non-protocol stdout header: {line!r}")
            if length is None or length > 16 * 1024 * 1024:
                raise AssertionError("Missing or oversized Content-Length")
            body = stream.read(length)
            if len(body) != length:
                raise EOFError("Truncated LSP response")
            received.put(json.loads(body))
    except Exception as failure:  # surfaced to the main test thread
        received.put(failure)


def next_message(received):
    message = received.get(timeout=20)
    if isinstance(message, Exception):
        raise message
    return message


def main():
    if not SERVER.is_file():
        raise AssertionError(f"Language server executable is missing: {SERVER}")
    process = subprocess.Popen(
        [str(SERVER)], cwd=ROOT, stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    received = Queue()
    reader = Thread(target=read_frames, args=(process.stdout, received), daemon=True)
    reader.start()
    try:
        package_uri = server_file_uri(ROOT / "examples" / "package")
        send(process, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "workspaceFolders": [{"uri": package_uri, "name": "package"}],
        }})
        initialized = next_message(received)
        assert initialized["id"] == 1
        assert initialized["result"]["capabilities"]["positionEncoding"] == "utf-16"
        assert initialized["result"]["capabilities"]["hoverProvider"] is True

        index_uri = server_file_uri(ROOT / "tests" / "fixtures" / "package_index" / "index.tsv")
        send(process, {"jsonrpc": "2.0", "id": 4, "method": "sagan/packages/query", "params": {
            "indexUri": index_uri, "prefix": "physics",
        }})
        catalog = next_message(received)
        assert catalog["id"] == 4 and catalog["result"]["schema"] == "sagan-package-index-v1"
        assert catalog["result"]["state"] == "ready"
        assert len(catalog["result"]["packages"]) == 2

        source_index_uri = server_file_uri(ROOT / "tests" / "fixtures" / "catalog" / "index.tsv")
        send(process, {"jsonrpc": "2.0", "id": 6, "method": "sagan/packages/catalog", "params": {
            "indexUri": source_index_uri, "prefix": "orbit", "limit": 1,
        }})
        catalog_source = next_message(received)
        assert catalog_source["id"] == 6
        assert catalog_source["result"]["schema"] == "sagan-package-catalog-v1"
        assert any(item["name"] == "orbit_answer" for item in
                   catalog_source["result"]["packages"][0]["modules"][0]["exports"])

        send(process, {"jsonrpc": "2.0", "id": 5, "method": "workspace/symbol", "params": {
            "query": "main",
        }})
        symbols = next_message(received)
        assert symbols["id"] == 5 and any(item["name"] == "main" for item in symbols["result"])

        uri = "untitled:sagan-lsp-process"
        source = "fun 🚀(value: Int): Int => value + 2\nfun main(): Int => 🚀(40)\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": uri, "languageId": "sagan", "version": 1, "text": source},
        }})
        opened = next_message(received)
        assert opened["method"] == "textDocument/publishDiagnostics"
        assert opened["params"]["diagnostics"] == []

        test_uri = "untitled:sagan-test-discovery-process"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": test_uri, "languageId": "sagan", "version": 1,
                             "text": 'test "orbit 🚀" { print(1) }\n'},
        }})
        assert next_message(received)["method"] == "textDocument/publishDiagnostics"
        send(process, {"jsonrpc": "2.0", "id": 21, "method": "sagan/tests/discover", "params": {
            "textDocument": {"uri": test_uri, "version": 1},
        }})
        discovered = next_message(received)
        while discovered.get("id") != 21:
            assert discovered.get("method") == "textDocument/publishDiagnostics"
            discovered = next_message(received)
        assert discovered["id"] == 21
        assert discovered["result"]["schema"] == "sagan-tests-v1"
        assert discovered["result"]["tests"][0]["name"] == "orbit 🚀"
        assert discovered["result"]["tests"][0]["range"]["end"]["character"] == 15
        test_id = discovered["result"]["tests"][0]["id"]
        send(process, {"jsonrpc": "2.0", "id": 20, "method": "sagan/tests/run", "params": {
            "textDocument": {"uri": test_uri, "version": 1}, "testIds": [test_id],
        }})
        test_events = []
        while True:
            message = next_message(received)
            if message.get("id") == 20:
                test_run = message["result"]
                break
            test_events.append(message)
        assert test_run["schema"] == "sagan-tests-v1"
        assert test_run["tests"][0]["state"] == "passed"
        assert any(item.get("method") == "sagan/testEvent" for item in test_events)
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": test_uri},
        }})
        assert next_message(received)["method"] == "textDocument/publishDiagnostics"

        project_path = ROOT / "tests" / "fixtures" / "modules" / "tests_project" / "main.sagan"
        project_uri = server_file_uri(project_path)
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": project_uri, "languageId": "sagan", "version": 1,
                             "text": project_path.read_text(encoding="utf-8")},
        }})
        assert next_message(received)["method"] == "textDocument/publishDiagnostics"
        send(process, {"jsonrpc": "2.0", "id": 23, "method": "sagan/tests/discover", "params": {
            "scope": "project", "projectUri": project_uri,
            "textDocument": {"uri": project_uri, "version": 1},
        }})
        while True:
            project_message = next_message(received)
            if project_message.get("id") == 23:
                break
            assert project_message.get("method") == "textDocument/publishDiagnostics"
        project_tests = project_message["result"]["tests"]
        assert project_message["result"]["state"] == "complete"
        assert len(project_tests) == 2
        send(process, {"jsonrpc": "2.0", "id": 24, "method": "sagan/tests/run", "params": {
            "scope": "project", "projectUri": project_uri,
            "textDocument": {"uri": project_uri, "version": 1},
            "testIds": [project_tests[0]["id"]],
        }})
        project_events = []
        while True:
            project_message = next_message(received)
            if project_message.get("id") == 24:
                break
            project_events.append(project_message)
        assert project_message["result"]["state"] == "complete"
        assert len(project_message["result"]["tests"]) == 1
        assert project_message["result"]["tests"][0]["state"] == "passed"
        assert any(item.get("method") == "sagan/testEvent" for item in project_events)
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": project_uri},
        }})
        assert next_message(received)["method"] == "textDocument/publishDiagnostics"

        send(process, {"jsonrpc": "2.0", "id": 22, "method": "sagan/operation", "params": {
            "kind": "check", "scope": "document",
            "textDocument": {"uri": uri, "version": 1},
            "workDoneToken": "check-process",
        }})
        operation_messages = []
        while True:
            message = next_message(received)
            if message.get("id") == 22:
                operation = message["result"]
                break
            operation_messages.append(message)
        assert operation["schema"] == "sagan-operations-v2"
        assert operation["state"] == "completed" and operation["exitStatus"] == 0
        assert operation["diagnostics"] == [] and operation["version"] == 1
        assert any(item.get("method") == "sagan/operationEvent" for item in operation_messages)
        assert any(item.get("method") == "$/progress" for item in operation_messages)

        send(process, {"jsonrpc": "2.0", "id": 23, "method": "sagan/operation", "params": {
            "kind": "check", "scope": "document",
            "textDocument": {"uri": uri, "version": 0},
        }})
        stale_operation = next_message(received)
        assert stale_operation["id"] == 23 and stale_operation["error"]["code"] == -32602

        loop_uri = "untitled:sagan-cancel-running-process"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": loop_uri, "languageId": "sagan", "version": 1,
                             "text": "while true {}\n"},
        }})
        while True:
            notice = next_message(received)
            if notice.get("method") == "textDocument/publishDiagnostics" and \
                    notice["params"]["uri"] == loop_uri:
                assert notice["params"]["diagnostics"] == []
                break
        send(process, {"jsonrpc": "2.0", "id": 24, "method": "sagan/operation", "params": {
            "kind": "run", "scope": "document",
            "textDocument": {"uri": loop_uri, "version": 1},
        }})
        cancelled_child = False
        while True:
            notice = next_message(received)
            if notice.get("method") == "sagan/operationEvent" and \
                    notice["params"]["text"] == "Running native program" and not cancelled_child:
                send(process, {"jsonrpc": "2.0", "method": "$/cancelRequest",
                               "params": {"id": 24}})
                cancelled_child = True
            if notice.get("id") == 24:
                assert cancelled_child and notice["result"]["state"] == "cancelled"
                assert notice["result"]["exitStatus"] is None
                assert notice["result"]["diagnostics"] == []
                assert notice["result"]["executable"] is None
                break
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": loop_uri},
        }})
        closed_uris = {next_message(received)["params"]["uri"] for _ in range(2)}
        assert closed_uris == {uri, loop_uri}

        point = {"line": 1, "character": 19}
        send(process, {"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover", "params": {
            "textDocument": {"uri": uri}, "position": point,
        }})
        hover = next_message(received)
        assert hover["id"] == 2 and "🚀" in hover["result"]["contents"]["value"]

        send(process, {"jsonrpc": "2.0", "id": 3, "method": "textDocument/definition", "params": {
            "textDocument": {"uri": uri}, "position": point,
        }})
        definition = next_message(received)
        assert definition["id"] == 3 and definition["result"][0]["uri"] == uri

        send(process, {"jsonrpc": "2.0", "id": 6, "method": "textDocument/rename", "params": {
            "textDocument": {"uri": uri}, "position": point, "newName": "launch",
        }})
        renamed = next_message(received)
        assert renamed["id"] == 6
        assert len(renamed["result"]["documentChanges"]) == 1
        assert len(renamed["result"]["documentChanges"][0]["edits"]) == 2

        grouped_source = (
            "fun main(): Int {\n"
            "  let n = 10\n"
            "  let i, a, b = 0, 0, 1\n"
            "  while i++ <= n  a, b = b, a+b\n"
            "  return b\n"
            "}\n"
        )
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": grouped_source}],
        }})
        grouped = next_message(received)
        assert grouped["params"]["version"] == 2
        assert grouped["params"]["diagnostics"] == [], grouped["params"]["diagnostics"]

        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 3},
            "contentChanges": [{"text": "fun main(): Int => missing\n"}],
        }})
        changed = next_message(received)
        assert changed["params"]["version"] == 3
        assert changed["params"]["diagnostics"][0]["code"] == "SAG-SEM-0001"

        member_path = ROOT / "tests" / "fixtures" / "modules" / "workspace_member_rename" / "main.sagan"
        member_uri = server_file_uri(member_path)
        member_source = member_path.read_text(encoding="utf-8")
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": member_uri, "languageId": "sagan", "version": 1,
                             "text": member_source},
        }})
        member_diagnostics = next_message(received)
        assert member_diagnostics["method"] == "textDocument/publishDiagnostics"
        member_line = member_source.splitlines()[4]
        member_point = {"line": 4, "character": member_line.index("sample") + 1}
        send(process, {"jsonrpc": "2.0", "id": 7, "method": "textDocument/rename", "params": {
            "textDocument": {"uri": member_uri}, "position": member_point, "newName": "measure",
        }})
        member_renamed = next_message(received)
        while "id" not in member_renamed:
            member_renamed = next_message(received)
        assert member_renamed["id"] == 7
        member_changes = member_renamed["result"]["documentChanges"]
        assert len(member_changes) == 2
        assert sum(len(change["edits"]) for change in member_changes) == 3

        independent_uri = "untitled:independent-f2-scopes"
        independent_source = (
            "fun first(): Int { let value = 1\n return value }\n"
            "fun second(): Int { let other = 2\n return other }\n"
        )
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": independent_uri, "languageId": "sagan", "version": 1,
                             "text": independent_source},
        }})
        independent_diagnostics = next_message(received)
        while (independent_diagnostics.get("method") != "textDocument/publishDiagnostics" or
               independent_diagnostics["params"]["uri"] != independent_uri):
            independent_diagnostics = next_message(received)
        assert independent_diagnostics["method"] == "textDocument/publishDiagnostics"
        assert independent_diagnostics["params"]["diagnostics"] == [], independent_diagnostics
        send(process, {"jsonrpc": "2.0", "id": 8, "method": "textDocument/prepareRename", "params": {
            "textDocument": {"uri": independent_uri},
            "position": {"line": 2, "character": 24},
        }})
        prepared_independent = next_message(received)
        while prepared_independent.get("id") != 8:
            prepared_independent = next_message(received)
        assert prepared_independent["id"] == 8
        assert prepared_independent["result"]["placeholder"] == "other"
        send(process, {"jsonrpc": "2.0", "id": 9, "method": "textDocument/rename", "params": {
            "textDocument": {"uri": independent_uri},
            "position": {"line": 2, "character": 24}, "newName": "value",
        }})
        renamed_independent = next_message(received)
        while renamed_independent.get("id") != 9:
            renamed_independent = next_message(received)
        assert renamed_independent["id"] == 9
        assert len(renamed_independent["result"]["documentChanges"]) == 1
        assert len(renamed_independent["result"]["documentChanges"][0]["edits"]) == 2
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": independent_uri},
        }})

        local_type_uri = "untitled:local-type-f2"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": local_type_uri, "languageId": "sagan", "version": 1,
                             "text": "class Probe { new() {} }\nlet probe = Probe()\nprint(probe)\n"},
        }})
        opened_type = next_message(received)
        while (opened_type.get("method") != "textDocument/publishDiagnostics" or
               opened_type["params"]["uri"] != local_type_uri):
            opened_type = next_message(received)
        assert opened_type["params"]["diagnostics"] == []
        send(process, {"jsonrpc": "2.0", "id": 10, "method": "textDocument/rename", "params": {
            "textDocument": {"uri": local_type_uri},
            "position": {"line": 1, "character": 13}, "newName": "Sensor",
        }})
        renamed_type = next_message(received)
        while renamed_type.get("id") != 10:
            renamed_type = next_message(received)
        assert len(renamed_type["result"]["documentChanges"]) == 1
        assert len(renamed_type["result"]["documentChanges"][0]["edits"]) == 2
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": local_type_uri},
        }})

        send(process, {"jsonrpc": "2.0", "id": 4, "method": "shutdown", "params": {}})
        shutdown = next_message(received)
        while shutdown.get("id") != 4:
            shutdown = next_message(received)
        assert shutdown["id"] == 4
        send(process, {"jsonrpc": "2.0", "method": "exit", "params": {}})
        process.stdin.close()
        assert process.wait(timeout=10) == 0
        assert process.stderr.read() == b""
        print("Sagan executable LSP framing, operations, cancellation, hover, F2 rename, and diagnostics passed.")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


if __name__ == "__main__":
    try:
        main()
    except Exception as failure:
        print(f"LSP executable test failed: {failure}", file=sys.stderr)
        raise
