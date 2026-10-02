"""Verify the local Sagan lexer and its rendered MkDocs output."""

from __future__ import annotations

from pathlib import Path
import sys

from pygments import lex
from pygments.token import Comment, Keyword, Number, String

from docs_sagan_lexer import SaganLexer


def main() -> int:
    site = Path(sys.argv[1] if len(sys.argv) > 1 else "build/docs-site")
    sample = '''/// A highlighted declaration\nfun main(): Int {\n  let value = 42\n  print("value ${value}")\n}\n'''
    token_types = {token_type for token_type, _ in lex(sample, SaganLexer())}
    expected = {
        Comment.Special,
        Keyword.Declaration,
        Number.Integer,
        String.Double,
        String.Interpol,
    }
    missing = expected - token_types
    if missing:
        raise AssertionError(f"Sagan lexer did not produce expected token types: {missing}")

    rendered = "\n".join(
        path.read_text(encoding="utf-8") for path in site.rglob("*.html")
    )
    required_fragments = (
        'class="language-sagan highlight"',
        'class="kd"',
        'class="s2"',
        'class="mi"',
    )
    absent = [fragment for fragment in required_fragments if fragment not in rendered]
    if absent:
        raise AssertionError(f"Rendered documentation lacks Sagan highlighting: {absent}")

    print("Sagan documentation syntax highlighting passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
