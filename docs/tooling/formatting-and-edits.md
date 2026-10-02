---
title: Formatting and source edits
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Formatting and source edits

Phase 6's compiler-library foundation is implemented. No editor or
command-line formatter is advertised yet. The library returns previewable,
versioned edits; it does not change a buffer or file by itself. Discover the
contract through `capabilities_json` (`sourceEditsSchema` is
`sagan-source-edits-v1`) and the action list from `source_edit_capabilities()`.
Each action states whether it is available and its limitation.

`format_document`, `format_range`, and `format_on_type` return versioned edits.
They normalize structural indentation to two spaces per brace depth, trailing
horizontal whitespace outside protected content, and proven token gaps:
commas, colons, member access, calls, arithmetic and logical operators,
assignments, equality, coalescing, expression arrows, and block braces.
Existing line breaks and ambiguous spacing are left alone. A range request
formats complete intersecting lines; on-type formatting responds to a closing
brace or newline. Multiline strings and comment contents are preserved. The
formatter requires a strict parse before and after the change and verifies
that token kinds and spellings remain identical. It declines incomplete or
ambiguous input instead of guessing. Wrapping is not rewritten.

Two spaces are the approved Sagan indentation style.

`preview_edits` checks each document URI, exact version, document identity,
UTF-8/UTF-16 boundaries, and deterministic non-overlapping ranges before
returning the proposed text. It never writes the editor buffer or disk file.
When a check fails, it returns `stale`, `invalid`, or `conflict` with a reason.

Rename is proof-gated. Local edits cover bindings, private members, and
non-exported, non-overloaded functions. Workspace edits cover exported identity
groups and exact imported public-member receivers. Both paths validate naming,
collisions, snapshot versions, and rebinding before returning an atomic edit;
ambiguous identities are refused rather than risking a broken project.

`organize_imports` sorts one uninterrupted top-of-file block of plain imports.
It preserves each original line and its line endings, then rechecks the result.
It refuses comments, blank-line import groups, mixed line endings, interleaved
declarations, or incomplete source because those cases need stronger ownership
and semantic proofs. `add_missing_import` accepts an exact public symbol ID
from the supplied workspace index, checks for collisions, inserts an import
after the module header, and rechecks the resulting source and binding. It
does not guess which unresolved spelling the user intended.

`plan_diagnostic_fix` accepts only a fix already attached to a diagnostic for
the exact document version. It validates and previews the edits, then rechecks
strict syntax or full document analysis. The first compiler-issued fix inserts
a missing final `}` only when that insertion parses successfully. It is not a
general automatic repair system.

The action list explicitly disables unused-import removal,
required-face-member generation, extract-variable, and extract-function. Their
side-effect, conformance, capture, or evaluation-order proofs are not available
yet. No action is offered by textual name matching alone.

To see before/after formatting, rename, and import previews and run the focused
safety tests:

```bash
make formatter-demo
```

The complete regression suite also runs those tests through `bash scripts/test.sh`.
