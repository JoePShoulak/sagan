---
title: Editor feature readiness
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor feature readiness

The extension project should use this checklist rather than infer readiness
from compiler version numbers. “Blocked” means the authoritative shared service
contract is not yet implemented; the extension must not reproduce it.

**Handoff status:** the Phase 9 server reliability gate has passed on Windows.
The extension chat can now wire up the LSP features advertised by `initialize`
without duplicating Sagan compiler logic. Rows marked unavailable or blocked
below are deliberate limits, not work for the extension to invent locally.

| Editor feature | Compiler/service gate | Current state |
| --- | --- | --- |
| Open/change/save/close | Versioned document store and overlay resolver | **LSP available; dependency edits refresh open importers** |
| Diagnostics | Structured diagnostics, recovery, UTF-16 conversion, stale-version gate | **LSP publication available; cross-file errors can have generic source locations** |
| Hover | Symbol/type/doc query plus semantic snapshot | **LSP available from shared source/builtin documentation** |
| Go to definition/type definition | Stable symbols and indexed locations | **LSP available, including imported modules** |
| Implementations/conformances | Face/class conformance index | **LSP available** |
| References/highlights | Identity-based workspace reference index | **LSP available** |
| Signature help | Resolved overload/signature query | **LSP available; ambiguous incomplete calls remain conservative** |
| Completion | Parser context, semantic scope/type state, module/catalog metadata | **LSP available for compiler-known candidates; installed imports and unfinished module-path imports work, but full external-package completion/auto-import is blocked** |
| `sagan.toml` editing | Compiler-owned manifest parser over versioned overlays | **Unsaved-buffer diagnostics, section/key and application-mode completion, section/key hover, and a section/key outline available; navigation, formatting, and fixes remain blocked** |
| Semantic highlighting | Stable classification vocabulary and ranges | **Full-document LSP semantic tokens available** |
| Document symbols | Hierarchical declaration index | **LSP available** |
| Workspace symbols | Workspace semantic index | **LSP available for package roots and open module graphs** |
| Folding | Recovering lossless syntax tree | **LSP available for compiler-derived regions** |
| Selection ranges | Syntax parent chains with precise ranges | **LSP available** |
| Document links | Resolved import/module targets | **LSP available** |
| Inlay hints | Resolved types/parameters with suppression rules | **LSP available** |
| Rename | Versioned identity-based safe workspace edits | **LSP available for proven local, private-member, exported-symbol, and imported public-member identities; ambiguous identities are refused** |
| Quick fixes/code actions | Structured fixes and proven refactoring actions | **LSP quick fixes and organize imports available; other unsafe actions disabled** |
| Formatting | Lossless deterministic formatter API | **LSP document/range/on-type available; recovered source formats only proven complete lines and refuses uncertain edits** |
| Type hierarchy | Type/conformance semantic index | **LSP available for indexed conformances** |
| Call hierarchy | Resolved callable/call-site index | **LSP available for resolved calls** |
| Check/build/run tasks | Structured cancellable operations | **Versioned document/project operations and `sagan/operation` transport are available; the extension provides commands and generated document/project tasks with cancellation and output** |
| Test Explorer | Authoritative Sagan test discovery and execution model | **The extension provides document/project discovery and selected or all execution through granular compiler capabilities; Run results include duration, output, and pass/fail/error/skipped state** |
| Debugger | Source maps/runtime metadata, then a separate DAP implementation | **Experimental `sagan-dap` launches on Windows and an isolated Linux test host, maps breakpoints/stacks, filters basic variables, and has source-level stop-on-entry and step-over/in/out probes; reliable values, exceptions, release packaging, and capability advertisement remain blocked** |
| Lexical TextMate coloring | Existing extension grammar | **Already available, outside this task** |

The remaining compiler-side milestones, in dependency order, are:

1. Finish operation stress tests and cancellation checkpoints inside type
   analysis, code generation, and module linking. Strict parsing now checks
   cancellation as it consumes tokens; the transport can cancel queued/active
   requests and child processes, but the remaining compiler loops still have
   some phase-boundary-only checkpoints.
2. **Project test-runner and VS Code client implemented:** imported tests execute from
   linked programs, package-root runs retain package identity, and overlays,
   selection, cancellation, stale runs, and LSP framing have focused tests.
   The extension consumes stable IDs and project runs through Test Explorer.
   A public terminal runner and debug-test profile remain unavailable.
3. Complete contextual package completion and navigation. Manifest dependency
   aliases, exact lockfile verification, offline installed-package linking,
   transitive imports, and navigation/hover for imported package symbols now
   work. The compiler also supplies module-path candidates and bounded
   selective-export candidates for incomplete imports, plus definition targets
   for module paths and definition/hover targets for selective import names.
   Export/member completion in every context, safe import edits,
   missing-source documentation targets, and multi-root collision behavior
   still need implementation and focused tests. No unfinished package
   capability is advertised.
   Imported package-owned value members now have a focused library and LSP
   completion regression, including callable signatures; that is one tested
   slice, not the complete package-completion capability.
   `sagan.toml` entry-module values now navigate to existing local source
   modules from the active manifest snapshot. Locked dependency aliases and
   inline-table package names navigate to installed manifests. Dependency
   value completion and safe manifest edits remain open.
4. Finish the experimental Windows DAP executable using GDB native DAP,
   source maps, and debug metadata. It already has a live launch/breakpoint/
   stack and step-over/in/out probes; verify all source contexts, Sagan scopes
   and values, exceptions,
   cancellation, shutdown, and runtime packaging before
   advertising debugger support.

The extension may begin consuming a feature only when:

1. the language-service library test for that feature passes;
2. the LSP lifecycle/request test passes where applicable;
3. the server advertises the corresponding capability;
4. UTF-16, cancellation, stale-version, and malformed-source behavior is tested;
5. documentation names any deliberate limitation; and
6. no editor-owned syntax, semantics, library catalog, or symbol matching is
   needed to make the feature work.

