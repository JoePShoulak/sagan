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

| Editor feature | Compiler/service gate | Current state |
| --- | --- | --- |
| Open/change/save/close | Versioned document store and overlay resolver | **LSP available; dependency edits refresh open importers** |
| Diagnostics | Structured diagnostics, recovery, UTF-16 conversion, stale-version gate | **LSP publication available; cross-file errors can have generic source locations** |
| Hover | Symbol/type/doc query plus semantic snapshot | **LSP available from shared source/builtin documentation** |
| Go to definition/type definition | Stable symbols and indexed locations | **LSP available, including imported modules** |
| Implementations/conformances | Face/class conformance index | **LSP available** |
| References/highlights | Identity-based workspace reference index | **LSP available** |
| Signature help | Resolved overload/signature query | **LSP available; ambiguous incomplete calls remain conservative** |
| Completion | Parser context, semantic scope/type state, module/catalog metadata | **LSP available for compiler-known candidates; no external package catalog** |
| Semantic highlighting | Stable classification vocabulary and ranges | **Full-document LSP semantic tokens available** |
| Document symbols | Hierarchical declaration index | **LSP available** |
| Workspace symbols | Workspace semantic index | **LSP available for package roots and open module graphs** |
| Folding | Recovering lossless syntax tree | **LSP available for compiler-derived regions** |
| Selection ranges | Syntax parent chains with precise ranges | **LSP available** |
| Document links | Resolved import/module targets | **LSP available** |
| Inlay hints | Resolved types/parameters with suppression rules | **LSP available** |
| Rename | Versioned identity-based safe workspace edits | **LSP available for proven local rename; cross-file/public rename unavailable** |
| Quick fixes/code actions | Structured fixes and proven refactoring actions | **LSP quick fixes and organize imports available; other unsafe actions disabled** |
| Formatting | Lossless deterministic formatter API | **LSP document/range/on-type available for strict source; incomplete-source formatting unsupported** |
| Type hierarchy | Type/conformance semantic index | **LSP available for indexed conformances** |
| Call hierarchy | Resolved callable/call-site index | **LSP available for resolved calls** |
| Check/build/run tasks | Structured cancellable operations | **Versioned check and native document/project build/run library operations available, with streams, artifacts, cancellation, overlays and stale-result checks; asynchronous orchestration and LSP transport blocked** |
| Test explorer | Authoritative Sagan test discovery model | **Unavailable by design for now** |
| Debugger | Source maps/runtime metadata, then a separate DAP implementation | **Source maps, candidate breakpoints, scopes, value metadata and launch plan available; live debugger, attach, optimized-local evaluation and DAP blocked** |
| Lexical TextMate coloring | Existing extension grammar | **Already available, outside this task** |

The extension may begin consuming a feature only when:

1. the language-service library test for that feature passes;
2. the LSP lifecycle/request test passes where applicable;
3. the server advertises the corresponding capability;
4. UTF-16, cancellation, stale-version, and malformed-source behavior is tested;
5. documentation names any deliberate limitation; and
6. no editor-owned syntax, semantics, library catalog, or symbol matching is
   needed to make the feature work.

