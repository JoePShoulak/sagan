"""Verify installed package symbols through standard LSP requests."""

import os
from pathlib import Path
from queue import Queue
import subprocess
from threading import Thread

from lsp_protocol_test import ROOT, SERVER, read_frames, send, server_file_uri


def run():
    project = ROOT / "tests" / "fixtures" / "catalog" / "consumer-alias"
    entry = project / "src" / "main.sagan"
    installed = ROOT / "tests" / "fixtures" / "catalog" / "orbit-tools" / "src" / "main.sagan"
    environment = os.environ.copy()
    environment["SAGAN_PACKAGE_INDEX"] = str(ROOT / "tests" / "fixtures" / "catalog" / "current-index.tsv")
    process = subprocess.Popen([str(SERVER)], cwd=ROOT, env=environment,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    received = Queue()
    Thread(target=read_frames, args=(process.stdout, received), daemon=True).start()

    def answer(request_id):
        while True:
            item = received.get(timeout=30)
            if isinstance(item, BaseException):
                raise item
            if item.get("id") == request_id:
                assert "error" not in item, item
                return item["result"]

    uri = server_file_uri(entry)
    try:
        send(process, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "workspaceFolders": [{"uri": server_file_uri(project), "name": "consumer-alias"}],
        }})
        assert answer(1)["capabilities"]["definitionProvider"]
        send(process, {"jsonrpc": "2.0", "method": "initialized", "params": {}})
        source = entry.read_text(encoding="utf-8")
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": uri, "languageId": "sagan", "version": 1, "text": source},
        }})
        lines = source.splitlines()
        call_line = next(i for i, line in enumerate(lines) if line == "print(orbit_answer())")
        position = {"line": call_line, "character": lines[call_line].index("orbit_answer") + 2}
        send(process, {"jsonrpc": "2.0", "id": 2, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": position}})
        definition = answer(2)
        assert definition and definition[0]["uri"] == server_file_uri(installed), definition
        send(process, {"jsonrpc": "2.0", "id": 3, "method": "textDocument/hover",
                       "params": {"textDocument": {"uri": uri}, "position": position}})
        hover = answer(3)
        assert hover and "orbit_answer" in str(hover), hover
        import_line = next(i for i, line in enumerate(lines)
                           if line == "import orbit_answer from orbit_tools.main")
        send(process, {"jsonrpc": "2.0", "id": 10, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": import_line,
                           "character": lines[import_line].index("orbit_tools.main") + 2,
                       }}})
        import_target = answer(10)
        assert import_target and import_target[0]["uri"] == server_file_uri(installed), import_target
        member_line = next(i for i, line in enumerate(lines) if "package_tools.orbit_answer" in line)
        send(process, {"jsonrpc": "2.0", "id": 4, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": member_line,
                           "character": lines[member_line].index("orbit_answer") + 2,
                       }}})
        completion = answer(4)
        assert any(item["label"] == "orbit_answer" for item in completion), completion
        partial = source + "import orbit_t"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": partial}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 6, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import orbit_t"),
                       }}})
        imports = answer(6)
        assert any(item["label"] == "orbit_tools.main" for item in imports), imports
        dotted = source + "import orbit_answer from orbit_tools."
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 3},
            "contentChanges": [{"text": dotted}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 7, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines),
                           "character": len("import orbit_answer from orbit_tools."),
                       }}})
        qualified = answer(7)
        assert any(item["label"] == "orbit_tools.main" for item in qualified), qualified
        selective = source + "import orbit_a from orbit_tools.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 4},
            "contentChanges": [{"text": selective}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 8, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import orbit_a"),
                       }}})
        exported = answer(8)["items"]
        orbit = next((item for item in exported if item["label"] == "orbit_answer"), None)
        assert orbit and orbit["textEdit"]["newText"] == "orbit_answer", exported
        assert orbit["data"]["sourceUri"] == server_file_uri(installed), orbit
        complete_import = source + "import orbit_answer from orbit_tools.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 5},
            "contentChanges": [{"text": complete_import}],
        }})
        import_position = {"line": len(lines), "character": len("import orbit_")}
        send(process, {"jsonrpc": "2.0", "id": 11, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": import_position}})
        named_target = answer(11)
        assert named_target and named_target[0]["uri"] == server_file_uri(installed), named_target
        send(process, {"jsonrpc": "2.0", "id": 12, "method": "textDocument/hover",
                       "params": {"textDocument": {"uri": uri}, "position": import_position}})
        assert "orbit_answer" in str(answer(12))
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": server_file_uri(installed), "languageId": "sagan",
                             "version": 1, "text": "module main\nfun overlay_answer(): Int => 7\nexport overlay_answer\n"},
        }})
        overlay_import = source + "import overlay_a from orbit_tools.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 6},
            "contentChanges": [{"text": overlay_import}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 9, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import overlay_a"),
                       }}})
        overlaid = answer(9)["items"]
        assert [item["label"] for item in overlaid] == ["overlay_answer"], overlaid
        complete_overlay = source + "import overlay_answer from orbit_tools.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 7},
            "contentChanges": [{"text": complete_overlay}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 13, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import overlay_")}}})
        overlay_target = answer(13)
        assert overlay_target and overlay_target[0]["uri"] == server_file_uri(installed)
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": server_file_uri(installed), "version": 2},
            "contentChanges": [{"text": "module main\nfun 🚀(): Int => 7\nexport 🚀\n"}],
        }})
        emoji_import = source + "import 🚀 from orbit_tools.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 8},
            "contentChanges": [{"text": emoji_import}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 14, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import ")}}})
        emoji_target = answer(14)
        assert emoji_target and emoji_target[0]["uri"] == server_file_uri(installed)
        assert emoji_target[0]["range"]["start"]["line"] == 1
        send(process, {"jsonrpc": "2.0", "id": 5, "method": "shutdown", "params": {}})
        answer(5)
        send(process, {"jsonrpc": "2.0", "method": "exit", "params": {}})
        assert process.wait(timeout=10) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


if __name__ == "__main__":
    run()
    print("Installed package LSP navigation, hover, and completion passed.")
