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
SERVER = ROOT / "bin" / ("sagan-lsp.exe" if WINDOWS_SERVER else "sagan-lsp")


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

        send(process, {"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": "fun main(): Int => missing\n"}],
        }})
        changed = next_message(received)
        assert changed["params"]["version"] == 2
        assert changed["params"]["diagnostics"][0]["code"] == "SAG-SEM-0001"

        send(process, {"jsonrpc": "2.0", "id": 4, "method": "shutdown", "params": {}})
        assert next_message(received)["id"] == 4
        send(process, {"jsonrpc": "2.0", "method": "exit", "params": {}})
        process.stdin.close()
        assert process.wait(timeout=10) == 0
        assert process.stderr.read() == b""
        print("Sagan executable LSP framing, hover, navigation, and diagnostics passed.")
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
