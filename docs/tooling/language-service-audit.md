---
title: Language-service readiness audit
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Language-service readiness audit

This audit records the compiler state before editor-tooling infrastructure was
added. It is a historical baseline, so its “missing” tables describe the
starting point rather than today's implementation. Current progress is tracked
in the [language-service roadmap](language-service-roadmap.md) and
[extension readiness checklist](extension-readiness.md). It is not permission
to redesign Sagan 1.0.
Editor integrations must consume the compiler's rules rather than reproduce
them in TypeScript, TextMate metadata, or another editor-specific database.

## Existing foundations

| Area | Existing implementation | Editor-tooling assessment |
| --- | --- | --- |
| Lexer | Unicode-aware tokenization, byte spans, documentation-comment tokens, comprehensive self-tests | **Ready to expose after source identities, structured results, trivia, cancellation, and recovery are added** |
| Parser and AST | Central C++ parser and owned AST with byte spans on nodes | **Ready to expose after tolerant parsing, node identities, finer declaration-name ranges, trivia preservation, and multiple diagnostics** |
| Semantic analyzer | Nested scopes, declarations, name resolutions, declaration/use spans | **Ready to expose, but symbol records need stable typed identities and document ownership** |
| Type checker | Inferred expression types, declaration types, conformance and entry-point validation | **Ready to expose, but results need symbol/node linkage, structured errors, cancellation, and partial-program behavior** |
| Module and package resolver | Authoritative manifest discovery, package roots, import/export validation, module graphs, linking | **Ready to expose after filesystem abstraction and in-memory overlays** |
| Documentation | Documentation comments attach to supported AST declarations | **Ready to expose after symbol association and a unified source/built-in documentation model** |
| Code generator | Central C++ generation for the executable language subset | **Ready to refactor behind build operations; source maps and debug metadata are absent** |
| Native runner | Compiler discovery, temporary native build, process execution and exit propagation | **Ready to refactor; progress, cancellation, structured output, and source-mapped failures are absent** |
| CLI | Batch tokens, AST, semantic/type inspection, module/package operations, C++ emission, build/run, demos | **Must become a presentation client of reusable libraries instead of the integration API** |
| Project model | Optional `sagan.toml`, package discovery, source root, entry module, console/windowed mode | **Authoritative workspace seed; multi-root workspace state and file notifications are absent** |
| Tests | Large in-process compiler self-test plus Bash CLI, module, package, demo, coverage, documentation, and installer tests | **Strong regression base; focused library and LSP protocol test executables are still needed** |
| Build | Make-based monolithic compiler executable and Windows launcher/installer | **Needs reusable library targets and a separate language-server executable** |

## Missing editor prerequisites

The present compiler is batch-oriented:

- `parser::span` contains signed byte offsets only. It has no document identity,
  URI, document version, line index, or UTF-16 conversion.
- lexical, parse, semantic, type, module, and project failures are exceptions
  with human-readable strings. Most passes stop at the first failure.
- ordinary comments and whitespace are discarded. Documentation comments are
  preserved, but the AST alone cannot safely format or rewrite source.
- AST children such as names, parameters, type annotations, and operators often
  lack their own ranges and stable node identities.
- symbols are strings and ranges. They do not distinguish all declaration
  kinds, overloads, shadowed locals, built-ins, synthetic declarations, or
  specializations with durable identities.
- the resolver reads files directly with `std::ifstream`; unsaved overlays
  cannot participate in import resolution.
- compilation state is reconstructed per command. There is no document store,
  workspace cache, dependency invalidation, request cancellation, or stale
  version rejection.
- the CLI owns file reading, presentation, and orchestration. There is no stable
  structured service or protocol schema.
- there is no formatter, source-edit engine, source-map/debug metadata, JSON-RPC
  transport, or Language Server Protocol implementation.
- built-in names exist in semantic analysis, but there is no versioned,
  compiler-readable standard-library symbol/documentation catalog.
- Sagan has no implemented language-level test declaration/discovery model.
  Editor test discovery must remain capability-disabled until that language or
  project contract exists; filenames or textual conventions are not authority.

## Requirement classification

“New infrastructure” means editor-oriented compiler infrastructure shared by
all clients, not logic placed in a particular editor extension.

| Requirement | Classification | Reason |
| --- | --- | --- |
| 1. Reusable language service | **Requires new editor-specific compiler infrastructure** | Compiler phases are reusable functions, but orchestration and state live in the CLI or local pass objects. |
| 2. Machine-readable interfaces | **Requires new infrastructure** | Byte spans exist; URI/version/UTF-16/result/schema/capability contracts do not. |
| 3. Unsaved overlays | **Requires new infrastructure** | Resolver and CLI are disk-only and stateless. |
| 4. Error recovery | **Requires new infrastructure** | Lexer/parser and later passes throw and normally stop at one error. |
| 5. Structured diagnostics | **Ready to expose plus new infrastructure** | Every phase identifies failures, but codes, severities, related locations, fixes, and collections are absent. |
| 6. Stable symbols | **Ready to expose plus new infrastructure** | Scope/resolution data exists, but identities and complete symbol taxonomy do not. |
| 7. Position queries | **Requires new infrastructure** | The raw data exists in several passes but is not cross-linked or queryable by document position. |
| 8. Completion | **Requires new infrastructure** | Scope, type, module, enum, generic, and visibility rules exist; contextual candidate production and standard-library metadata do not. |
| 9. Semantic classification | **Ready to expose plus new infrastructure** | Declaration kinds can be derived centrally, but no stable public taxonomy exists. |
| 10. Document/workspace structure | **Ready to expose plus new infrastructure** | AST and module graphs exist; hierarchy, link, call, selection, folding, and workspace indices do not. |
| 11. Refactoring | **Requires new infrastructure** | No structured edit, conflict analysis, version gate, or trivia-preserving rewrite layer exists. |
| 12. Formatting/source preservation | **Requires new infrastructure** | No formatter exists and ordinary trivia is discarded. |
| 13. Documentation metadata | **Ready to expose plus new infrastructure** | Declaration doc comments exist; parameter/return/generic/availability and built-in catalogs do not. |
| 14. Build/run/test contracts | **Ready to expose plus new infrastructure** | Build/run paths exist but are synchronous and textual. Test discovery is unavailable until Sagan defines an authoritative test model. |
| 15. Debugger prerequisites | **Dependent on runtime or code-generation work** | Generated C++ has no Sagan source maps, stable debug identities, value metadata, or evaluation hook. |
| 16. LSP server | **Requires new infrastructure after requirements 1–14** | Transport must remain a thin client of the service. |
| 17. Performance/reliability | **Requires new infrastructure throughout** | No cache, cancellation, bounded request model, or editor workload benchmark exists. |
| 18. Layered testing | **Requires new infrastructure throughout** | Existing tests are strong batch regressions but not isolated service/protocol lifecycle tests. |

No listed requirement is inherently inappropriate for Sagan. Individual
refactorings, test discovery, attach debugging, and standard-library completion
must be reported as unsupported until their authoritative compiler/runtime
preconditions exist. The server must never simulate those features with textual
name matching or an editor-only database.

## Architectural risks to control

1. **Offset drift:** UTF-8 byte offsets and LSP UTF-16 positions must be
   converted by a single tested line index, never ad hoc in protocol handlers.
2. **AST breakage:** adding recovery nodes and trivia must not change strict
   batch acceptance. Strict compilation rejects recovered trees.
3. **stale publication:** every query result must carry the analyzed document
   version; newer overlays invalidate publication.
4. **identity instability:** public symbol IDs must not be raw pointers, vector
   indices, or display names.
5. **transport leakage:** no parser, type, formatter, build, or refactoring
   behavior belongs in JSON-RPC/LSP code.
6. **stdio corruption:** diagnostics and logs from an LSP process must never be
   written to protocol stdout.
7. **false capability claims:** initialization advertises only implemented,
   tested capabilities, including standard-library and test support.

