---
title: Parser
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Parser
**Status: next; not implemented.**

The parser will consume the existing token stream and construct a Sagan syntax
tree for declarations, expressions, statements, types, functions, control flow,
classes, interfaces, and composition.

This stage must make currently provisional syntax concrete, including grammar,
operator precedence, newline handling, delimiter ambiguities, error recovery,
and syntax diagnostics. Existing parser utilities and AST foundations inherited
from Schematic are infrastructure only; they are not a Sagan parser or finished
Sagan AST.
