#include "codegen/cpp_generator.hpp"
#include "parser/lex.hpp"
#include "parser/ast_render.hpp"
#include "parser/parse_error.hpp"
#include "parser/parser.hpp"
#include "parser/tokenizer.hpp"
#include "parser/tokens.hpp"
#include "parser/unicode.hpp"
#include "semantic/analyzer.hpp"
#include "semantic/semantic_error.hpp"
#include "semantic/type_checker.hpp"
#include "version.hpp"

#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
  struct unsupported_expression final : parser::expression
  {
    using expression::expression;
    auto print(std::ostream &, int) const -> void override {}
  };

  struct unsupported_statement final : parser::statement
  {
    using statement::statement;
    auto print(std::ostream &, int) const -> void override {}
  };

  auto escape_text(const std::string &text) -> std::string
  {
    std::string result;
    for (const unsigned char c : text)
    {
      switch (c)
      {
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\0':
        result += "\\0";
        break;
      case '\\':
        result += "\\\\";
        break;
      default:
        result.push_back(static_cast<char>(c));
        break;
      }
    }
    return result;
  }

  auto tokenize(const std::string &source) -> std::vector<parser::token>
  {
    parser::tokenizer lexer(parser::programText{source}, get_token);
    std::vector<parser::token> result;
    while (auto next = lexer.next())
    {
      result.push_back(std::move(*next));
    }
    return result;
  }

  auto expect_ids(const std::string &name, const std::string &source, const std::vector<int> &expected) -> bool
  {
    const auto actual = tokenize(source);
    if (actual.size() != expected.size())
    {
      // LCOV_EXCL_START - a passing self-test cannot execute its own failure report.
      std::cerr << "[FAIL] " << name << ": expected " << expected.size() << " tokens, got " << actual.size()
                << '\n';
      return false;
      // LCOV_EXCL_STOP
    }

    for (std::size_t i = 0; i < expected.size(); i++)
    {
      if (actual[i].id != expected[i])
      {
        // LCOV_EXCL_START
        std::cerr << "[FAIL] " << name << ": token " << i << " expected " << tokens::name(expected[i])
                  << ", got " << tokens::name(actual[i].id) << '\n';
        return false;
        // LCOV_EXCL_STOP
      }
    }

    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_tokenizer_lifecycle() -> bool
  {
    parser::tokenizer lexer(parser::programText{"let x = 1\n"}, get_token);
    const bool initial = !lexer.started() && !lexer.empty() && !lexer.existing_token().has_value() &&
                         lexer.get_span().begin == 0 && lexer.get_span().end == 0;
    const auto first = lexer.get_token();
    const auto cached = lexer.get_token();
    const bool pulled = lexer.started() && !lexer.empty() && first.has_value() && cached.has_value() &&
                        first->id == tokens::KWD_LET && cached->id == first->id &&
                        lexer.existing_token()->id == first->id && lexer.get_span().begin == 0 &&
                        lexer.get_span().end == 3;
    const auto second = lexer.next();
    const bool advanced = second.has_value() && second->id == tokens::IDENTIFIER &&
                          lexer.get_span().begin == 3 && lexer.get_span().end == 5;
    while (lexer.next())
    {
    }
    const bool exhausted = lexer.empty() && !lexer.existing_token().has_value() && !lexer.next().has_value();
    const bool token_names = tokens::name(-1) == "INVALID_TOKEN" &&
                             tokens::name(tokens::TOKEN_COUNT) == "INVALID_TOKEN";
    if (!(initial && pulled && advanced && exhausted && token_names))
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] tokenizer lifecycle and token-name boundaries\n";
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] tokenizer lifecycle and token-name boundaries\n";
    return true;
  }

  auto expect_unicode_primitives() -> bool
  {
    const std::string invalid_byte(1, static_cast<char>(0xff));
    const std::string truncated_two_byte(1, static_cast<char>(0xc2));
    const std::string invalid_continuation{static_cast<char>(0xc2), 'A'};
    const std::string tagged_flag =
        "\xf0\x9f\x8f\xb4\xf3\xa0\x81\xa7\xf3\xa0\x81\xa2\xf3\xa0\x81\xbf";
    const bool passed = !sagan::unicode::decode("", 0).has_value() &&
                        !sagan::unicode::decode(invalid_byte, 0).has_value() &&
                        !sagan::unicode::decode(truncated_two_byte, 0).has_value() &&
                        !sagan::unicode::decode(invalid_continuation, 0).has_value() &&
                        sagan::unicode::emoji_sequence_length(invalid_byte, 0) == 0 &&
                        sagan::unicode::emoji_sequence_length("✈️", 0) == std::string("✈️").size() &&
                        sagan::unicode::emoji_sequence_length("☝️🏻", 0) == std::string("☝️🏻").size() &&
                        sagan::unicode::emoji_sequence_length(tagged_flag, 0) == tagged_flag.size();
    if (!passed)
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] Unicode primitive edge cases\n";
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] Unicode primitive edge cases\n";
    return true;
  }

  auto expect_error(const std::string &name, const std::string &source) -> bool
  {
    try
    {
      static_cast<void>(tokenize(source));
    }
    catch (const parser::parse_error &)
    {
      std::cout << "[PASS] " << name << '\n';
      return true;
    }

    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected a lexical error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto expect_token_text(const std::string &name, const std::string &source, const std::size_t index,
                         const std::string &expected) -> bool
  {
    const auto actual = tokenize(source);
    if (index >= actual.size() || actual[index].text != expected)
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": expected token text '" << expected << "'\n";
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_robustness(const std::string &name) -> bool
  {
    unsigned int state = 0x5a17c9e3;
    for (int sample = 0; sample < 2000; sample++)
    {
      std::string source;
      const int length = sample % 65;
      source.reserve(static_cast<std::size_t>(length));
      for (int i = 0; i < length; i++)
      {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        source.push_back(static_cast<char>(state & 0xff));
      }
      try
      {
        static_cast<void>(tokenize(source));
      }
      catch (const parser::parse_error &)
      {
      }
      // LCOV_EXCL_START - this branch indicates a bug in the tokenizer rather than an expected outcome.
      catch (const std::exception &error)
      {
        std::cerr << "[FAIL] " << name << ": unexpected exception: " << error.what() << '\n';
        return false;
      }
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_ast(const std::string &name, const std::string &source, const std::string &expected) -> bool
  {
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    std::ostringstream output;
    tree.print(output);
    if (output.str() != expected)
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ":\n" << output.str();
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_syntax_error(const std::string &name, const std::string &source) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      static_cast<void>(syntax.parse());
    }
    catch (const parser::parse_error &)
    {
      std::cout << "[PASS] " << name << '\n';
      return true;
    }
    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected a syntax error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto expect_syntax_error_contains(const std::string &name, const std::string &source,
                                    const std::string &expected_message) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      static_cast<void>(syntax.parse());
    }
    catch (const parser::parse_error &error)
    {
      if (std::string(error.what()).contains(expected_message))
      {
        std::cout << "[PASS] " << name << '\n';
        return true;
      }
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": expected diagnostic containing '" << expected_message
                << "', but found '" << error.what() << "'\n";
      return false;
      // LCOV_EXCL_STOP
    }
    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected a syntax error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto expect_visual_ast(const std::string &name, const std::string &source) -> bool
  {
    std::string escaped_source;
    for (const char character : source)
    {
      switch (character)
      {
      case '&': escaped_source += "&amp;"; break;
      case '<': escaped_source += "&lt;"; break;
      case '>': escaped_source += "&gt;"; break;
      case '"': escaped_source += "&quot;"; break;
      case '\'': escaped_source += "&#39;"; break;
      default: escaped_source.push_back(character); break;
      }
    }
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    const std::string dot = parser::render_ast_dot(tree);
    const std::string svg = parser::render_ast_svg(tree);
    const std::string html = parser::render_ast_html(source, tree, "AST test");
    const bool passed = dot.contains("digraph SaganAST") && dot.contains("Identifier") &&
                        svg.contains("<svg") && svg.contains("Identifier") &&
                        html.contains("Input source") && html.contains(escaped_source) &&
                        html.contains("data-action=\"fit\"") && html.contains("Wheel to zoom");
    if (!passed)
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": visual output was incomplete\n";
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_renderer_rejects_unknown_nodes() -> bool
  {
    std::ostringstream ignored;
    unsupported_expression(parser::span{0, 1}).print(ignored, 0);
    unsupported_statement(parser::span{0, 1}).print(ignored, 0);
    bool expression_rejected = false;
    bool statement_rejected = false;
    try
    {
      std::vector<parser::statement_ref> statements;
      statements.push_back(std::make_unique<parser::let_declaration>(
          parser::span{0, 1}, "value", std::nullopt,
          std::make_unique<unsupported_expression>(parser::span{0, 1})));
      static_cast<void>(parser::render_ast_dot(parser::program(std::move(statements))));
    }
    catch (const std::runtime_error &)
    {
      expression_rejected = true;
    }
    try
    {
      std::vector<parser::statement_ref> statements;
      statements.push_back(std::make_unique<unsupported_statement>(parser::span{0, 1}));
      static_cast<void>(parser::render_ast_dot(parser::program(std::move(statements))));
    }
    catch (const std::runtime_error &)
    {
      statement_rejected = true;
    }
    if (!(expression_rejected && statement_rejected))
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] renderer rejects unknown AST nodes\n";
      return false;
      // LCOV_EXCL_STOP
    }
    std::cout << "[PASS] renderer rejects unknown AST nodes\n";
    return true;
  }

  auto expect_semantic_model(const std::string &name, const std::string &source,
                             const std::vector<std::string> &expected) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      const auto tree = syntax.parse();
      const auto model = semantic::analyze(tree);
      std::ostringstream output;
      model.print(output);
      for (const auto &fragment : expected)
      {
        if (!output.str().contains(fragment))
        {
          // LCOV_EXCL_START
          std::cerr << "[FAIL] " << name << ": missing semantic model fragment '" << fragment << "'\n";
          return false;
          // LCOV_EXCL_STOP
        }
      }
      std::cout << "[PASS] " << name << '\n';
      return true;
    }
    catch (const parser::parse_error &error)
    {
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": parse error at byte " << error.range.begin << ": "
                << error.what() << '\n';
      return false;
      // LCOV_EXCL_STOP
    }
  }

  auto expect_semantic_error(const std::string &name, const std::string &source,
                             const std::string &expected) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      const auto tree = syntax.parse();
      static_cast<void>(semantic::analyze(tree));
    }
    catch (const semantic::semantic_error &error)
    {
      if (std::string(error.what()).contains(expected))
      {
        std::cout << "[PASS] " << name << '\n';
        return true;
      }
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": unexpected semantic error '" << error.what() << "'\n";
      return false;
      // LCOV_EXCL_STOP
    }
    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected a semantic error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto expect_type_model(const std::string &name, const std::string &source,
                         const std::vector<std::string> &expected) -> bool
  {
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    static_cast<void>(semantic::analyze(tree));
    const auto model = semantic::check_types(tree);
    std::ostringstream output;
    model.print(output);
    for (const auto &fragment : expected)
    {
      if (!output.str().contains(fragment))
      {
        // LCOV_EXCL_START
        std::cerr << "[FAIL] " << name << ": missing type model fragment '" << fragment << "'\n";
        return false;
        // LCOV_EXCL_STOP
      }
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_type_error(const std::string &name, const std::string &source,
                         const std::string &expected) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      const auto tree = syntax.parse();
      static_cast<void>(semantic::analyze(tree));
      static_cast<void>(semantic::check_types(tree));
    }
    catch (const semantic::semantic_error &error)
    {
      if (std::string(error.what()).contains(expected))
      {
        std::cout << "[PASS] " << name << '\n';
        return true;
      }
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": unexpected type error '" << error.what() << "'\n";
      return false;
      // LCOV_EXCL_STOP
    }
    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected a type error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto expect_entry_point(const std::string &name, const std::string &source) -> bool
  {
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    static_cast<void>(semantic::analyze(tree));
    static_cast<void>(semantic::check_types(tree));
    semantic::validate_entry_point(tree);
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto expect_entry_error(const std::string &name, const std::string &source,
                          const std::string &expected) -> bool
  {
    try
    {
      parser::syntax_parser syntax(tokenize(source));
      const auto tree = syntax.parse();
      static_cast<void>(semantic::analyze(tree));
      static_cast<void>(semantic::check_types(tree));
      semantic::validate_entry_point(tree);
    }
    catch (const semantic::semantic_error &error)
    {
      if (std::string(error.what()).contains(expected))
      {
        std::cout << "[PASS] " << name << '\n';
        return true;
      }
      // LCOV_EXCL_START
      std::cerr << "[FAIL] " << name << ": unexpected entry-point error '" << error.what() << "'\n";
      return false;
      // LCOV_EXCL_STOP
    }
    // LCOV_EXCL_START
    std::cerr << "[FAIL] " << name << ": expected an entry-point error\n";
    return false;
    // LCOV_EXCL_STOP
  }

  auto run_self_tests() -> int
  {
    bool passed = true;

    passed &= expect_tokenizer_lifecycle();
    passed &= expect_unicode_primitives();

    passed &= expect_ids(
        "declaration",
        "let altitude: Float = 125_000.0\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::COLON, tokens::IDENTIFIER, tokens::EQUAL,
         tokens::FLOAT, tokens::NEWLINE});

    passed &= expect_ids(
        "emoji identifiers",
        "let 🌌 = 42\nfun 👩🏽‍🚀(🇺🇸)\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::INTEGER, tokens::NEWLINE,
         tokens::KWD_FUN, tokens::IDENTIFIER, tokens::LPAREN, tokens::IDENTIFIER, tokens::RPAREN,
         tokens::NEWLINE});

    passed &= expect_ids(
        "Unicode XID identifiers",
        "let Δx = 1\nlet 変数 = Δx\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::INTEGER, tokens::NEWLINE,
         tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::IDENTIFIER, tokens::NEWLINE});

    passed &= expect_token_text("NFC identifier normalization", "let cafe\u0301 = 1\n", 1, "café");

    passed &= expect_ids(
        "operators",
        "value++ >= other and !failed\n",
        {tokens::IDENTIFIER, tokens::PLUS_PLUS, tokens::GREATER_EQUAL, tokens::IDENTIFIER, tokens::KWD_AND,
         tokens::BANG, tokens::IDENTIFIER, tokens::NEWLINE});

    passed &= expect_ids(
        "complete keyword vocabulary",
        "let fun class face enum if else match case for in while until break continue return yield "
        "import from as module export hope unless finally scream and or not self is has true false inf nan\n",
        {tokens::KWD_LET, tokens::KWD_FUN, tokens::KWD_CLASS, tokens::KWD_FACE, tokens::KWD_ENUM,
         tokens::KWD_IF, tokens::KWD_ELSE, tokens::KWD_MATCH, tokens::KWD_CASE, tokens::KWD_FOR,
         tokens::KWD_IN, tokens::KWD_WHILE, tokens::KWD_UNTIL, tokens::KWD_BREAK, tokens::KWD_CONTINUE,
         tokens::KWD_RETURN, tokens::KWD_YIELD, tokens::KWD_IMPORT, tokens::KWD_FROM, tokens::KWD_AS,
         tokens::KWD_MODULE, tokens::KWD_EXPORT, tokens::KWD_HOPE, tokens::KWD_UNLESS,
         tokens::KWD_FINALLY, tokens::KWD_SCREAM, tokens::KWD_AND, tokens::KWD_OR, tokens::KWD_NOT,
         tokens::KWD_SELF, tokens::KWD_IS, tokens::KWD_HAS, tokens::KWD_TRUE, tokens::KWD_FALSE,
         tokens::KWD_INF, tokens::KWD_NAN, tokens::NEWLINE});

    passed &= expect_ids(
        "complete punctuation and operator vocabulary",
        "( ) [ ] { } < > , : . ? ; + - * / % ^ = ! ... ?. := => == != <= >= ++ -- += -= *= /= %= ^= value\n",
        {tokens::LPAREN, tokens::RPAREN, tokens::LBRACKET, tokens::RBRACKET, tokens::LBRACE,
         tokens::RBRACE, tokens::LANGLE, tokens::RANGLE, tokens::COMMA, tokens::COLON, tokens::DOT,
         tokens::QUESTION, tokens::SEMICOLON, tokens::PLUS, tokens::MINUS, tokens::STAR, tokens::SLASH,
         tokens::PERCENT, tokens::CARET, tokens::EQUAL, tokens::BANG, tokens::SPREAD, tokens::SAFE_DOT,
         tokens::ASSIGN_VALUE, tokens::FAT_ARROW, tokens::EQUAL_EQUAL, tokens::BANG_EQUAL,
         tokens::LESS_EQUAL, tokens::GREATER_EQUAL, tokens::PLUS_PLUS, tokens::MINUS_MINUS,
         tokens::PLUS_EQUAL, tokens::MINUS_EQUAL, tokens::STAR_EQUAL, tokens::SLASH_EQUAL,
         tokens::PERCENT_EQUAL, tokens::CARET_EQUAL, tokens::IDENTIFIER, tokens::NEWLINE});

    passed &= expect_ids(
        "continued expression",
        "let total =\n  first +\n  second\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::IDENTIFIER, tokens::PLUS,
         tokens::IDENTIFIER, tokens::NEWLINE});

    passed &= expect_ids(
        "parenthesized newlines",
        "let total = (\n  first +\n  second\n)\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::LPAREN, tokens::IDENTIFIER,
         tokens::PLUS, tokens::IDENTIFIER, tokens::RPAREN, tokens::NEWLINE});

    passed &= expect_ids(
        "method convention",
        "vector.normalize!()\n",
        {tokens::IDENTIFIER, tokens::DOT, tokens::METHOD_IDENTIFIER, tokens::LPAREN, tokens::RPAREN,
         tokens::NEWLINE});

    passed &= expect_ids(
        "interpolation",
        "\"speed: ${distance / time}\"\n",
        {tokens::STRING_BEGIN, tokens::STRING_SEGMENT, tokens::INTERPOLATION_BEGIN, tokens::IDENTIFIER,
         tokens::SLASH, tokens::IDENTIFIER, tokens::INTERPOLATION_END, tokens::STRING_END, tokens::NEWLINE});

    passed &= expect_ids(
        "raw string",
        "r\"C:\\simulation\\data\"\n",
        {tokens::STRING, tokens::NEWLINE});

    passed &= expect_ids(
        "single quoted and multiline strings",
        "'signal'\n\"\"\"first\nsecond\"\"\"\n",
        {tokens::STRING_BEGIN, tokens::STRING_SEGMENT, tokens::STRING_END, tokens::NEWLINE,
         tokens::STRING_BEGIN, tokens::STRING_SEGMENT, tokens::STRING_END, tokens::NEWLINE});

    passed &= expect_ids(
        "complete string escape vocabulary",
        "\"\\\\\\\"\\'\\n\\r\\t\\0\\u{41}\\u{3a9}\\u{20ac}\\u{1f680}\"\n",
        {tokens::STRING_BEGIN, tokens::STRING_SEGMENT, tokens::STRING_END, tokens::NEWLINE});

    passed &= expect_ids(
        "nested comments",
        "/* outer /* inner */ outer */ let value = 1\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::INTEGER, tokens::NEWLINE});

    passed &= expect_ids(
        "documentation",
        "/// Calculates velocity.\nfun velocity()\n",
        {tokens::DOC_COMMENT, tokens::NEWLINE, tokens::KWD_FUN, tokens::IDENTIFIER, tokens::LPAREN,
         tokens::RPAREN, tokens::NEWLINE});

    passed &= expect_error("malformed exponent", "let value = 1e\n");
    passed &= expect_error("leading decimal point", "let value = .5\n");
    passed &= expect_error("trailing decimal point", "let value = 5.\n");
    passed &= expect_error("misplaced digit separator", "let value = 1__000\n");
    passed &= expect_error("unknown character", "let value = @tag\n");
    passed &= expect_error("unterminated string", "let value = \"missing\n");
    passed &= expect_error("unterminated block comment", "/* missing\n");
    passed &= expect_error("lone carriage return", "let value = 1\rlet other = 2\n");
    passed &= expect_error("malformed UTF-8", std::string("let value = ") + '\xc0' + '\xaf' + "\n");
    passed &= expect_error("invalid identifier start", "let \u0301accent = 1\n");
    passed &= expect_error("incomplete emoji sequence", "let 🚀‍name = 1\n");
    passed &= expect_error("surrogate Unicode escape", "let value = \"\\u{d800}\"\n");
    passed &= expect_error("oversized Unicode escape", "let value = \"\\u{110000}\"\n");
    passed &= expect_error("escape at end of input", "\"unfinished\\");
    passed &= expect_error("unknown string escape", "\"\\q\"\n");
    passed &= expect_error("Unicode escape missing brace", "\"\\u1234\"\n");
    passed &= expect_error("Unicode escape non-hex digit", "\"\\u{12z4}\"\n");
    passed &= expect_error("empty Unicode escape", "\"\\u{}\"\n");
    passed &= expect_error("unterminated Unicode escape", "\"\\u{1234\"\n");
    passed &= expect_error("unterminated raw string at newline", "r\"unfinished\n");
    passed &= expect_error("unterminated raw string at end", "r\"unfinished");
    passed &= expect_error("seven-digit Unicode escape", "\"\\u{1234567}\"\n");
    passed &= expect_error("numeric identifier suffix", "let invalid = 12parsecs\n");
    passed &= expect_error("unterminated interpolation", "\"${value");
    passed &= expect_ids(
        "interpolation begins immediately and nests braces",
        "\"${{1: 2}}\"\n",
        {tokens::STRING_BEGIN, tokens::INTERPOLATION_BEGIN, tokens::LBRACE, tokens::INTEGER,
         tokens::COLON, tokens::INTEGER, tokens::RBRACE, tokens::INTERPOLATION_END,
         tokens::STRING_END, tokens::NEWLINE});
    passed &= expect_ids(
        "CRLF logical newline",
        "let first = 1\r\nlet second = 2\r\n",
        {tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::INTEGER, tokens::NEWLINE,
         tokens::KWD_LET, tokens::IDENTIFIER, tokens::EQUAL, tokens::INTEGER, tokens::NEWLINE});
    passed &= expect_robustness("random byte robustness");
    passed &= expect_ast(
        "parser foundation",
        "let altitude: Float = 125_000.0\nlet target = (altitude)\nlet pending\n",
        "Program\n"
        "  Let(altitude: Float)\n"
        "    Float(125_000.0)\n"
        "  Let(target)\n"
        "    Group\n"
        "      Identifier(altitude)\n"
        "  Let(pending)\n");
    passed &= expect_ast(
        "expression precedence",
        "let result = -2^3^2 + 4 * 5\n",
        "Program\n"
        "  Let(result)\n"
        "    Binary(+)\n"
        "      Prefix(-)\n"
        "        Binary(^)\n"
        "          Integer(2)\n"
        "          Binary(^)\n"
        "            Integer(3)\n"
        "            Integer(2)\n"
        "      Binary(*)\n"
        "        Integer(4)\n"
        "        Integer(5)\n");
    passed &= expect_ast(
        "conditional and assignment associativity",
        "let selected = ready and count >= 1 ? count ; fallback\n"
        "let assigned = target := source := 1\n",
        "Program\n"
        "  Let(selected)\n"
        "    Conditional\n"
        "      Binary(and)\n"
        "        Identifier(ready)\n"
        "        Binary(>=)\n"
        "          Identifier(count)\n"
        "          Integer(1)\n"
        "      Identifier(count)\n"
        "      Identifier(fallback)\n"
        "  Let(assigned)\n"
        "    AssignValue(:=)\n"
        "      Identifier(target)\n"
        "      AssignValue(:=)\n"
        "        Identifier(source)\n"
        "        Integer(1)\n");
    passed &= expect_syntax_error("chained comparison", "let invalid = a < b < c\n");
    passed &= expect_ast(
        "or equality and special floating values",
        "let compared = (left == right) or (value != nan) or inf\n",
        "Program\n"
        "  Let(compared)\n"
        "    Binary(or)\n"
        "      Binary(or)\n"
        "        Group\n"
        "          Binary(==)\n"
        "            Identifier(left)\n"
        "            Identifier(right)\n"
        "        Group\n"
        "          Binary(!=)\n"
        "            Identifier(value)\n"
        "            Float(nan)\n"
        "      Float(inf)\n");
    passed &= expect_ast(
        "postfix calls indexing and member access",
        "let course = fleet[active_index]?.navigator.current_course(origin, destination).magnitude()\n",
        "Program\n"
        "  Let(course)\n"
        "    Call\n"
        "      Member(magnitude)\n"
        "        Call\n"
        "          Member(current_course)\n"
        "            SafeMember(navigator)\n"
        "              Index\n"
        "                Identifier(fleet)\n"
        "                Identifier(active_index)\n"
        "          Identifier(origin)\n"
        "          Identifier(destination)\n");
    passed &= expect_ast(
        "mutating method call",
        "let normalized = vectors[0].normalize!()\n",
        "Program\n"
        "  Let(normalized)\n"
        "    Call\n"
        "      Member(normalize!)\n"
        "        Index\n"
        "          Identifier(vectors)\n"
        "          Integer(0)\n");
    passed &= expect_syntax_error("missing member name", "let invalid = spacecraft.\n");
    passed &= expect_ast(
        "string interpolation expression",
        "let message = \"speed: ${distance / time} km/s\"\n",
        "Program\n"
        "  Let(message)\n"
        "    String\n"
        "      Text(\"speed: \")\n"
        "      Interpolation\n"
        "        Binary(/)\n"
        "          Identifier(distance)\n"
        "          Identifier(time)\n"
        "      Text(\" km/s\")\n");
    passed &= expect_ast(
        "raw and multiline strings",
        "let path = r\"C:\\simulation\\${literal}\"\nlet report = \"\"\"line one\nline two\"\"\"\n",
        "Program\n"
        "  Let(path)\n"
        "    RawString\n"
        "      Text(\"C:\\\\simulation\\\\${literal}\")\n"
        "  Let(report)\n"
        "    MultilineString\n"
        "      Text(\"line one\\nline two\")\n");
    passed &= expect_ast(
        "string postfix chain",
        "let size = \"telemetry\".trim().length()\n",
        "Program\n"
        "  Let(size)\n"
        "    Call\n"
        "      Member(length)\n"
        "        Call\n"
        "          Member(trim)\n"
        "            String\n"
        "              Text(\"telemetry\")\n");
    passed &= expect_syntax_error("empty string interpolation", "let invalid = \"value: ${}\"\n");
    passed &= expect_ast(
        "escaped AST string text",
        "let escaped = \"\\r\\t\\0\\\\\\\"\"\n",
        "Program\n"
        "  Let(escaped)\n"
        "    String\n"
        "      Text(\"\\r\\t\\0\\\\\\\"\")\n");
    passed &= expect_ast(
        "arrays trailing commas and spread",
        "let values = [1, ...defaults, calculate(2, 3,),]\n",
        "Program\n"
        "  Let(values)\n"
        "    Array\n"
        "      Integer(1)\n"
        "      Spread\n"
        "        Identifier(defaults)\n"
        "      Call\n"
        "        Identifier(calculate)\n"
        "        Integer(2)\n"
        "        Integer(3)\n");
    passed &= expect_ast(
        "empty array and dictionary",
        "let values = []\nlet metadata = {}\n",
        "Program\n"
        "  Let(values)\n"
        "    Array\n"
        "  Let(metadata)\n"
        "    Dictionary\n");
    passed &= expect_ast(
        "dictionary expression keys and spread",
        "let metadata = {\"name\": \"Voyager\", active_key: true, ...defaults,}\n",
        "Program\n"
        "  Let(metadata)\n"
        "    Dictionary\n"
        "      Entry\n"
        "        String\n"
        "          Text(\"name\")\n"
        "        String\n"
        "          Text(\"Voyager\")\n"
        "      Entry\n"
        "        Identifier(active_key)\n"
        "        Bool(true)\n"
        "      Spread\n"
        "        Identifier(defaults)\n");
    passed &= expect_ast(
        "vectors coordinates and delimiter ambiguity",
        "let direction = <1.0, 0.0, 0.0,>\n"
        "let position = (x, y, z,)\n"
        "let relation = left < right\n"
        "let flags = <(left > right), true>\n",
        "Program\n"
        "  Let(direction)\n"
        "    Vector\n"
        "      Float(1.0)\n"
        "      Float(0.0)\n"
        "      Float(0.0)\n"
        "  Let(position)\n"
        "    Coordinate\n"
        "      Identifier(x)\n"
        "      Identifier(y)\n"
        "      Identifier(z)\n"
        "  Let(relation)\n"
        "    Binary(<)\n"
        "      Identifier(left)\n"
        "      Identifier(right)\n"
        "  Let(flags)\n"
        "    Vector\n"
        "      Group\n"
        "        Binary(>)\n"
        "          Identifier(left)\n"
        "          Identifier(right)\n"
        "      Bool(true)\n");
    passed &= expect_syntax_error("empty vector", "let invalid = <>\n");
    passed &= expect_syntax_error("single-element vector", "let invalid = <1>\n");
    passed &= expect_syntax_error("single-element coordinate", "let invalid = (1,)\n");
    passed &= expect_syntax_error("empty parentheses", "let invalid = ()\n");
    passed &= expect_syntax_error("array elements require commas", "let invalid = [1 2]\n");
    passed &= expect_syntax_error("dictionary entries require commas", "let invalid = {1: 2 3: 4}\n");
    passed &= expect_syntax_error("coordinate elements require commas", "let invalid = (1, 2 3)\n");
    passed &= expect_syntax_error("dictionary entry without colon", "let invalid = {\"name\",}\n");
    passed &= expect_ast(
        "blocks assignments and conditional statements",
        "fun update(telemetry_ready: Bool): Bool {\n"
        "if telemetry_ready {\n"
        "  let course = navigator.course(origin)\n"
        "  ship.course = course\n"
        "  ship.commit!()\n"
        "} else if retrying {\n"
        "  retry()\n"
        "} else {\n"
        "}\n"
        "}\n",
        "Program\n"
        "  Function(update: Bool)\n"
        "    Parameter(telemetry_ready: Bool)\n"
        "    Block\n"
        "      If\n"
        "        Condition\n"
        "          Identifier(telemetry_ready)\n"
        "        Then\n"
        "          Block\n"
        "            Let(course)\n"
        "              Call\n"
        "                Member(course)\n"
        "                  Identifier(navigator)\n"
        "                Identifier(origin)\n"
        "            Assignment(=)\n"
        "              Member(course)\n"
        "                Identifier(ship)\n"
        "              Identifier(course)\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Member(commit!)\n"
        "                  Identifier(ship)\n"
        "        Else\n"
        "          If\n"
        "            Condition\n"
        "              Identifier(retrying)\n"
        "            Then\n"
        "              Block\n"
        "                ExpressionStatement\n"
        "                  Call\n"
        "                    Identifier(retry)\n"
        "            Else\n"
        "              Block\n");
    passed &= expect_ast(
        "compound assignment statements",
        "fun accumulate(delta) {\n"
        "  total += delta\n"
        "  remaining -= 1\n"
        "  scale *= factor\n"
        "  distance /= elapsed\n"
        "  index %= capacity\n"
        "  energy ^= exponent\n"
        "}\n",
        "Program\n"
        "  Function(accumulate)\n"
        "    Parameter(delta)\n"
        "    Block\n"
        "      Assignment(+=)\n"
        "        Identifier(total)\n"
        "        Identifier(delta)\n"
        "      Assignment(-=)\n"
        "        Identifier(remaining)\n"
        "        Integer(1)\n"
        "      Assignment(*=)\n"
        "        Identifier(scale)\n"
        "        Identifier(factor)\n"
        "      Assignment(/=)\n"
        "        Identifier(distance)\n"
        "        Identifier(elapsed)\n"
        "      Assignment(%=)\n"
        "        Identifier(index)\n"
        "        Identifier(capacity)\n"
        "      Assignment(^=)\n"
        "        Identifier(energy)\n"
        "        Identifier(exponent)\n");
    passed &= expect_syntax_error_contains("compound assignment without value",
                                           "fun invalid() {\n  total +=\n}\n",
                                           "Expected an expression after '+='");
    passed &= expect_syntax_error_contains("chained compound assignment",
                                           "fun invalid() {\n  first += second += third\n}\n",
                                           "Assignment statements cannot be chained");
    passed &= expect_syntax_error("top-level expression statement", "launch()\n");
    passed &= expect_syntax_error("top-level control flow", "if ready {\n}\n");
    passed &= expect_syntax_error_contains("standalone else", "fun invalid() {\n  else {\n  }\n}\n",
                                           "'else' is only valid after an if statement");
    passed &= expect_syntax_error("unterminated statement block", "fun launch() {\n  launch_engine()\n");
    passed &= expect_ast(
        "loops control transfer and returns",
        "fun navigate(items): Result {\n"
        "  for item in items {\n"
        "    if item.ready {\n"
        "      continue\n"
        "    }\n"
        "    while item.pending {\n"
        "      item.poll()\n"
        "      break\n"
        "    }\n"
        "  }\n"
        "  until finished {\n"
        "    finished = check()\n"
        "  }\n"
        "  if failed {\n"
        "    return\n"
        "  }\n"
        "  return result\n"
        "}\n",
        "Program\n"
        "  Function(navigate: Result)\n"
        "    Parameter(items)\n"
        "    Block\n"
        "      For(item)\n"
        "        Iterable\n"
        "          Identifier(items)\n"
        "        Block\n"
        "          If\n"
        "            Condition\n"
        "              Member(ready)\n"
        "                Identifier(item)\n"
        "            Then\n"
        "              Block\n"
        "                Continue\n"
        "          While\n"
        "            Condition\n"
        "              Member(pending)\n"
        "                Identifier(item)\n"
        "            Block\n"
        "              ExpressionStatement\n"
        "                Call\n"
        "                  Member(poll)\n"
        "                    Identifier(item)\n"
        "              Break\n"
        "      Until\n"
        "        Condition\n"
        "          Identifier(finished)\n"
        "        Block\n"
        "          Assignment(=)\n"
        "            Identifier(finished)\n"
        "            Call\n"
        "              Identifier(check)\n"
        "      If\n"
        "        Condition\n"
        "          Identifier(failed)\n"
        "        Then\n"
        "          Block\n"
        "            Return\n"
        "      Return\n"
        "        Identifier(result)\n");
    passed &= expect_syntax_error("break outside loop", "fun invalid() {\n  break\n}\n");
    passed &= expect_syntax_error("continue outside loop", "fun invalid() {\n  continue\n}\n");
    passed &= expect_syntax_error("missing for in", "fun invalid(items) {\n  for item items {\n  }\n}\n");
    passed &= expect_syntax_error("labeled break is unsupported",
                                  "fun invalid(items) {\n  for item in items {\n    break outer\n  }\n}\n");
    passed &= expect_ast(
        "yield statements",
        "fun generate(items) {\n"
        "  for item in items {\n"
        "    yield item\n"
        "  }\n"
        "  yield\n"
        "}\n",
        "Program\n"
        "  Function(generate)\n"
        "    Parameter(items)\n"
        "    Block\n"
        "      For(item)\n"
        "        Iterable\n"
        "          Identifier(items)\n"
        "        Block\n"
        "          Yield\n"
        "            Identifier(item)\n"
        "      Yield\n");
    passed &= expect_ast(
        "match cases and fallback",
        "fun describe(status): String {\n"
        "  match status {\n"
        "    case 0 {\n"
        "      return \"idle\"\n"
        "    }\n"
        "    case ready {\n"
        "      return describe_ready(status)\n"
        "    }\n"
        "    case else {\n"
        "      return\n"
        "    }\n"
        "  }\n"
        "}\n",
        "Program\n"
        "  Function(describe: String)\n"
        "    Parameter(status)\n"
        "    Block\n"
        "      Match\n"
        "        Subject\n"
        "          Identifier(status)\n"
        "        Case\n"
        "          Integer(0)\n"
        "          Block\n"
        "            Return\n"
        "              String\n"
        "                Text(\"idle\")\n"
        "        Case\n"
        "          Identifier(ready)\n"
        "          Block\n"
        "            Return\n"
        "              Call\n"
        "                Identifier(describe_ready)\n"
        "                Identifier(status)\n"
        "        CaseElse\n"
        "          Block\n"
        "            Return\n");
    passed &= expect_syntax_error("empty match", "fun invalid(value) {\n  match value {\n  }\n}\n");
    passed &= expect_syntax_error("unterminated match body",
                                  "fun invalid(value) {\n  match value {\n    case 1 {\n    }\n");
    passed &= expect_syntax_error(
        "duplicate match fallback",
        "fun invalid(value) {\n  match value {\n    case else {\n    }\n    case else {\n    }\n  }\n}\n");
    passed &= expect_syntax_error(
        "match fallback must be last",
        "fun invalid(value) {\n  match value {\n    case else {\n    }\n    case 1 {\n    }\n  }\n}\n");
    passed &= expect_syntax_error("case outside match", "fun invalid() {\n  case 1 {\n  }\n}\n");
    passed &= expect_ast(
        "exception handling and raising",
        "fun execute() {\n"
        "  hope {\n"
        "    launch()\n"
        "  } unless NetworkError {\n"
        "    recover()\n"
        "    scream NetworkError\n"
        "  } unless error {\n"
        "    scream wrap(error)\n"
        "  } finally {\n"
        "    cleanup()\n"
        "  }\n"
        "}\n",
        "Program\n"
        "  Function(execute)\n"
        "    Block\n"
        "      Hope\n"
        "        Protected\n"
        "          Block\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Identifier(launch)\n"
        "        Unless\n"
        "          Identifier(NetworkError)\n"
        "          Block\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Identifier(recover)\n"
        "            Scream\n"
        "              Identifier(NetworkError)\n"
        "        Unless\n"
        "          Identifier(error)\n"
        "          Block\n"
        "            Scream\n"
        "              Call\n"
        "                Identifier(wrap)\n"
        "                Identifier(error)\n"
        "        Finally\n"
        "          Block\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Identifier(cleanup)\n");
    passed &= expect_ast(
        "finally-only hope",
        "fun cleanup_only() {\n  hope {\n    work()\n  } finally {\n    cleanup()\n  }\n}\n",
        "Program\n"
        "  Function(cleanup_only)\n"
        "    Block\n"
        "      Hope\n"
        "        Protected\n"
        "          Block\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Identifier(work)\n"
        "        Finally\n"
        "          Block\n"
        "            ExpressionStatement\n"
        "              Call\n"
        "                Identifier(cleanup)\n");
    passed &= expect_syntax_error("hope without clauses", "fun invalid() {\n  hope {\n    work()\n  }\n}\n");
    passed &= expect_syntax_error("scream without value", "fun invalid() {\n  scream\n}\n");
    passed &= expect_syntax_error("standalone unless", "fun invalid() {\n  unless error {\n  }\n}\n");
    passed &= expect_syntax_error("standalone finally", "fun invalid() {\n  finally {\n  }\n}\n");
    passed &= expect_ast(
        "faces classes enums and composition",
        "face Renderable {\n"
        "  fun render(): Frame\n"
        "}\n"
        "face Spacecraft is Renderable, Movable {\n"
        "  fun trajectory(): Vector\n"
        "  fun label(): String {\n"
        "    return \"spacecraft\"\n"
        "  }\n"
        "}\n"
        "class ExplorerShip has Spacecraft, Trackable {\n"
        "  let name: String = \"Explorer\"\n"
        "  fun render(): Frame {\n"
        "    return self.draw()\n"
        "  }\n"
        "  fun .calculate_internal_state(): Vector {\n"
        "    return self.position\n"
        "  }\n"
        "}\n"
        "enum MissionState {\n"
        "  planned\n"
        "  running,\n"
        "  complete\n"
        "}\n",
        "Program\n"
        "  Face(Renderable)\n"
        "    Function(render: Frame)\n"
        "      Signature\n"
        "  Face(Spacecraft is Renderable Movable)\n"
        "    Function(trajectory: Vector)\n"
        "      Signature\n"
        "    Function(label: String)\n"
        "      Block\n"
        "        Return\n"
        "          String\n"
        "            Text(\"spacecraft\")\n"
        "  Class(ExplorerShip has Spacecraft Trackable)\n"
        "    Let(name: String)\n"
        "      String\n"
        "        Text(\"Explorer\")\n"
        "    Function(render: Frame)\n"
        "      Block\n"
        "        Return\n"
        "          Call\n"
        "            Member(draw)\n"
        "              Identifier(self)\n"
        "    Function(.calculate_internal_state: Vector)\n"
        "      Block\n"
        "        Return\n"
        "          Member(position)\n"
        "            Identifier(self)\n"
        "  Enum(MissionState)\n"
        "    EnumMember(planned)\n"
        "    EnumMember(running)\n"
        "    EnumMember(complete)\n");
    passed &= expect_syntax_error("top-level function signature", "fun incomplete(value: Float): Float\n");
    passed &= expect_syntax_error("face field", "face Invalid {\n  let value: Float\n}\n");
    passed &= expect_syntax_error("class method signature", "class Invalid {\n  fun incomplete()\n}\n");
    passed &= expect_syntax_error("enum literal member", "enum Invalid {\n  1\n}\n");
    passed &= expect_syntax_error("trailing composition comma", "face Invalid is Renderable, {\n}\n");
    passed &= expect_syntax_error("unterminated type body", "class Invalid {\n  let value = 1\n");
    passed &= expect_ast(
        "modules imports and exports",
        "module orbital_demo\n"
        "import math\n"
        "import Vector from math\n"
        "import Renderer from rendering as SceneRenderer\n"
        "import physics as simulation_physics\n"
        "export ExplorerShip\n"
        "export GuidanceStatus as Status\n"
        "class ExplorerShip {\n"
        "  fun launch() {\n"
        "  }\n"
        "}\n"
        "enum GuidanceStatus {\n"
        "  ready\n"
        "}\n",
        "Program\n"
        "  Module(orbital_demo)\n"
        "  Import(math)\n"
        "  Import(Vector from math)\n"
        "  Import(Renderer from rendering as SceneRenderer)\n"
        "  Import(physics as simulation_physics)\n"
        "  Export(ExplorerShip)\n"
        "  Export(GuidanceStatus as Status)\n"
        "  Class(ExplorerShip)\n"
        "    Function(launch)\n"
        "      Block\n"
        "  Enum(GuidanceStatus)\n"
        "    EnumMember(ready)\n");
    passed &= expect_syntax_error("duplicate module declaration", "module one\nmodule two\n");
    passed &= expect_syntax_error("late module declaration", "import math\nmodule invalid\n");
    passed &= expect_syntax_error("import without name", "import\n");
    passed &= expect_syntax_error("import from without module", "import Vector from\n");
    passed &= expect_syntax_error("import alias without name", "import math as\n");
    passed &= expect_syntax_error("export without name", "export\n");
    passed &= expect_ast(
        "documentation comment attachment",
        "/// Orbital demonstration module.\n"
        "module documented\n"
        "/// Imports vector mathematics.\n"
        "import Vector from math\n"
        "/**Exports the spacecraft type.\nAcross modules.*/\n"
        "export Ship\n"
        "/// Default altitude.\n"
        "/// Measured in kilometers.\n"
        "let altitude = 1\n"
        "/// Performs launch preparation.\n"
        "fun prepare() {\n"
        "  /// Retry counter.\n"
        "  let retries = 0\n"
        "}\n"
        "/// Objects that can render.\n"
        "face Renderable {\n"
        "  /// Produces a frame.\n"
        "  fun render(): Frame\n"
        "}\n"
        "/**A documented spacecraft.*/\n"
        "class Ship {\n"
        "  /// Display name.\n"
        "  let name = \"Sagan\"\n"
        "  /// Advances the spacecraft.\n"
        "  fun fly() {\n"
        "  }\n"
        "}\n"
        "/// Mission states.\n"
        "enum Status {\n"
        "  /// Ready to begin.\n"
        "  ready\n"
        "}\n",
        "Program\n"
        "  Module(documented)\n"
        "    Documentation(\"Orbital demonstration module.\")\n"
        "  Import(Vector from math)\n"
        "    Documentation(\"Imports vector mathematics.\")\n"
        "  Export(Ship)\n"
        "    Documentation(\"Exports the spacecraft type.\\nAcross modules.\")\n"
        "  Let(altitude)\n"
        "    Documentation(\"Default altitude.\")\n"
        "    Documentation(\"Measured in kilometers.\")\n"
        "    Integer(1)\n"
        "  Function(prepare)\n"
        "    Documentation(\"Performs launch preparation.\")\n"
        "    Block\n"
        "      Let(retries)\n"
        "        Documentation(\"Retry counter.\")\n"
        "        Integer(0)\n"
        "  Face(Renderable)\n"
        "    Documentation(\"Objects that can render.\")\n"
        "    Function(render: Frame)\n"
        "      Documentation(\"Produces a frame.\")\n"
        "      Signature\n"
        "  Class(Ship)\n"
        "    Documentation(\"A documented spacecraft.\")\n"
        "    Let(name)\n"
        "      Documentation(\"Display name.\")\n"
        "      String\n"
        "        Text(\"Sagan\")\n"
        "    Function(fly)\n"
        "      Documentation(\"Advances the spacecraft.\")\n"
        "      Block\n"
        "  Enum(Status)\n"
        "    Documentation(\"Mission states.\")\n"
        "    EnumMember(ready)\n"
        "      Documentation(\"Ready to begin.\")\n");
    passed &= expect_syntax_error("orphaned documentation comment", "/// No declaration follows.\n");
    passed &= expect_syntax_error("documentation before control flow",
                                  "fun invalid() {\n  /// Not a declaration.\n  if ready {\n  }\n}\n");
    passed &= expect_syntax_error("orphaned enum-member documentation",
                                  "enum Invalid {\n  /// No member follows.\n}\n");
    passed &= expect_syntax_error("documentation requires following newline", "/**Same line.*/ fun invalid() {\n}\n");
    passed &= expect_ast(
        "expression-bodied functions and lambdas",
        "fun square(value: Float): Float => value * value\n"
        "face Mapper {\n"
        "  fun map(value: Float): Float => transform(value)\n"
        "}\n"
        "class Calculator {\n"
        "  fun .double(value: Float): Float => value * 2.0\n"
        "}\n"
        "let greater = fun(left: Float, right: Float): Bool => left > right\n"
        "let incremented = (fun(value) => value + 1)(4)\n",
        "Program\n"
        "  Function(square: Float)\n"
        "    Parameter(value: Float)\n"
        "    ExpressionBody\n"
        "      Binary(*)\n"
        "        Identifier(value)\n"
        "        Identifier(value)\n"
        "  Face(Mapper)\n"
        "    Function(map: Float)\n"
        "      Parameter(value: Float)\n"
        "      ExpressionBody\n"
        "        Call\n"
        "          Identifier(transform)\n"
        "          Identifier(value)\n"
        "  Class(Calculator)\n"
        "    Function(.double: Float)\n"
        "      Parameter(value: Float)\n"
        "      ExpressionBody\n"
        "        Binary(*)\n"
        "          Identifier(value)\n"
        "          Float(2.0)\n"
        "  Let(greater)\n"
        "    Lambda(: Bool)\n"
        "      Parameter(left: Float)\n"
        "      Parameter(right: Float)\n"
        "      Binary(>)\n"
        "        Identifier(left)\n"
        "        Identifier(right)\n"
        "  Let(incremented)\n"
        "    Call\n"
        "      Group\n"
        "        Lambda\n"
        "          Parameter(value)\n"
        "          Binary(+)\n"
        "            Identifier(value)\n"
        "            Integer(1)\n"
        "      Integer(4)\n");
    passed &= expect_syntax_error("lambda missing arrow", "let invalid = fun(value) value\n");
    passed &= expect_syntax_error("expression body missing value", "fun invalid() =>\n");
    passed &= expect_visual_ast("visual AST renderers", "let result = 1 + value\n");
    passed &= expect_visual_ast("visual statement AST",
                                "fun choose(value) => value\nlet mapper = fun(value) => value * 2\n"
                                "fun generate(value) {\n  yield value\n}\n");
    passed &= expect_visual_ast("visual module AST",
                                "module demo\nimport Vector from math as Vector3\nexport Ship\nclass Ship {\n}\n"
                                "let vessel = Ship\n");
    passed &= expect_visual_ast("visual documentation AST",
                                "/// Documented value.\nlet value = other\n");
    passed &= expect_visual_ast(
        "visual escaped and truncated string AST",
        "let value = \"&'<>\\r\\tabcdefghijklmnopqrstuvwxyz0123456789\" + other\n");
    passed &= expect_renderer_rejects_unknown_nodes();
    passed &= expect_semantic_model(
        "semantic scopes and resolutions",
        "face Named {\n  fun name(): String\n}\n"
        "class Probe has Named {\n  let label: String = \"Sagan\"\n"
        "  fun name(): String {\n    return self.label\n  }\n}\n"
        "fun choose(value: Float): Float {\n  let baseline = 1.0\n"
        "  if value > baseline {\n    return value\n  }\n  return baseline\n}\n"
        "let selected = choose(2.0)\n",
        {"Symbol(type Named", "Symbol(type Probe", "Symbol(function choose", "Scope(",
         "Symbol(parameter value", "baseline @", "choose @"});
    passed &= expect_semantic_model(
        "semantic traversal coverage",
        "module semantic_demo\n"
        "import External from support as Imported\n"
        "face Capability {\n  fun inspect(value: Float): String\n}\n"
        "class Worker is Capability {\n"
        "  let label: String = \"worker\"\n"
        "  fun inspect(value: Float): String => \"${self.label}: ${value}\"\n"
        "}\n"
        "enum State {\n  ready\n  waiting\n}\n"
        "fun overloaded(value: Float): Float => value\n"
        "fun overloaded(value: String): String => value\n"
        "fun exercise(input: Float, values: Vector): Float {\n"
        "  let total: Float = input\n"
        "  let grouped = (total)\n"
        "  let negative = -grouped\n"
        "  let selected = input > 0 ? input ; total\n"
        "  let assigned = total := selected\n"
        "  let called = overloaded(assigned)\n"
        "  let indexed = values[0]\n"
        "  let member = values.length\n"
        "  let text = \"value ${called}\"\n"
        "  let items = [called, ...values]\n"
        "  let mapping = {called: indexed, ...values}\n"
        "  let pair = <called, indexed>\n"
        "  let point = (called, indexed)\n"
        "  let mapper = fun(value: Float): Float => value + total\n"
        "  total += called\n"
        "  if input > 0 {\n    total\n  } else {\n    total\n  }\n"
        "  while input > 0 {\n    break\n  }\n"
        "  until input > 0 {\n    continue\n  }\n"
        "  for item in values {\n    let copy = item\n    yield copy\n  }\n"
        "  match input {\n    case 0 { total\n    }\n    case else { total\n    }\n  }\n"
        "  hope {\n    total\n  } unless 0 {\n    scream total\n  } finally {\n    total\n  }\n"
        "  return mapper(total)\n"
        "}\n"
        "export exercise\n"
        "let result = exercise(1.0, <1.0, 2.0>)\n",
        {"Symbol(import Imported", "Symbol(type Capability", "Symbol(type Worker",
         "Symbol(enum member ready", "Symbol(function overloaded", "Scope(",
         "loop binding item", "Scope(", "lambda", "exercise @"});
    passed &= expect_semantic_error("duplicate declaration",
                                    "let value = 1\nlet value = 2\n", "Duplicate declaration of 'value'");
    passed &= expect_semantic_error("undefined name", "let value = missing\n", "Undefined name 'missing'");
    passed &= expect_semantic_error("local declaration order", "fun invalid() {\n  let value = value\n}\n",
                                    "Undefined name 'value'");
    passed &= expect_semantic_error("duplicate parameter",
                                    "fun invalid(value, value) => value\n",
                                    "Duplicate declaration of 'value'");
    passed &= expect_semantic_error("annotation must name a type",
                                    "let NotAType = 1\nlet value: NotAType = 2\n",
                                    "'NotAType' does not name a type");
    passed &= expect_type_model(
        "built-in type inference and checking",
        "fun choose(value: Float, enabled: Bool): Float {\n"
        "  let baseline = 1.0\n"
        "  if enabled {\n    return value > baseline ? value ; baseline\n  }\n"
        "  return baseline\n}\n"
        "let count: Int = 2\n"
        "let selected = choose(2.0, true)\n",
        {"count: Int8", "selected: Float64", "baseline: Float64", "Bool @", "Int8 @", "Float64 @"});
    passed &= expect_type_model(
        "exact overload selection",
        "fun identity(value: Int): Int => value\n"
        "fun identity(value: String): String => value\n"
        "let number = identity(1)\n"
        "let text = identity(\"Sagan\")\n",
        {"number: Int64", "text: String"});
    passed &= expect_type_model(
        "smallest fitting integer widths",
        "let tiny = 127\nlet small = 128\nlet medium = 32768\n"
        "let large = 2147483648\nlet negative = -128\nlet wider_negative = -129\n"
        "let widened = tiny + small\n",
        {"tiny: Int8", "small: Int16", "medium: Int32", "large: Int64",
         "negative: Int8", "wider_negative: Int16", "widened: Int16"});
    passed &= expect_type_model(
        "floating widths",
        "let default_value = 1.0\nlet precise: Float64 = 2.0\nlet compact: Float32 = 3.0\n",
        {"default_value: Float64", "precise: Float64", "compact: Float32"});
    passed &= expect_type_model(
        "default API widths and lossless numeric widening",
        "fun widen_integer(value: Int): Int => value\n"
        "fun widen_float(value: Float): Float => value\n"
        "let default_integer: Int\nlet default_float: Float\n"
        "let widened_integer = widen_integer(1)\n"
        "let widened_float = widen_float(32767)\n",
        {"value: Int64", "value: Float64", "default_integer: Int64", "default_float: Float64",
         "widened_integer: Int64", "widened_float: Float64"});
    passed &= expect_type_model(
        "type traversal across declarations and control flow",
        "class Label {\n  let text: String\n  fun read(): String => self.text\n}\n"
        "fun traverse(enabled: Bool, values: Vector): Int {\n"
        "  while enabled {\n    break\n  }\n"
        "  until enabled {\n    continue\n  }\n"
        "  for item in values {\n    yield item\n  }\n"
        "  match 1 {\n    case 1 {\n      enabled\n    }\n    case else {\n      enabled\n    }\n  }\n"
        "  hope {\n    enabled\n  } unless 1 {\n    scream 2\n  } finally {\n    enabled\n  }\n"
        "  return 0\n}\n",
        {"text: String", "enabled: Bool", "values: Vector", "item: Unknown", "Int8 @"});
    passed &= expect_type_model(
        "definite return through branches and match",
        "fun choose(flag: Bool): Int {\n"
        "  if flag {\n    return 1\n  } else {\n    return 2\n  }\n}\n"
        "fun classify(value: Int): String {\n"
        "  match value {\n    case 0 {\n      return \"zero\"\n    }\n"
        "    case else {\n      return \"other\"\n    }\n  }\n}\n",
        {"flag: Bool", "value: Int64"});
    passed &= expect_type_model(
        "homogeneous array inference indexing and iteration",
        "fun first(values) {\n  for value in values {\n    yield value\n  }\n  return values[0]\n}\n"
        "let small = [1, 2]\nlet mixed_width = [1, 200]\n"
        "let combined = [...small, 3]\nlet selected = mixed_width[0]\n",
        {"small: Array<Int8>", "mixed_width: Array<Int16>", "combined: Array<Int8>",
         "selected: Int16", "value: Unknown"});
    passed &= expect_type_model(
        "homogeneous dictionary inference indexing and spreads",
        "let counts = {\"one\": 1, \"two\": 200}\n"
        "let more = {...counts, \"three\": 3}\n"
        "let selected = more[\"two\"]\n",
        {"counts: Dictionary<String, Int16>", "more: Dictionary<String, Int16>", "selected: Int16"});
    passed &= expect_type_model(
        "dimensioned vector and coordinate inference",
        "let direction = <1.0, 0.0, 0.0>\n"
        "let origin = (0, 0, 0)\n"
        "let extended = <...direction, 1.0>\n"
        "let component = direction[1]\n",
        {"direction: Vector3<Float64>", "origin: Coordinate3<Int8>",
         "extended: Vector4<Float64>", "component: Float64"});
    passed &= expect_type_error("initializer type mismatch", "let value: Bool = 1\n",
                                "Variable initializer requires Bool, but received Int");
    passed &= expect_type_error("uninferable variable", "let pending\n",
                                "requires a type annotation or initializer");
    passed &= expect_type_error("condition type mismatch",
                                "fun invalid(): Int {\n  if 1 {\n    return 1\n  }\n  return 0\n}\n",
                                "If condition requires Bool, but received Int");
    passed &= expect_type_error("missing definite return",
                                "fun incomplete(flag: Bool): Int {\n"
                                "  if flag {\n    return 1\n  }\n}\n",
                                "may reach the end without returning Int64");
    passed &= expect_type_model(
        "definite initialization by assignment",
        "fun initialize(): Int {\n  let value: Int\n  value = 1\n  return value\n}\n",
        {"value: Int64"});
    passed &= expect_type_error("use before initialization",
                                "fun invalid(): Int {\n  let value: Int\n  return value\n}\n",
                                "Variable 'value' is used before initialization");
    passed &= expect_type_error("compound assignment before initialization",
                                "fun invalid(): Int {\n  let value: Int\n  value += 1\n  return value\n}\n",
                                "Variable 'value' is used before initialization");
    passed &= expect_type_error("partial branch initialization",
                                "fun invalid(flag: Bool): Int {\n  let value: Int\n"
                                "  if flag {\n    value = 1\n  }\n  return value\n}\n",
                                "Variable 'value' is used before initialization");
    passed &= expect_type_model(
        "complete branch initialization",
        "fun valid(flag: Bool): Int {\n  let value: Int\n"
        "  if flag {\n    value = 1\n  } else {\n    value = 2\n  }\n  return value\n}\n",
        {"value: Int64"});
    passed &= expect_type_error("unreachable statement",
                                "fun invalid(): Int {\n  return 1\n  let unreachable = 2\n}\n",
                                "Unreachable statement");
    passed &= expect_type_error("unreachable statement in Void function",
                                "fun invalid(): Void {\n  return\n  let unreachable = 2\n}\n",
                                "Unreachable statement");
    passed &= expect_type_error("invalid assignment target",
                                "fun invalid(): Void {\n  1 = 2\n}\n",
                                "Assignment target is not assignable");
    passed &= expect_type_error("return type mismatch", "fun invalid(): Bool => 1\n",
                                "Function return requires Bool, but received Int");
    passed &= expect_type_error("operator type mismatch", "let invalid = true + 1\n",
                                "requires numeric operands");
    passed &= expect_type_error("call overload mismatch",
                                "fun identity(value: Int): Int => value\nlet invalid = identity(true)\n",
                                "No matching overload for 'identity'");
    passed &= expect_type_error("floating narrowing",
                                "let wide: Float64 = 1.0\nlet narrow: Float32 = wide\n",
                                "Variable initializer requires Float32, but received Float64");
    passed &= expect_type_error("lossy integer to float conversion",
                                "fun consume(value: Float32): Float32 => value\n"
                                "let invalid = consume(2147483647)\n",
                                "No matching overload for 'consume'");
    passed &= expect_type_error("heterogeneous array",
                                "let invalid = [1, \"two\"]\n", "Array elements have incompatible types");
    passed &= expect_type_error("empty inferred array",
                                "let invalid = []\n", "Empty array requires an explicit element type");
    passed &= expect_type_error("array index type",
                                "let values = [1, 2]\nlet invalid = values[true]\n",
                                "Array index requires Int, but received Bool");
    passed &= expect_type_error("heterogeneous dictionary keys",
                                "let invalid = {1: true, \"two\": false}\n",
                                "Dictionary keys have incompatible types");
    passed &= expect_type_error("heterogeneous dictionary values",
                                "let invalid = {\"one\": 1, \"two\": false}\n",
                                "Dictionary values have incompatible types");
    passed &= expect_type_error("empty inferred dictionary",
                                "let invalid = {}\n", "Empty dictionary requires explicit key and value types");
    passed &= expect_type_error("dictionary key type",
                                "let values = {\"one\": 1}\nlet invalid = values[true]\n",
                                "Dictionary key requires String, but received Bool");
    passed &= expect_type_error("vector component type",
                                "let invalid = <1.0, true>\n", "Vector components must be numeric");
    passed &= expect_type_error("vector dimension mismatch",
                                "fun consume(value: Vector): Vector => value\n"
                                "let left = <1.0, 2.0>\nlet right = <1.0, 2.0, 3.0>\n"
                                "let invalid = true ? left ; right\n",
                                "Conditional branches have incompatible types");
    passed &= expect_entry_point("Int entry point", "fun main(): Int => 0\n");
    passed &= expect_entry_point("Void entry point", "fun main(): Void {\n  return\n}\n");
    passed &= expect_entry_error("missing entry point", "let library_value = 1\n",
                                 "requires a 'main' entry point");
    passed &= expect_entry_error("entry point parameters", "fun main(value: Int): Int => value\n",
                                 "cannot declare parameters");
    passed &= expect_entry_error("entry point result", "fun main(): String => \"invalid\"\n",
                                 "must return Int or Void");
    passed &= expect_entry_error("duplicate entry point",
                                 "fun main(): Int => 0\nfun main(): Int => 1\n",
                                 "more than one 'main' entry point");

    std::cout << (passed ? "All front-end tests passed.\n" : "Front-end tests failed.\n");
    return passed ? 0 : 1;
  }

  auto read_file(const std::string &path) -> std::string
  {
    if (!std::filesystem::is_regular_file(path))
    {
      throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
      throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
  }

  auto write_file(const std::string &path, const std::string &contents) -> void
  {
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
      throw std::runtime_error("Could not write '" + path + "'");
    }
    output << contents;
  }

  auto print_error(const std::string &source, const auto &error, const std::string &category) -> void
  {
    int line = 1;
    int column = 1;
    std::size_t i = 0;
    while (i < static_cast<std::size_t>(error.range.begin) && i < source.size())
    {
      if (source[i] == '\n')
      {
        line++;
        column = 1;
        i++;
      }
      else
      {
        column++;
        const auto current = sagan::unicode::decode(source, i);
        i += current ? current->width : 1;
      }
    }
    std::cerr << category << " error at " << line << ':' << column << ": " << error.what() << '\n';
  }
}

auto main(const int argc, char **argv) -> int
{
  if (argc == 2 && std::string(argv[1]) == "--version")
  {
    std::cout << "Sagan " << SAGAN_VERSION << '\n';
    return 0;
  }

  if (argc == 2 && std::string(argv[1]) == "--self-test")
  {
    return run_self_tests();
  }

  enum class output_mode
  {
    tokens,
    ast_text,
    ast_dot,
    ast_svg,
    ast_html,
    semantic,
    types,
    entry,
    emit_cpp,
  };

  output_mode mode = output_mode::tokens;
  std::string path;
  std::string output_path;
  if (argc == 3 && std::string(argv[1]) == "--ast")
  {
    mode = output_mode::ast_text;
    path = argv[2];
  }
  else if (argc == 3 && std::string(argv[1]) == "--ast-dot")
  {
    mode = output_mode::ast_dot;
    path = argv[2];
  }
  else if (argc == 4 && std::string(argv[1]) == "--ast-svg")
  {
    mode = output_mode::ast_svg;
    path = argv[2];
    output_path = argv[3];
  }
  else if (argc == 4 && std::string(argv[1]) == "--ast-html")
  {
    mode = output_mode::ast_html;
    path = argv[2];
    output_path = argv[3];
  }
  else if (argc == 3 && std::string(argv[1]) == "--semantic")
  {
    mode = output_mode::semantic;
    path = argv[2];
  }
  else if (argc == 3 && std::string(argv[1]) == "--types")
  {
    mode = output_mode::types;
    path = argv[2];
  }
  else if (argc == 3 && std::string(argv[1]) == "--entry")
  {
    mode = output_mode::entry;
    path = argv[2];
  }
  else if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--emit-cpp")
  {
    mode = output_mode::emit_cpp;
    path = argv[2];
    if (argc == 4) output_path = argv[3];
  }
  else if (argc == 2)
  {
    path = argv[1];
  }
  else if (argc == 1)
  {
    path = "examples/tokenizer_demo.sagan";
  }
  else
  {
    std::cerr << "usage: sagan [--version | --self-test | --ast FILE | --ast-dot FILE | "
                 "--ast-svg FILE OUTPUT | --ast-html FILE OUTPUT | --semantic FILE | --types FILE | "
                 "--entry FILE | --emit-cpp FILE [OUTPUT] | FILE]\n";
    return 2;
  }
  const bool ast_mode = mode != output_mode::tokens;
  try
  {
    const std::string source = read_file(path);
    const auto result = tokenize(source);

    if (ast_mode)
    {
      parser::syntax_parser syntax(result);
      const auto tree = syntax.parse();
      if (mode == output_mode::semantic)
      {
        const auto model = semantic::analyze(tree);
        std::cout << "Sagan " << SAGAN_VERSION << " semantic model: " << path << "\n\n";
        model.print(std::cout);
      }
      else if (mode == output_mode::types)
      {
        static_cast<void>(semantic::analyze(tree));
        const auto model = semantic::check_types(tree);
        std::cout << "Sagan " << SAGAN_VERSION << " type model: " << path << "\n\n";
        model.print(std::cout);
      }
      else if (mode == output_mode::entry)
      {
        static_cast<void>(semantic::analyze(tree));
        static_cast<void>(semantic::check_types(tree));
        semantic::validate_entry_point(tree);
        std::cout << "Sagan " << SAGAN_VERSION << " executable entry point is valid: " << path << '\n';
      }
      else if (mode == output_mode::emit_cpp)
      {
        static_cast<void>(semantic::analyze(tree));
        static_cast<void>(semantic::check_types(tree));
        semantic::validate_entry_point(tree);
        const std::string generated = codegen::generate_cpp(tree);
        if (output_path.empty()) std::cout << generated;
        else
        {
          write_file(output_path, generated);
          std::cout << "Wrote generated C++ to " << output_path << '\n';
        }
      }
      else if (mode == output_mode::ast_text)
      {
        std::cout << "Sagan " << SAGAN_VERSION << " AST: " << path << "\n\n";
        tree.print(std::cout);
      }
      else if (mode == output_mode::ast_dot)
      {
        std::cout << parser::render_ast_dot(tree);
      }
      else if (mode == output_mode::ast_svg)
      {
        write_file(output_path, parser::render_ast_svg(tree));
        std::cout << "Wrote SVG AST to " << output_path << '\n';
      }
      else
      {
        write_file(output_path, parser::render_ast_html(source, tree, "Sagan AST: " + path));
        std::cout << "Wrote visual AST demo to " << output_path << '\n';
      }
      return 0;
    }

    std::cout << "Sagan " << SAGAN_VERSION << " tokenizer: " << path << "\n\n";
    std::cout << std::left << std::setw(24) << "TOKEN" << std::setw(18) << "SPAN" << "TEXT\n";
    std::cout << std::string(72, '-') << '\n';
    for (const auto &tok : result)
    {
      const std::string range = std::to_string(tok.range.begin) + ".." + std::to_string(tok.range.end);
      std::cout << std::left << std::setw(24) << tokens::name(tok.id) << std::setw(18) << range
                << escape_text(tok.text) << '\n';
    }
  }
  catch (const parser::parse_error &error)
  {
    try
    {
      print_error(read_file(path), error, ast_mode ? "syntax" : "lexical");
    }
    // LCOV_EXCL_START - requires the already-read source file to disappear during diagnostic handling.
    catch (const std::exception &)
    {
      std::cerr << "lexical error: " << error.what() << '\n';
    }
    // LCOV_EXCL_STOP
    return 1;
  }
  catch (const semantic::semantic_error &error)
  {
    print_error(read_file(path), error, "semantic");
    return 1;
  }
  catch (const std::exception &error)
  {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
