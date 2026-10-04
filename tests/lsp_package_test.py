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
    environment["SAGAN_PACKAGE_INDEX"] = str(ROOT / "tests" / "fixtures" / "catalog" / "current-compiler-index.tsv")
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
        probe_line = next(i for i, line in enumerate(lines) if "probe.sample(41)" in line)
        type_line = next(i for i, line in enumerate(lines) if "probe: OrbitProbe" in line)
        send(process, {"jsonrpc": "2.0", "id": 16, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": type_line, "character": lines[type_line].index("OrbitProbe") + 2,
                       }}})
        assert answer(16), "Imported class did not resolve in LSP"
        send(process, {"jsonrpc": "2.0", "id": 17, "method": "textDocument/typeDefinition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": probe_line, "character": lines[probe_line].index("probe.sample") + 2,
                       }}})
        assert answer(17), "Imported receiver type did not resolve in LSP"
        send(process, {"jsonrpc": "2.0", "id": 15, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": probe_line,
                           "character": lines[probe_line].index("probe.sample") + len("probe.") + 2,
                       }}})
        probe_completion = answer(15)
        assert any(item["label"] == "sample" and item["detail"] == "(value: Int): Int"
                   for item in probe_completion), probe_completion
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
        assert not imports["isIncomplete"] and any(
            item["label"] == "orbit_tools.main" for item in imports["items"]), imports
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
        assert not qualified["isIncomplete"] and any(
            item["label"] == "orbit_tools.main" for item in qualified["items"]), qualified
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
        unavailable_import = source + "import missing_package.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 9},
            "contentChanges": [{"text": unavailable_import}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 15, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import missing_")}}})
        assert answer(15) == [], "Unavailable package import produced a broken navigation target"
        unavailable_export = source + "import missing from missing_package.main\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 10},
            "contentChanges": [{"text": unavailable_export}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 16, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": uri}, "position": {
                           "line": len(lines), "character": len("import miss")}}})
        unavailable_items = answer(16)
        assert unavailable_items == {"isIncomplete": False, "items": []}, (
            "Unavailable package exports produced a completion protocol error")
        scratch_uri = server_file_uri(project / "src" / "scratch.sagan")
        scratch = "module scratch\nimport orbit_tools.main as package_tools\nfun probe(): Int { return 0 }\n"
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": scratch_uri, "languageId": "sagan", "version": 1,
                             "text": scratch},
        }})
        send(process, {"jsonrpc": "2.0", "id": 17, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": scratch_uri}, "position": {
                           "line": 2, "character": len("fun probe(): Int { ")}}})
        auto_imports = answer(17)
        rocket = next((item for item in auto_imports if item["label"] == "🚀"), None)
        assert rocket and rocket["additionalTextEdits"] == [{
            "range": {"start": {"line": 1, "character": 0},
                      "end": {"line": 1, "character": 0}},
            "newText": "import 🚀 from orbit_tools.main\n",
        }], auto_imports
        manifest_path = project / "sagan.toml"
        manifest_uri = server_file_uri(manifest_path)
        manifest_text = manifest_path.read_text(encoding="utf-8")
        invalid_manifest = manifest_text.replace("[dependencies]", "mystery = \"value\"\n[dependencies]")
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": manifest_uri, "languageId": "toml", "version": 1,
                             "text": invalid_manifest},
        }})
        while True:
            publication = received.get(timeout=30)
            if isinstance(publication, BaseException):
                raise publication
            if publication.get("method") == "textDocument/publishDiagnostics" and (
                    publication["params"]["uri"] == manifest_uri):
                break
        issues = publication["params"]["diagnostics"]
        assert len(issues) == 1 and "mystery" in issues[0]["message"], publication
        assert issues[0]["range"]["start"]["line"] == invalid_manifest.splitlines().index(
            'mystery = "value"'), publication
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 2},
            "contentChanges": [{"text": manifest_text}],
        }})
        while True:
            publication = received.get(timeout=30)
            if isinstance(publication, BaseException):
                raise publication
            if publication.get("method") == "textDocument/publishDiagnostics" and (
                    publication["params"]["uri"] == manifest_uri and
                    publication["params"].get("version") == 2):
                break
        assert publication["params"]["diagnostics"] == [], publication
        entry_line = next(i for i, line in enumerate(manifest_text.splitlines())
                          if line == 'entry = "main"')
        send(process, {"jsonrpc": "2.0", "id": 22, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": entry_line, "character": len('entry = "ma')}}})
        assert answer(22)[0]["uri"] == uri, "Manifest entry did not navigate to the open source"
        dependency_line = next(i for i, line in enumerate(manifest_text.splitlines())
                               if line.startswith("orbit_tools ="))
        send(process, {"jsonrpc": "2.0", "id": 23, "method": "textDocument/definition",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": dependency_line, "character": 2}}})
        dependency_definition = answer(23)
        assert dependency_definition and dependency_definition[0]["uri"] == server_file_uri(
            installed.parent.parent / "sagan.toml"), dependency_definition
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 3},
            "contentChanges": [{"text": "[package]\nna"}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 18, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1, "character": 2}}})
        manifest_candidates = answer(18)
        assert manifest_candidates == [{
            "label": "name", "kind": 14,
            "textEdit": {"range": {"start": {"line": 1, "character": 0},
                                   "end": {"line": 1, "character": 2}},
                         "newText": 'name = ""'},
        }], manifest_candidates
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 4},
            "contentChanges": [{"text": '[application]\nmode = "wi"'}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 19, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1, "character": 10}}})
        mode_candidates = answer(19)
        assert mode_candidates == [{
            "label": "windowed", "kind": 14,
            "textEdit": {"range": {"start": {"line": 1, "character": 7},
                                   "end": {"line": 1, "character": 11}},
                         "newText": '"windowed"'},
        }], mode_candidates
        send(process, {"jsonrpc": "2.0", "id": 20, "method": "textDocument/hover",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1, "character": 2}}})
        mode_hover = answer(20)
        assert "windowed" in mode_hover["contents"]["value"], mode_hover
        assert mode_hover["range"] == {"start": {"line": 1, "character": 0},
                                        "end": {"line": 1, "character": 4}}, mode_hover
        send(process, {"jsonrpc": "2.0", "id": 21, "method": "textDocument/documentSymbol",
                       "params": {"textDocument": {"uri": manifest_uri}}})
        manifest_outline = answer(21)
        assert len(manifest_outline) == 1 and manifest_outline[0]["name"] == "application", (
            manifest_outline)
        assert manifest_outline[0]["children"][0]["name"] == "mode", manifest_outline
        assert manifest_outline[0]["children"][0]["selectionRange"] == {
            "start": {"line": 1, "character": 0}, "end": {"line": 1, "character": 4}}, (
                manifest_outline)
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 5},
            "contentChanges": [{"text": "[dependencies]\norbit_"}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 24, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1, "character": 6}}})
        dependency_candidates = answer(24)
        assert any(item["label"] == "orbit_tools" and item["textEdit"]["newText"] ==
                   'orbit_tools = { package = "orbit-tools", version = "^0.1.0" }'
                   for item in dependency_candidates), dependency_candidates
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 6},
            "contentChanges": [{"text": '[dependencies]\norbit-tools = "^0."'}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 25, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1,
                                               "character": len('orbit-tools = "^0.')}}})
        dependency_versions = answer(25)
        assert any(item["label"] == "^0.1.0" and item["textEdit"]["newText"] == '"^0.1.0"'
                   for item in dependency_versions), dependency_versions
        alias_requirement = 'orbit_tools = { package = "orbit-tools", version = "^0." }'
        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": manifest_uri, "version": 7},
            "contentChanges": [{"text": '[dependencies]\n' + alias_requirement}],
        }})
        send(process, {"jsonrpc": "2.0", "id": 26, "method": "textDocument/completion",
                       "params": {"textDocument": {"uri": manifest_uri},
                                  "position": {"line": 1,
                                               "character": alias_requirement.find('^0.') + 3}}})
        alias_versions = answer(26)
        assert any(item["label"] == "^0.1.0" and item["textEdit"]["newText"] == '"^0.1.0"'
                   for item in alias_versions), alias_versions
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
