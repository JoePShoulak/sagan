"""Register Sagan syntax highlighting for the MkDocs documentation site."""

from __future__ import annotations

from pygments.lexer import RegexLexer, bygroups, include
from pygments.token import Comment, Keyword, Name, Number, Operator, Punctuation, String, Text


class SaganLexer(RegexLexer):
    """A documentation lexer aligned with Sagan's current lexical vocabulary."""

    name = "Sagan"
    aliases = ["sagan"]
    filenames = ["*.sagan"]
    mimetypes = ["text/x-sagan"]

    _declarations = (
        r"let|const|weak|fun|test|new|class|face|enum|dimension|quantity|unit|"
        r"affine|module|export|import|from|as"
    )
    _control = r"if|else|match|case|for|in|while|until|break|continue|return|yield"
    _errors = r"hope|unless|finally|scream"

    tokens = {
        "root": [
            (r"\s+", Text.Whitespace),
            (r"///[^\r\n]*", Comment.Special),
            (r"//[^\r\n]*", Comment.Single),
            (r"/\*\*", Comment.Special, "doc-comment"),
            (r"/\*", Comment.Multiline, "block-comment"),
            (r'r"""', String.Affix, "raw-triple-string"),
            (r'r"', String.Affix, "raw-double-string"),
            (r"r'", String.Affix, "raw-single-string"),
            (r'"""', String.Double, "triple-string"),
            (r'"', String.Double, "double-string"),
            (r"'", String.Single, "single-string"),
            (r"\b(?:\d(?:_?\d)*)\.\d(?:_?\d)*(?:[eE][+-]?\d(?:_?\d)*)?\b", Number.Float),
            (r"\b\d(?:_?\d)*(?:[eE][+-]?\d(?:_?\d)*)\b", Number.Float),
            (r"\b\d(?:_?\d)*\b", Number.Integer),
            (r"\b(fun)(\s+)([A-Za-z_][\w]*!?)", bygroups(Keyword.Declaration, Text.Whitespace, Name.Function)),
            (r"\b(class|face|enum|dimension|quantity|unit)(\s+)([A-Za-z_][\w]*)",
             bygroups(Keyword.Declaration, Text.Whitespace, Name.Class)),
            (r"\b(" + _declarations + r")\b", Keyword.Declaration),
            (r"\b(" + _control + r")\b", Keyword),
            (r"\b(" + _errors + r")\b", Keyword),
            (r"\b(and|or|not|is|has)\b", Operator.Word),
            (r"\b(true|false|inf|nan)\b", Keyword.Constant),
            (r"\bself\b", Name.Builtin.Pseudo),
            (r"\b(print|assert)\b", Name.Builtin),
            (r"[A-Za-z_][\w]*!?", Name),
            (r"\.\.\.|\?\.|\?\?|:=|=>|==|!=|<=|>=|\+\+|--|[+\-*/%^]=?",
             Operator),
            (r"[=!?<>]", Operator),
            (r"[()\[\]{},:.;]", Punctuation),
            (r".", Text),
        ],
        "block-comment": [
            (r"/\*", Comment.Multiline, "#push"),
            (r"\*/", Comment.Multiline, "#pop"),
            (r"[^*/]+|[*/]", Comment.Multiline),
        ],
        "doc-comment": [
            (r"/\*", Comment.Special, "#push"),
            (r"\*/", Comment.Special, "#pop"),
            (r"[^*/]+|[*/]", Comment.Special),
        ],
        "string-content": [
            (r"\\(?:[\\\"'nrt0]|u\{[0-9A-Fa-f]{1,6}\})", String.Escape),
            (r"\$\{", String.Interpol, "interpolation"),
        ],
        "double-string": [
            include("string-content"),
            (r'"', String.Double, "#pop"),
            (r"[^\\\"$]+|[$]", String.Double),
            (r"\\.", String.Escape),
        ],
        "single-string": [
            include("string-content"),
            (r"'", String.Single, "#pop"),
            (r"[^\\'$]+|[$]", String.Single),
            (r"\\.", String.Escape),
        ],
        "triple-string": [
            include("string-content"),
            (r'"""', String.Double, "#pop"),
            (r'[^\\$\"]+|\"(?!\"\")|[$]', String.Double),
            (r"\\.", String.Escape),
        ],
        "raw-double-string": [
            (r'"', String.Double, "#pop"),
            (r'[^\"]+', String.Double),
        ],
        "raw-single-string": [
            (r"'", String.Single, "#pop"),
            (r"[^']+", String.Single),
        ],
        "raw-triple-string": [
            (r'"""', String.Double, "#pop"),
            (r'[^\"]+|\"(?!\"\")', String.Double),
        ],
        "interpolation": [
            (r"\{", Punctuation, "#push"),
            (r"\}", String.Interpol, "#pop"),
            include("root"),
        ],
    }


def on_config(config):
    """Make the local lexer discoverable as the ``sagan`` Pygments alias."""

    from pygments import lexers
    from pygments.lexers import _mapping

    _mapping.LEXERS["SaganLexer"] = (
        __name__,
        SaganLexer.name,
        tuple(SaganLexer.aliases),
        tuple(SaganLexer.filenames),
        tuple(SaganLexer.mimetypes),
    )
    lexers._lexer_cache[SaganLexer.name] = SaganLexer
    return config
