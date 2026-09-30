---
title: Formatting and source edits
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Formatting and source edits

Phase 6 has begun in the reusable C++ language-service library. No editor or
command-line formatter is advertised yet. The current library API provides a
safe, intentionally narrow layout pass and a preview-only edit contract.

`format_document`, `format_range`, and `format_on_type` return versioned edits.
They currently normalize structural indentation to two spaces per brace depth
and a small set of unambiguous token gaps: commas, colons, member access,
calls, assignments, equality, coalescing, and expression arrows. Other
expression spacing is left alone. Multiline string and comment contents are
preserved. The formatter requires a strict parse before and after the change
and verifies that the token kinds and spellings remain identical. It declines
incomplete or ambiguous input instead of guessing. This is not yet a complete
style formatter: wrapping and broader spacing choices are not rewritten.

Two spaces are the approved Sagan indentation style for the fuller formatter.

`preview_edits` checks each document URI, exact version, document identity,
UTF-8/UTF-16 boundaries, and deterministic non-overlapping ranges before
returning the proposed text. It never writes the editor buffer or disk file.
When a check fails, it returns `stale`, `invalid`, or `conflict` with a reason.

`rename_local` is the first proof-gated refactoring. It handles only a local
source binding with an identity-indexed reference set. It validates the new
identifier and naming convention, refuses existing names, reindexes the
previewed source, and verifies that every reference still resolves to the one
renamed declaration. Public, imported, member, and cross-file rename remain
unavailable.

`organize_imports` sorts one uninterrupted top-of-file block of plain imports.
It preserves each original line and its line endings, then rechecks the result.
It refuses comments, blank-line import groups, mixed line endings, interleaved
declarations, or incomplete source because those cases need stronger ownership
and semantic proofs. Removing unused imports, adding missing imports, and the
other Phase 6 refactorings remain unimplemented.

To see before/after formatting, rename, and import previews and run the focused
safety tests:

```bash
make formatter-demo
```

The complete regression suite also runs those tests through `bash scripts/test.sh`.
