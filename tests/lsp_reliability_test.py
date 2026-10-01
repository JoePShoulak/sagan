"""Bounded end-to-end reliability and performance gate for sagan-lsp."""

import json
import os
from pathlib import Path
from queue import Queue
import subprocess
import sys
import tempfile
from threading import Thread
from time import monotonic

from lsp_protocol_test import ROOT, SERVER, next_message, read_frames, send


MAX_QUERY_SECONDS = 15.0
MAX_EDIT_BURST_SECONDS = 60.0
MAX_REPRESENTATIVE_SECONDS = 30.0


def start_server(logging=False):
    environment = os.environ.copy()
    if logging:
        environment["SAGAN_LSP_LOG"] = "stderr"
    else:
        environment.pop("SAGAN_LSP_LOG", None)
    process = subprocess.Popen(
        [str(SERVER)], cwd=ROOT, env=environment,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    received = Queue()
    Thread(target=read_frames, args=(process.stdout, received), daemon=True).start()
    logs = []
    log_reader = Thread(target=lambda: logs.extend(process.stderr), daemon=True)
    log_reader.start()
    return process, received, logs, log_reader


def response(process, received, identifier, method, params):
    started = monotonic()
    send(process, {"jsonrpc": "2.0", "id": identifier, "method": method, "params": params})
    result = next_message(received)
    elapsed = monotonic() - started
    assert result.get("id") == identifier and "error" not in result, result
    assert elapsed <= MAX_QUERY_SECONDS, f"{method} exceeded {MAX_QUERY_SECONDS}s: {elapsed:.2f}s"
    return result["result"], elapsed


def check_reliability(logging=False):
    process, received, logs, log_reader = start_server(logging)
    workspace = tempfile.TemporaryDirectory(prefix="sagan-lsp-workspace-")
    try:
        package = (ROOT / "examples" / "package").resolve().as_uri()
        second_root = Path(workspace.name)
        (second_root / "src").mkdir()
        (second_root / "sagan.toml").write_text(
            '[package]\nname = "lsp-second"\nversion = "0.1.0"\nsource = "src"\nentry = "main"\n',
            encoding="utf-8",
        )
        (second_root / "src" / "main.sagan").write_text(
            "module main\nfun second_workspace(): Int => 1\nexport second_workspace\n"
            "fun main(): Int => second_workspace()\n", encoding="utf-8",
        )
        module = second_root.resolve().as_uri()
        result, _ = response(process, received, 1, "initialize", {
            "workspaceFolders": [{"uri": package, "name": "package"},
                                 {"uri": module, "name": "module"}],
        })
        assert result["capabilities"]["positionEncoding"] == "utf-16"
        symbols, graph_seconds = response(process, received, 2, "workspace/symbol", {"query": "second_workspace"})
        assert any(item["name"] == "second_workspace" for item in symbols), symbols
        showcase = ROOT / "examples" / "showcase.sagan"
        showcase_uri = showcase.resolve().as_uri()
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": showcase_uri, "languageId": "sagan", "version": 1,
                             "text": showcase.read_text(encoding="utf-8")},
        }})
        assert next_message(received)["method"] == "textDocument/publishDiagnostics"
        representative_start = monotonic()
        for identifier in range(100, 110):
            result, _ = response(process, received, identifier, "textDocument/documentSymbol", {
                "textDocument": {"uri": showcase_uri},
            })
            assert isinstance(result, list)
        representative_seconds = monotonic() - representative_start
        assert representative_seconds <= MAX_REPRESENTATIVE_SECONDS, representative_seconds
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
            "textDocument": {"uri": showcase_uri},
        }})
        assert next_message(received)["params"]["uri"] == showcase_uri
        first = "untitled:sagan-reliability-first"
        second = "untitled:sagan-reliability-second"
        for uri, source in ((first, "fun main(): Int => 0\n"),
                            (second, "fun neighbor(): Int => 1\n")):
            send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
                "textDocument": {"uri": uri, "languageId": "sagan", "version": 1, "text": source},
            }})
            expected = 1 if uri == first else 2
            updates = [next_message(received) for _ in range(expected)]
            assert all(item["method"] == "textDocument/publishDiagnostics" for item in updates)

        started = monotonic()
        for version in range(2, 22):
            source = "fun main(): Int => missing\n" if version % 2 == 0 else "fun main(): Int => 0\n"
            send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
                "textDocument": {"uri": first, "version": version},
                "contentChanges": [{"text": source}],
            }})
        updates = [next_message(received) for _ in range(40)]
        burst_seconds = monotonic() - started
        assert burst_seconds <= MAX_EDIT_BURST_SECONDS, burst_seconds
        first_updates = [item["params"] for item in updates if item["params"]["uri"] == first]
        second_updates = [item["params"] for item in updates if item["params"]["uri"] == second]
        assert [item["version"] for item in first_updates] == list(range(2, 22))
        assert all(item["version"] == 1 and not item["diagnostics"] for item in second_updates)
        assert all(bool(item["diagnostics"]) == (item["version"] % 2 == 0)
                   for item in first_updates)

        # Notifications with an older version must not change the overlay.
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": first, "version": 20},
            "contentChanges": [{"text": "fun main(): Int => 99\n"}],
        }})
        symbols, repeated_seconds = response(process, received, 3, "textDocument/documentSymbol", {
            "textDocument": {"uri": first},
        })
        assert any(item["name"] == "main" for item in symbols), symbols
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": first, "version": 22},
            "contentChanges": [{"text": "fun main( {\n"}],
        }})
        malformed = [next_message(received) for _ in range(2)]
        assert malformed[0]["params"]["version"] == 22 and malformed[0]["params"]["diagnostics"]

        bad = b"{bad"
        process.stdin.write(b"Content-Length: 4\r\n\r\n" + bad)
        process.stdin.flush()
        parse_error = next_message(received)
        assert parse_error["error"]["code"] == -32700, parse_error
        _, recovery_seconds = response(process, received, 4, "textDocument/semanticTokens/full", {
            "textDocument": {"uri": second},
        })

        send(process, {"jsonrpc": "2.0", "method": "workspace/didChangeWorkspaceFolders", "params": {
            "event": {"removed": [{"uri": module, "name": "module"}], "added": []},
        }})
        missing, _ = response(process, received, 5, "workspace/symbol", {"query": "second_workspace"})
        assert not missing, missing
        send(process, {"jsonrpc": "2.0", "method": "workspace/didChangeWorkspaceFolders", "params": {
            "event": {"removed": [], "added": [{"uri": module, "name": "module"}]},
        }})
        restored, _ = response(process, received, 6, "workspace/symbol", {"query": "second_workspace"})
        assert any(item["name"] == "second_workspace" for item in restored), restored

        for uri, expected in ((first, 2), (second, 1)):
            send(process, {"jsonrpc": "2.0", "method": "textDocument/didClose", "params": {
                "textDocument": {"uri": uri},
            }})
            closed = [next_message(received) for _ in range(expected)]
            assert closed[0]["params"]["uri"] == uri and not closed[0]["params"]["diagnostics"]
        _, _ = response(process, received, 7, "shutdown", {})
        send(process, {"jsonrpc": "2.0", "method": "exit", "params": {}})
        process.stdin.close()
        assert process.wait(timeout=10) == 0
        log_reader.join(timeout=10)
        stderr = b"".join(logs).decode("utf-8", errors="replace")
        if logging:
            assert "[sagan-lsp] receive initialize" in stderr
            assert "[sagan-lsp] send response" in stderr
        else:
            assert not stderr, stderr
        return graph_seconds, burst_seconds, max(repeated_seconds, recovery_seconds), representative_seconds
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        workspace.cleanup()


def main():
    if not SERVER.is_file():
        raise AssertionError(f"Missing language server: {SERVER}")
    graph, burst, query, representative = check_reliability()
    check_reliability(logging=True)
    print(f"LSP reliability passed: multi-root, 20-edit burst, stale version, malformed input, "
          f"workspace cleanup, and stderr-only logging. "
          f"Timings: graph {graph:.3f}s, burst {burst:.3f}s, query {query:.3f}s, "
          f"10 showcase queries {representative:.3f}s.")


if __name__ == "__main__":
    try:
        main()
    except Exception as failure:
        print(f"LSP reliability test failed: {failure}", file=sys.stderr)
        raise
