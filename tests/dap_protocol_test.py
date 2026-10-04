"""Executable DAP smoke test: framed launch, breakpoint, and native output."""

import json
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path


def frame(message):
    body = json.dumps(message, ensure_ascii=False).encode("utf-8")
    return b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body


def reader(stream, messages):
    try:
        while True:
            headers = {}
            while True:
                line = stream.readline()
                if not line:
                    return
                if line == b"\r\n":
                    break
                name, value = line.decode("ascii").split(":", 1)
                headers[name.lower()] = value.strip()
            count = int(headers["content-length"])
            data = stream.read(count)
            if len(data) != count:
                return
            messages.put(json.loads(data))
    except Exception as error:  # reported on the main test thread
        messages.put(error)


def malformed_frame(binary, environment=None):
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    stdout, stderr = process.communicate(b"Content-Length: nope\r\n\r\n", timeout=10)
    assert stdout == b"", stdout
    assert b"DAP transport error" in stderr, stderr


def missing_debugger(binary):
    environment = os.environ.copy()
    environment["SAGAN_GDB"] = str(Path(tempfile.gettempdir()) / "missing-sagan-gdb.exe")
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    messages = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
    try:
        process.stdin.write(frame({"seq": 1, "type": "request", "command": "initialize",
                                   "arguments": {"adapterID": "sagan"}}))
        process.stdin.flush()
        answer = messages.get(timeout=10)
        assert answer.get("request_seq") == 1 and not answer["success"], answer
        assert "GDB" in answer["message"] or "gdb" in answer["message"], answer
        process.stdin.close()
        assert process.wait(timeout=10) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def cancelled_build(binary, gdb):
    with tempfile.TemporaryDirectory(prefix="sagan cancel ") as folder:
        source = Path(folder) / "cancel.sagan"
        shutil.copyfile("tests/fixtures/runtime/root_script.sagan", source)
        environment = os.environ.copy()
        environment["SAGAN_GDB"] = str(gdb)
        process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env=environment)
        messages = queue.Queue()
        threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()

        def send(seq, command, arguments=None):
            process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                       "arguments": arguments or {}}))
            process.stdin.flush()

        try:
            send(1, "initialize", {"adapterID": "sagan"})
            while messages.get(timeout=15).get("request_seq") != 1:
                pass
            send(2, "launch", {"program": str(source)})
            send(3, "cancel", {"requestId": 2})
            saw_cancel = saw_launch = False
            while not (saw_cancel and saw_launch):
                item = messages.get(timeout=30)
                if item.get("request_seq") == 3:
                    saw_cancel = item["success"]
                if item.get("request_seq") == 2:
                    assert not item["success"] and "cancel" in item["message"].lower(), item
                    saw_launch = True
            send(4, "disconnect", {"terminateDebuggee": True})
            while messages.get(timeout=15).get("request_seq") != 4:
                pass
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def source_stop_on_entry(binary, gdb):
    with tempfile.TemporaryDirectory(prefix="sagan entry ") as folder:
        source = Path(folder) / "entry 🚀.sagan"
        original = Path("tests/fixtures/runtime/root_script.sagan").read_bytes()
        source.write_bytes(original.replace(b"\n", b"\r\n") + b'print("after entry")\r\n')
        environment = os.environ.copy()
        environment["SAGAN_GDB"] = str(gdb)
        process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env=environment)
        messages = queue.Queue()
        threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
        pending = []

        def send(seq, command, arguments=None):
            process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                       "arguments": arguments or {}}))
            process.stdin.flush()

        def expect(predicate):
            for index, item in enumerate(pending):
                if predicate(item):
                    return pending.pop(index)
            while True:
                item = messages.get(timeout=60)
                if isinstance(item, BaseException):
                    raise item
                if predicate(item):
                    return item
                pending.append(item)

        try:
            send(1, "initialize", {"adapterID": "sagan"})
            assert expect(lambda item: item.get("request_seq") == 1)["success"]
            expect(lambda item: item.get("event") == "initialized")
            send(2, "launch", {"program": str(source), "stopOnEntry": True})
            send(7, "setBreakpoints", {"source": {"path": str(source)},
                                       "breakpoints": [{"line": 5, "column": 1}]})
            send(3, "configurationDone")
            assert expect(lambda item: item.get("request_seq") == 2)["success"]
            assert expect(lambda item: item.get("request_seq") == 7)["success"]
            assert expect(lambda item: item.get("request_seq") == 3)["success"]
            stopped = expect(lambda item: item.get("event") == "stopped")
            assert stopped["body"]["reason"] == "entry", stopped
            thread_id = stopped["body"]["threadId"]
            send(4, "stackTrace", {"threadId": thread_id})
            stack = expect(lambda item: item.get("request_seq") == 4)
            assert stack["success"] and stack["body"]["stackFrames"], stack
            top = stack["body"]["stackFrames"][0]
            assert top["source"]["path"] == str(source) and top["line"] == 4, top
            send(5, "continue", {"threadId": thread_id})
            assert expect(lambda item: item.get("request_seq") == 5)["success"]
            breakpoint = expect(lambda item: item.get("event") == "stopped")
            assert breakpoint["body"]["reason"] == "breakpoint", breakpoint
            send(8, "continue", {"threadId": breakpoint["body"]["threadId"]})
            assert expect(lambda item: item.get("request_seq") == 8)["success"]
            expect(lambda item: item.get("event") == "terminated")
            send(6, "disconnect")
            assert expect(lambda item: item.get("request_seq") == 6)["success"]
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def package_launch(binary, gdb):
    environment = os.environ.copy()
    environment["SAGAN_GDB"] = str(gdb)
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    messages = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()

    def send(seq, command, arguments=None):
        process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                   "arguments": arguments or {}}))
        process.stdin.flush()

    try:
        send(1, "initialize", {"adapterID": "sagan"})
        while messages.get(timeout=15).get("request_seq") != 1:
            pass
        send(2, "launch", {"packageRoot": str(Path("examples/package").resolve())})
        send(3, "configurationDone")
        launched = terminated = False
        while not (launched and terminated):
            item = messages.get(timeout=60)
            if item.get("request_seq") == 2:
                assert item["success"], item
                launched = True
            if item.get("event") == "terminated":
                terminated = True
        send(4, "disconnect")
        while messages.get(timeout=15).get("request_seq") != 4:
            pass
        process.wait(timeout=10)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def mapped_runtime_failure(binary, gdb, fixture, expected_code):
    source = Path(fixture).resolve()
    environment = os.environ.copy()
    environment["SAGAN_GDB"] = str(gdb)
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    messages = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()

    def send(seq, command, arguments=None):
        process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                   "arguments": arguments or {}}))
        process.stdin.flush()

    try:
        send(1, "initialize", {"adapterID": "sagan"})
        output_events = []
        while messages.get(timeout=15).get("request_seq") != 1:
            pass
        send(2, "launch", {"program": str(source)})
        send(3, "configurationDone")
        launched = terminated = False
        while not (launched and terminated):
            item = messages.get(timeout=60)
            if isinstance(item, BaseException):
                raise item
            if item.get("request_seq") == 2:
                assert item["success"], item
                launched = True
            if item.get("event") == "output":
                output_events.append(item["body"])
            if item.get("event") == "terminated":
                terminated = True
        failures = [item for item in output_events if expected_code in item.get("output", "")]
        assert len(failures) == 1, output_events
        assert failures[0]["source"]["path"] == str(source), failures[0]
        assert failures[0]["line"] >= 1 and failures[0]["column"] >= 1, failures[0]
        assert not any("SAGAN_RUNTIME_ERROR" in item.get("output", "")
                       for item in output_events), output_events
        send(4, "disconnect")
        while messages.get(timeout=15).get("request_seq") != 4:
            pass
        process.wait(timeout=10)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def imported_module_breakpoint(binary, gdb):
    root = Path("tests/fixtures/modules/module_demo").resolve()
    source = root / "guidance.sagan"
    entry = root / "main.sagan"
    environment = os.environ.copy()
    environment["SAGAN_GDB"] = str(gdb)
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    messages = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
    pending = []

    def send(seq, command, arguments=None):
        process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                   "arguments": arguments or {}}))
        process.stdin.flush()

    def expect(predicate):
        for index, item in enumerate(pending):
            if predicate(item):
                return pending.pop(index)
        while True:
            item = messages.get(timeout=60)
            if isinstance(item, BaseException):
                raise item
            if predicate(item):
                return item
            pending.append(item)

    try:
        send(1, "initialize", {"adapterID": "sagan"})
        assert expect(lambda item: item.get("request_seq") == 1)["success"]
        expect(lambda item: item.get("event") == "initialized")
        send(2, "launch", {"program": str(root / "main.sagan")})
        send(3, "setBreakpoints", {"source": {"path": str(source)},
                                    "breakpoints": [{"line": 6, "column": 1}]})
        answer = expect(lambda item: item.get("request_seq") == 3)
        assert answer["success"] and len(answer["body"]["breakpoints"]) == 1, answer
        send(9, "setBreakpoints", {"source": {"path": str(entry)},
                                    "breakpoints": [{"line": 10, "column": 1}]})
        assert expect(lambda item: item.get("request_seq") == 9)["success"]
        send(4, "configurationDone")
        configured = expect(lambda item: item.get("request_seq") == 4)
        assert configured["success"], configured
        assert expect(lambda item: item.get("request_seq") == 2)["success"]
        stopped = expect(lambda item: item.get("event") == "stopped")
        assert stopped["body"]["reason"] == "breakpoint", stopped
        thread_id = stopped["body"]["threadId"]
        send(5, "stackTrace", {"threadId": thread_id})
        stack = expect(lambda item: item.get("request_seq") == 5)
        assert stack["success"] and stack["body"]["stackFrames"], stack
        assert stack["body"]["stackFrames"][0]["source"]["path"] == str(source), stack
        send(13, "stepIn", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 13)["success"]
        assert expect(lambda item: item.get("event") == "stopped")["body"]["reason"] == "step"
        send(14, "stackTrace", {"threadId": thread_id})
        nested = expect(lambda item: item.get("request_seq") == 14)["body"]["stackFrames"]
        assert nested and nested[0]["name"] in ("helper", "offset") and \
            nested[1]["source"]["path"] == str(source) and \
            nested[1]["name"] == "calculate", nested
        send(15, "stepOut", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 15)["success"]
        assert expect(lambda item: item.get("event") == "stopped")["body"]["reason"] == "step"
        send(16, "stackTrace", {"threadId": thread_id})
        resumed = expect(lambda item: item.get("request_seq") == 16)["body"]["stackFrames"]
        assert resumed and resumed[0]["source"]["path"] == str(source), resumed
        send(6, "setBreakpoints", {"source": {"path": str(source)}, "breakpoints": []})
        assert expect(lambda item: item.get("request_seq") == 6)["success"]
        send(7, "continue", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 7)["success"]
        second = expect(lambda item: item.get("event") == "stopped")
        assert second["body"]["reason"] == "breakpoint", second
        send(10, "stackTrace", {"threadId": thread_id})
        second_stack = expect(lambda item: item.get("request_seq") == 10)
        assert second_stack["body"]["stackFrames"][0]["source"]["path"] == str(entry), second_stack
        send(11, "setBreakpoints", {"source": {"path": str(entry)}, "breakpoints": []})
        assert expect(lambda item: item.get("request_seq") == 11)["success"]
        send(12, "continue", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 12)["success"]
        expect(lambda item: item.get("event") == "terminated")
        send(8, "disconnect")
        assert expect(lambda item: item.get("request_seq") == 8)["success"]
        process.wait(timeout=10)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def source_step_in_out(binary, gdb):
    source = Path("tests/fixtures/runtime/root_script.sagan").resolve()
    environment = os.environ.copy()
    environment["SAGAN_GDB"] = str(gdb)
    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment)
    messages = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
    pending = []

    def send(seq, command, arguments=None):
        process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                   "arguments": arguments or {}}))
        process.stdin.flush()

    def expect(predicate):
        for index, item in enumerate(pending):
            if predicate(item):
                return pending.pop(index)
        while True:
            item = messages.get(timeout=60)
            if isinstance(item, BaseException):
                raise item
            if predicate(item):
                return item
            pending.append(item)

    try:
        send(1, "initialize", {"adapterID": "sagan"})
        assert expect(lambda item: item.get("request_seq") == 1)["success"]
        expect(lambda item: item.get("event") == "initialized")
        send(2, "launch", {"program": str(source)})
        send(3, "setBreakpoints", {"source": {"path": str(source)},
                                    "breakpoints": [{"line": 4, "column": 1}]})
        assert expect(lambda item: item.get("request_seq") == 3)["success"]
        send(4, "configurationDone")
        assert expect(lambda item: item.get("request_seq") == 4)["success"]
        assert expect(lambda item: item.get("request_seq") == 2)["success"]
        stopped = expect(lambda item: item.get("event") == "stopped")
        thread_id = stopped["body"]["threadId"]
        send(11, "setBreakpoints", {"source": {"path": str(source)},
                                     "breakpoints": [{"line": 4, "condition": "false"}]})
        unsupported = expect(lambda item: item.get("request_seq") == 11)
        assert not unsupported["success"] and "Conditional" in unsupported["message"], unsupported
        send(5, "stackTrace", {"threadId": thread_id})
        before_step = expect(lambda item: item.get("request_seq") == 5)
        assert before_step["body"]["stackFrames"][0]["line"] == 4
        send(6, "stepIn", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 6)["success"]
        stepped_in = expect(lambda item: item.get("event") == "stopped")
        assert stepped_in["body"]["reason"] == "step", stepped_in
        send(16, "evaluate", {"frameId": before_step["body"]["stackFrames"][0]["id"],
                               "expression": "offset"})
        assert not expect(lambda item: item.get("request_seq") == 16)["success"]
        send(7, "stackTrace", {"threadId": thread_id})
        inside = expect(lambda item: item.get("request_seq") == 7)["body"]["stackFrames"]
        assert inside and inside[0]["source"]["path"] == str(source) and \
            inside[0]["line"] == 1 and inside[0]["name"] == "answer", inside
        send(14, "evaluate", {"frameId": inside[0]["id"], "expression": "value"})
        assert not expect(lambda item: item.get("request_seq") == 14)["success"]
        send(15, "evaluate", {"frameId": inside[0]["id"], "expression": "offset"})
        assert not expect(lambda item: item.get("request_seq") == 15)["success"]
        send(8, "stepOut", {"threadId": thread_id})
        assert expect(lambda item: item.get("request_seq") == 8)["success"]
        stepped_out = expect(lambda item: item.get("event") == "stopped")
        assert stepped_out["body"]["reason"] == "step", stepped_out
        send(9, "stackTrace", {"threadId": thread_id})
        outside = expect(lambda item: item.get("request_seq") == 9)["body"]["stackFrames"]
        assert outside and outside[0]["source"]["path"] == str(source) and \
            outside[0]["name"] == "<top level>", outside
        send(10, "disconnect", {"terminateDebuggee": True})
        assert expect(lambda item: item.get("request_seq") == 10)["success"]
        process.wait(timeout=10)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def local_scalar_values(binary, gdb):
    with tempfile.TemporaryDirectory(prefix="sagan local values ") as folder:
        source = Path(folder) / "locals 🚀.sagan"
        source.write_text(
            "let local = 1\n"
            "fun compute(value: Int, label: String): Int {\n"
            "  let local = value + 5\n"
            "  let rate = 2.5\n"
            "  let ready = true\n"
            "  let message = \"launch 🚀 \\\\360\"\n"
            "  print(local)\n"
            "  return local\n"
            "}\n"
            "print(compute(2, \"ship 🚀\"))\n", encoding="utf-8")
        environment = os.environ.copy()
        environment["SAGAN_GDB"] = str(gdb)
        process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env=environment)
        messages = queue.Queue()
        threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
        pending = []

        def send(seq, command, arguments=None):
            process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                       "arguments": arguments or {}}))
            process.stdin.flush()

        def expect(predicate):
            for index, item in enumerate(pending):
                if predicate(item):
                    return pending.pop(index)
            while True:
                item = messages.get(timeout=60)
                if isinstance(item, BaseException):
                    raise item
                if predicate(item):
                    return item
                pending.append(item)

        try:
            send(1, "initialize", {"adapterID": "sagan"})
            assert expect(lambda item: item.get("request_seq") == 1)["success"]
            expect(lambda item: item.get("event") == "initialized")
            send(2, "launch", {"program": str(source)})
            send(3, "setBreakpoints", {"source": {"path": str(source)},
                                       "breakpoints": [{"line": 7}, {"line": 8}]})
            assert expect(lambda item: item.get("request_seq") == 3)["success"]
            send(4, "configurationDone")
            assert expect(lambda item: item.get("request_seq") == 4)["success"]
            assert expect(lambda item: item.get("request_seq") == 2)["success"]
            stopped = expect(lambda item: item.get("event") == "stopped")
            send(5, "stackTrace", {"threadId": stopped["body"]["threadId"]})
            stack = expect(lambda item: item.get("request_seq") == 5)
            assert stack["body"]["stackFrames"][0]["line"] == 7, stack
            frame_id = stack["body"]["stackFrames"][0]["id"]
            send(6, "scopes", {"frameId": frame_id})
            scopes = expect(lambda item: item.get("request_seq") == 6)
            send(10, "evaluate", {"frameId": frame_id, "expression": "value", "context": "hover"})
            assert not expect(lambda item: item.get("request_seq") == 10)["success"]
            observed = {}
            for scope in scopes["body"]["scopes"]:
                send(7, "variables", {"variablesReference": scope["variablesReference"]})
                values = expect(lambda item: item.get("request_seq") == 7)
                assert values["success"], values
                observed.update({item["name"]: item["value"]
                                 for item in values["body"]["variables"]})
            assert observed.get("local") == "7" and observed.get("value") == "2", observed
            assert observed.get("rate") == "2.5" and observed.get("ready") == "true", observed
            assert observed.get("message") == '"launch 🚀 \\\\360"', observed
            assert observed.get("label") == '"ship 🚀"', observed
            send(9, "evaluate", {"frameId": frame_id, "expression": "local", "context": "hover"})
            local_value = expect(lambda item: item.get("request_seq") == 9)
            assert local_value["success"] and local_value["body"]["result"] == "7", local_value
            send(11, "evaluate", {"frameId": frame_id, "expression": "value", "context": "hover"})
            parameter_value = expect(lambda item: item.get("request_seq") == 11)
            assert parameter_value["success"] and parameter_value["body"]["result"] == "2", parameter_value
            send(16, "evaluate", {"frameId": frame_id, "expression": "label", "context": "hover"})
            label_value = expect(lambda item: item.get("request_seq") == 16)
            assert label_value["success"] and label_value["body"]["result"] == '"ship 🚀"', label_value
            send(15, "evaluate", {"frameId": frame_id, "expression": "message", "context": "hover"})
            message_value = expect(lambda item: item.get("request_seq") == 15)
            assert message_value["success"] and message_value["body"]["result"] == '"launch 🚀 \\\\360"', message_value
            send(12, "continue", {"threadId": stopped["body"]["threadId"]})
            assert expect(lambda item: item.get("request_seq") == 12)["success"]
            resumed_stop = expect(lambda item: item.get("event") == "stopped")
            send(13, "stackTrace", {"threadId": resumed_stop["body"]["threadId"]})
            resumed_stack = expect(lambda item: item.get("request_seq") == 13)
            resumed_frame = resumed_stack["body"]["stackFrames"][0]["id"]
            send(14, "evaluate", {"frameId": resumed_frame, "expression": "value", "context": "hover"})
            assert not expect(lambda item: item.get("request_seq") == 14)["success"]
            send(8, "disconnect")
            assert expect(lambda item: item.get("request_seq") == 8)["success"]
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def run():
    binary = Path(os.environ.get("SAGAN_DAP_BINARY",
                                 "bin/sagan-dap.exe" if os.name == "nt" else "bin/sagan-dap")).resolve()
    gdb = Path(os.environ.get("SAGAN_TEST_GDB",
                              "C:/msys64/ucrt64/bin/gdb.exe" if os.name == "nt" else "/usr/bin/gdb"))
    if not binary.is_file():
        raise AssertionError(f"Sagan DAP executable is missing: {binary}")
    malformed_frame(binary)
    missing_debugger(binary)
    if os.name == "nt":
        isolated = os.environ.copy()
        isolated["PATH"] = os.pathsep.join([r"C:\Windows\System32", r"C:\Windows"])
        malformed_frame(binary, isolated)
    if not gdb.is_file():
        if os.environ.get("SAGAN_DAP_TEST_REQUIRE_GDB") == "1":
            raise AssertionError(f"GDB is required for the DAP protocol suite: {gdb}")
        print("GDB unavailable; executable launch probe skipped")
        return
    cancelled_build(binary, gdb)
    mapped_runtime_failure(binary, gdb, "tests/fixtures/runtime/source_trace_error.sagan",
                           "SAG-RUN-0101")
    mapped_runtime_failure(binary, gdb, "tests/fixtures/runtime/assert_fail.sagan",
                           "SAG-RUN-0200")
    source_stop_on_entry(binary, gdb)
    package_launch(binary, gdb)
    imported_module_breakpoint(binary, gdb)
    source_step_in_out(binary, gdb)
    local_scalar_values(binary, gdb)
    artifact_root = Path(tempfile.gettempdir()) / "sagan-dap"
    prior_artifacts = set(artifact_root.glob("session-*"))
    with tempfile.TemporaryDirectory(prefix="sagan dap ") as folder:
        source = Path(folder) / "rocket 🚀.sagan"
        shutil.copyfile("tests/fixtures/runtime/root_script.sagan", source)
        source.write_text(source.read_text(encoding="utf-8") + '\nprint("after step")\n',
                          encoding="utf-8")
        environment = os.environ.copy()
        environment["SAGAN_GDB"] = str(gdb)
        process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env=environment)
        messages = queue.Queue()
        thread = threading.Thread(target=reader, args=(process.stdout, messages), daemon=True)
        thread.start()
        pending_messages = []

        def send(seq, command, arguments=None):
            process.stdin.write(frame({"seq": seq, "type": "request", "command": command,
                                       "arguments": arguments or {}}))
            process.stdin.flush()

        def expect(predicate, timeout=30):
            for index, prior in enumerate(pending_messages):
                if predicate(prior):
                    return pending_messages.pop(index)
            while True:
                item = messages.get(timeout=timeout)
                if isinstance(item, BaseException):
                    raise item
                if os.environ.get("SAGAN_DAP_TEST_TRACE"):
                    print("DAP:", ascii(item), flush=True)
                if predicate(item):
                    return item
                pending_messages.append(item)

        try:
            send(1, "initialize", {"adapterID": "sagan", "linesStartAt1": True,
                                    "columnsStartAt1": True, "pathFormat": "path"})
            assert expect(lambda item: item.get("request_seq") == 1)["success"]
            expect(lambda item: item.get("event") == "initialized")
            send(20, "launch", {"program": str(source), "profile": "optimized"})
            assert not expect(lambda item: item.get("request_seq") == 20)["success"]
            send(2, "launch", {"program": str(source), "stopOnEntry": False})
            send(3, "setBreakpoints", {"source": {"path": str(source)},
                                       "breakpoints": [{"line": 4, "column": 1}]})
            breaks = expect(lambda item: item.get("request_seq") == 3)
            assert breaks["success"], breaks
            assert len(breaks["body"]["breakpoints"]) == 1, breaks
            send(4, "configurationDone")
            assert expect(lambda item: item.get("request_seq") == 4)["success"]
            answer = expect(lambda item: item.get("request_seq") == 2)
            assert answer["success"], answer
            stopped = expect(lambda item: item.get("event") == "stopped")
            assert stopped["body"]["reason"] == "breakpoint", stopped
            send(5, "threads")
            threads = expect(lambda item: item.get("request_seq") == 5)
            assert threads["success"] and threads["body"]["threads"], threads
            thread_id = stopped["body"]["threadId"]
            send(6, "stackTrace", {"threadId": thread_id})
            stack = expect(lambda item: item.get("request_seq") == 6)
            assert stack["success"], stack
            assert stack["body"]["stackFrames"], stack
            assert stack["body"]["stackFrames"][0]["source"]["path"] == str(source), stack
            send(9, "scopes", {"frameId": stack["body"]["stackFrames"][0]["id"]})
            scopes = expect(lambda item: item.get("request_seq") == 9)
            assert scopes["success"], scopes
            for scope in scopes["body"]["scopes"]:
                send(10, "variables", {"variablesReference": scope["variablesReference"]})
                assert expect(lambda item: item.get("request_seq") == 10)["success"]
            send(12, "next", {"threadId": thread_id})
            assert expect(lambda item: item.get("request_seq") == 12)["success"]
            expect(lambda item: item.get("event") == "stopped")
            send(13, "stackTrace", {"threadId": thread_id})
            stepped = expect(lambda item: item.get("request_seq") == 13)
            assert stepped["success"] and stepped["body"]["stackFrames"], stepped
            assert stepped["body"]["stackFrames"][0]["line"] == 6, stepped
            send(16, "scopes", {"frameId": stepped["body"]["stackFrames"][0]["id"]})
            stepped_scopes = expect(lambda item: item.get("request_seq") == 16)
            assert stepped_scopes["success"], stepped_scopes
            observed = {}
            for scope in stepped_scopes["body"]["scopes"]:
                send(17, "variables", {"variablesReference": scope["variablesReference"]})
                values = expect(lambda item: item.get("request_seq") == 17)
                assert values["success"], values
                assert all("evaluateName" not in value and "memoryReference" not in value
                           for value in values["body"]["variables"]), values
                observed.update({value["name"]: value["value"]
                                 for value in values["body"]["variables"]})
            assert observed.get("offset") == "2", observed
            send(18, "evaluate", {"frameId": stepped["body"]["stackFrames"][0]["id"],
                                   "expression": "offset", "context": "hover"})
            evaluated = expect(lambda item: item.get("request_seq") == 18)
            assert evaluated["success"] and evaluated["body"]["result"] == "2", evaluated
            assert evaluated["body"]["type"] == "Int64", evaluated
            send(19, "evaluate", {"frameId": stepped["body"]["stackFrames"][0]["id"],
                                   "expression": "offset + 1", "context": "hover"})
            assert not expect(lambda item: item.get("request_seq") == 19)["success"]
            send(11, "setBreakpoints", {"source": {"path": str(source)}, "breakpoints": []})
            cleared = expect(lambda item: item.get("request_seq") == 11)
            assert cleared["success"] and cleared["body"]["breakpoints"] == [], cleared
            send(7, "continue", {"threadId": thread_id})
            assert expect(lambda item: item.get("request_seq") == 7)["success"]
            expect(lambda item: item.get("event") == "terminated")
            send(8, "disconnect", {"terminateDebuggee": True})
            assert expect(lambda item: item.get("request_seq") == 8)["success"]
        finally:
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)
            error_output = process.stderr.read().decode("utf-8", errors="replace")
            if error_output:
                print("DAP stderr:", error_output, flush=True)
        assert set(artifact_root.glob("session-*")) <= prior_artifacts, \
            "DAP left generated build artifacts after disconnect"
        print("Sagan DAP launch protocol passed")


if __name__ == "__main__":
    run()
