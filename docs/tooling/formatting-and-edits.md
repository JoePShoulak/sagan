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
They currently normalize structural indentation to two spaces per brace depth,
leaving all other source text alone. Multiline string and comment contents are
preserved. The formatter requires a strict parse before and after the change and
verifies that the token kinds and spellings remain identical. It declines
incomplete or ambiguous input instead of guessing. This is not yet a complete
style formatter: spacing within expressions, wrapping, import layout, and other
style choices are not rewritten.

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
unavailable. Other Phase 6 refactorings are not yet implemented.

To see a before/after preview and run the focused safety tests:

```bash
make formatter-demo
```

The complete regression suite also runs those tests through `bash scripts/test.sh`.
