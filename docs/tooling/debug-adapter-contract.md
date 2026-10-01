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
(`bin/sagan-dap` on Linux) with `make bin/sagan-dap`. It is not included in release artifacts and is not
yet advertised by compiler capability discovery. Editor clients must not
offer a supported Sagan debug configuration on its presence alone.
The compiler's `sagan.language-service/1` discovery includes the
`debugAdapterExecutable` sibling filename; it is a location contract, not a
claim that the debugger capability is enabled.

The adapter speaks framed DAP on binary stdio and uses `launch` arguments
`{"program":"<absolute .sagan path>"}` or
`{"packageRoot":"<absolute package directory>"}`. Optional `profile` must be
`debug` (default); `cwd` and string-array `args` are accepted. `optimized`,
`env`, and `stopOnEntry` currently return explicit errors. Launch uses the
compiler's native build operation before passing a
native executable to the bundled GDB DAP backend. `SAGAN_GDB` is a local test
override; production Windows lookup first expects
`<installation>/toolchain/ucrt64/bin/gdb.exe` beside the sibling
`<installation>/bin/sagan-dap.exe`. The initial Windows integration probe covers a Unicode
source path and a manifest-backed package launch, pending-to-verified
breakpoints in entry and imported modules, a Sagan-mapped stack frame and
source-level `next`, top-level
binding visibility in a paused scope, output, process termination, malformed
framing, and removal of that session's
temporary native-build artifacts.

This is still a development probe, not a completed debugger: native variable
representations are not yet usable as Sagan values, reliable source-level
step-in/out and runtime exception translation need focused tests (source-level
`next` has a Windows smoke test), attach and
arbitrary evaluation are absent, Linux transport lacks an execution test, and GDB
plus its Python/runtime closure is not packaged. All live debugger capability
flags remain false.

A direct lookup of a boxed Sagan value through GDB's DAP `evaluate` was also
tested and rejected: GDB refuses the `std::shared_ptr` dereference because its
safe DAP evaluation mode forbids calls into the debuggee. The adapter must not
disable that safety guard or present an address as a Sagan value. Variable
values and evaluation remain unsupported until a compiler-owned debug value
representation can be inspected safely.

The Windows prototype uses GDB 16.3's native DAP interpreter behind a
Sagan-owned service. The service, not the editor, builds the selected source
or package, retains its generated-code map, resolves Sagan breakpoints, maps
frames, and owns process cleanup. A generic pass-through to GDB is
insufficient: it would expose generated C++ locations and names.
`attach` and arbitrary paused-expression evaluation remain unsupported until
they have an authoritative runtime implementation and tests.

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

`src/dap/framing.hpp` now provides a bounded stdio DAP frame reader/writer.
It rejects missing, duplicate, malformed, truncated, and over-16-MiB frame
lengths. Focused tests exercise request boundaries and malformed input.

On Windows, `src/dap/gdb_process.hpp` now starts GDB's native DAP interpreter
with separate protocol pipes and a kill-on-close Job Object. A bounded probe
exchanges `initialize` with GDB 16.3 and checks that stopping the wrapper
reaps the child. The executable protocol probe also covers a launch and
debuggee lifecycle; further cases listed below remain unverified. The probe skips
when GDB is absent; release packaging must make GDB present and test it in
an isolated runtime environment before debugger discovery can become true.

## Gates before discovery becomes true

Before advertising `debugAdapter` or `debugLaunch`, the repository still needs
source-level stepping, usable Sagan scopes and values, runtime-failure mapping,
full cancellation/cleanup tests, Windows release packaging of GDB and its
runtime, Linux execution validation, and broader end-to-end protocol tests. Breakpoint, stepping, variable,
evaluation, exception, and attach capabilities must be advertised separately
and only after their respective tests pass. Release packaging must be checked
outside an MSYS2 runtime `PATH`.

External package imports and lockfile resolution are also still unfinished.
The package index alone is not enough to promise package-owned completion or
navigation; no `packageCompletion` capability is advertised.
