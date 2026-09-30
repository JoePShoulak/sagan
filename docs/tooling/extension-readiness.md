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
| Open/change/save/close | Versioned document store and overlay resolver | **Compiler API available; LSP transport blocked** |
| Diagnostics | Structured diagnostics, recovery, UTF-16 conversion, stale-version gate | **Compiler API available; publication/LSP transport blocked** |
| Hover | Symbol/type/doc query plus semantic snapshot | **Blocked** |
| Go to definition/type definition | Stable symbols and indexed locations | **Identity/index foundation available; query and cross-module linking blocked** |
| Implementations/conformances | Face/class conformance index | **Blocked** |
| References/highlights | Identity-based workspace reference index | **Per-document identity index available; workspace/member linking blocked** |
| Signature help | Resolved overload/signature query | **Blocked** |
| Completion | Parser context, semantic scope/type state, module/catalog metadata | **Blocked** |
| Semantic highlighting | Stable classification vocabulary and ranges | **Blocked** |
| Document symbols | Hierarchical declaration index | **Typed declaration foundation available; hierarchy query blocked** |
| Workspace symbols | Workspace semantic index | **Blocked** |
| Folding | Recovering lossless syntax tree | **Foundation available; fine-grained block nodes blocked** |
| Selection ranges | Syntax parent chains with precise ranges | **Foundation available; expression parent chains blocked** |
| Document links | Resolved import/module targets | **Module targets available; position query/LSP transport blocked** |
| Inlay hints | Resolved types/parameters with suppression rules | **Blocked** |
| Rename | Versioned identity-based safe workspace edits | **Blocked** |
| Quick fixes/code actions | Structured fixes and proven refactoring actions | **Blocked** |
| Formatting | Lossless deterministic formatter API | **Blocked** |
| Type hierarchy | Type/conformance semantic index | **Blocked** |
| Call hierarchy | Resolved callable/call-site index | **Blocked** |
| Check/build/run tasks | Structured cancellable operations | **Blocked** |
| Test explorer | Authoritative Sagan test discovery model | **Unavailable by design for now** |
| Debugger | Source maps/runtime metadata, then a separate DAP implementation | **Blocked; DAP is out of scope** |
| Lexical TextMate coloring | Existing extension grammar | **Already available, outside this task** |

The extension may begin consuming a feature only when:

1. the language-service library test for that feature passes;
2. the LSP lifecycle/request test passes where applicable;
3. the server advertises the corresponding capability;
4. UTF-16, cancellation, stale-version, and malformed-source behavior is tested;
5. documentation names any deliberate limitation; and
6. no editor-owned syntax, semantics, library catalog, or symbol matching is
   needed to make the feature work.

