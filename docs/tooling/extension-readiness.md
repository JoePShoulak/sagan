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
| Hover | Symbol/type/doc query plus semantic snapshot | **Compiler query and shared source/builtin documentation available; LSP transport blocked** |
| Go to definition/type definition | Stable symbols and indexed locations | **Compiler queries available; LSP transport blocked** |
| Implementations/conformances | Face/class conformance index | **Face implementation query available; LSP transport blocked** |
| References/highlights | Identity-based workspace reference index | **Position queries available; LSP transport blocked** |
| Signature help | Resolved overload/signature query | **Compiler query supports names, overload alternatives and conservative incomplete-call recovery; LSP transport blocked** |
| Completion | Parser context, semantic scope/type state, module/catalog metadata | **Compiler query supports scope, members, modules, imports and contextual keywords; external package catalogs and LSP transport blocked** |
| Semantic highlighting | Stable classification vocabulary and ranges | **Compiler classification query available; LSP semantic-token transport blocked** |
| Document symbols | Hierarchical declaration index | **Hierarchy query available; LSP transport blocked** |
| Workspace symbols | Workspace semantic index | **Workspace search query available; LSP transport blocked** |
| Folding | Recovering lossless syntax tree | **Basic syntax/trivia ranges available; precise nested construct ranges and LSP transport blocked** |
| Selection ranges | Syntax parent chains with precise ranges | **Typed-expression and recovering syntax expansion available; LSP transport blocked** |
| Document links | Resolved import/module targets | **Resolved import link query available; LSP transport blocked** |
| Inlay hints | Resolved types/parameters with suppression rules | **Compiler inferred-type and parameter-name hints available; LSP transport blocked** |
| Rename | Versioned identity-based safe workspace edits | **Blocked** |
| Quick fixes/code actions | Structured fixes and proven refactoring actions | **Blocked** |
| Formatting | Lossless deterministic formatter API | **Blocked** |
| Type hierarchy | Type/conformance semantic index | **Compiler query available for indexed conformances; LSP transport blocked** |
| Call hierarchy | Resolved callable/call-site index | **Compiler query available for resolved calls; LSP transport blocked** |
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

