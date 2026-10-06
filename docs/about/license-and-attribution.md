---
title: License and attribution
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# License and attribution

Sagan is distributed under `GPL-3.0-only`; the complete controlling text is
the root `LICENSE.txt`. This choice applies to Sagan-owned
code and preserves the GPLv3 obligations inherited from Schematic-derived
foundations.

Sagan vendors the header-only UniAlgo Unicode library for NFC normalization.
Its public-domain and MIT license text is preserved in
`third_party/uni-algo/LICENSE.md`. Generated identifier-property tables are
derived from the Unicode Character Database.
Sagan is based on Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic). Schematic is
published under the GNU General Public License version 3, and the repository
contains Schematic-derived infrastructure.

!!! warning "Distribution requirement"
    The complete `GPL-3.0-only` text, applicable attribution, and every bundled
    third-party notice must accompany redistributed Sagan source or binaries.
    This page is a project record, not legal advice.

The inherited foundations include tokenizer state, source spans, diagnostics,
generator utilities, parser-support utilities, AST foundations, and build
structure. Sagan-specific token and lexer work does not erase the provenance of
the remaining derived code.

Contributors should document the origin of imported or adapted code, retain
copyright and license notices, and avoid describing the project as wholly
original. See the [contributor licensing guidance](../contributing/licensing-and-attribution.md).
