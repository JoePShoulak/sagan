---
title: Debug adapter contract
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Debug adapter contract

The repository now builds an **experimental** `bin/sagan-dap.exe` on Windows
(`bin/sagan-dap` on Linux) with `make bin/sagan-dap`. Windows installer and
portable staging now include the adapter, GDB 16.3, and its relocated Python
runtime; the extracted portable payload passed the DAP protocol suite with
only Windows system directories on `PATH`. This is not yet advertised by
compiler capability discovery. Editor clients must not
offer a supported Sagan debug configuration on its presence alone.
The compiler's `sagan.language-service/1` discovery includes the
`debugAdapterExecutable` sibling filename; it is a location contract, not a
claim that the debugger capability is enabled.

The adapter speaks framed DAP on binary stdio and uses `launch` arguments
`{"program":"<absolute .sagan path>"}` or
`{"packageRoot":"<absolute package directory>"}`. Optional `profile` must be
`debug` (default); `cwd` and string-array `args` are accepted. `optimized`
and `env` currently return explicit errors. `stopOnEntry: true` is supported
for a debug build with an executable top-level Sagan statement. The adapter
sets a temporary source-mapped entry breakpoint and removes it before normal
execution resumes, preserving user breakpoints. Launch uses the
compiler's native build operation before passing a
native executable to the bundled GDB DAP backend. `SAGAN_GDB` is a local test
override; production Windows lookup first expects
`<installation>/toolchain/ucrt64/bin/gdb.exe` beside the sibling
`<installation>/bin/sagan-dap.exe`. The initial Windows integration probe covers a Unicode
source path and a manifest-backed package launch, pending-to-verified
breakpoints in entry and imported modules, a Sagan-mapped stack frame and
source-level `next`, `stepIn`, and `stepOut` on a function-call fixture, a
source-mapped `stopOnEntry` followed by a user breakpoint, top-level
binding visibility in a paused scope, output, process termination, malformed
framing, and removal of that session's
temporary native-build artifacts.

This is still a development probe, not a completed debugger: native variable
representations are not yet reliably usable as Sagan values. Source-level stepping now
has Windows top-level-to-function-and-back and imported-module probes, but
still needs method, lambda, and failure-path coverage. Runtime exception
translation needs focused tests; attach and
arbitrary expression evaluation is absent. A guarded scalar lookup probe is
available but does not constitute general debugging support. Installer execution and the
debugger dependency/license review remain release gates. All live debugger capability
flags remain false.

The adapter recognizes generated programs' structured runtime error and
traceback records, then emits one DAP `output` event with the Sagan diagnostic
code, message, traceback notes, and mapped source/line/column. The CLI and
DAP share the same runtime-diagnostic parser. Executable probes cover integer
overflow and assertion failure. This reports a failure after it occurs; it
does not pause on exceptions, so `debugExceptions` remains false.

A direct C++ `std::shared_ptr` dereference through GDB's DAP `evaluate` was
rejected because it invokes an overloaded operator in the debuggee. The
experimental adapter now materializes initialized top-level scalar bindings
in DAP `variables` and accepts their exact names in DAP `evaluate`, only in a
matching source-mapped stack frame after each declaration has completed. It
reads the generated pointer field without a debuggee call and returns the
Sagan type; a pre-initialization function-entry probe and arbitrary expressions
are refused. Parameters, locals, strings, collections, nested values,
optimized builds, and general expression evaluation remain unsupported.
`debugEvaluate` and `debugVariables` remain false.

The Windows prototype uses GDB 16.3's native DAP interpreter behind a
Sagan-owned service. The service, not the editor, builds the selected source
or package, retains its generated-code map, resolves Sagan breakpoints, maps
frames, and owns process cleanup. A generic pass-through to GDB is
insufficient: it would expose generated C++ locations and names.
`attach` and arbitrary paused-expression evaluation remain unsupported until
they have an authoritative runtime implementation and tests.
An isolated Linux build on HP1 with GCC 15.2 and temporary, user-space GDB
17.1 passed the executable DAP launch protocol, installed-package LSP
protocol, package-resolution library, DAP-framing library, and GDB-process
tests. No GDB package was installed system-wide. This does not establish a
distributable Linux debugger package or full value and exception behavior.

## Breakpoint mapping foundation

The compiler library exposes `sagan-dap-breakpoints-v1` through
`src/dap/breakpoints.hpp`. `map_breakpoint` accepts an immutable source
snapshot, its debug metadata and generated C++, and a zero-based UTF-16
position. It returns the executable Sagan source range and one-based generated
line and column, or a refusal reason. It rejects invalid UTF-16 boundaries,
out-of-document lines, and source lines with no executable location. It does
not guess a different line, which could move a breakpoint across a branch or
function. Document identity keeps imported-module breakpoints separate from
the entry file. This mapping is tested with emoji and linked modules.

The prototype `setBreakpoints` handler uses this mapping, replaces the native
set across all Sagan sources in the session, and reports GDB's native
verification. GDB can initially report a pending breakpoint and later send a
verified breakpoint event after loading the generated executable.
Conditional breakpoints, hit counts, and logpoints are rejected explicitly;
they are not silently treated as unconditional breakpoints.

`src/dap/framing.hpp` now provides a bounded stdio DAP frame reader/writer.
It rejects missing, duplicate, malformed, truncated, and over-16-MiB frame
lengths. Focused tests exercise request boundaries and malformed input.

On Windows, `src/dap/gdb_process.hpp` now starts GDB's native DAP interpreter
with separate protocol pipes and a kill-on-close Job Object. A bounded probe
exchanges `initialize` with GDB 16.3 and checks that stopping the wrapper
reaps the child. The executable protocol probe also covers a launch and
debuggee lifecycle on Windows and Linux; further cases listed below remain unverified. The probe skips
when GDB is absent. The Windows portable artifact now supplies GDB, Python,
and supporting DLLs and passes this probe with an isolated `PATH`.

## Gates before discovery becomes true

Before advertising `debugAdapter` or `debugLaunch`, the repository still needs
source-level stepping across all valid constructs, usable Sagan scopes and values,
exception-stop behavior, full cancellation/cleanup tests, installer validation,
debugger dependency/license review, and broader end-to-end protocol tests.
Breakpoint, stepping, variable,
evaluation, exception, and attach capabilities must be advertised separately
and only after their respective tests pass. Release packaging must be checked
outside an MSYS2 runtime `PATH`.

External package imports and exact `sagan.lock` resolution are implemented.
The installed package index and lockfile alone are not enough to promise all
contextual package-owned completion, navigation, or auto-import behavior;
those capabilities remain unadvertised until their protocol tests pass.
