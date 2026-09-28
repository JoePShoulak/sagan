#include "parser/lex.hpp"
#include "parser/parse_error.hpp"
#include "parser/tokenizer.hpp"
#include "parser/tokens.hpp"
#include "parser/unicode.hpp"
#include "version.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
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
      std::cerr << "[FAIL] " << name << ": expected " << expected.size() << " tokens, got " << actual.size()
                << '\n';
      return false;
    }

    for (std::size_t i = 0; i < expected.size(); i++)
    {
      if (actual[i].id != expected[i])
      {
        std::cerr << "[FAIL] " << name << ": token " << i << " expected " << tokens::name(expected[i])
                  << ", got " << tokens::name(actual[i].id) << '\n';
        return false;
      }
    }

    std::cout << "[PASS] " << name << '\n';
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

    std::cerr << "[FAIL] " << name << ": expected a lexical error\n";
    return false;
  }

  auto expect_token_text(const std::string &name, const std::string &source, const std::size_t index,
                         const std::string &expected) -> bool
  {
    const auto actual = tokenize(source);
    if (index >= actual.size() || actual[index].text != expected)
    {
      std::cerr << "[FAIL] " << name << ": expected token text '" << expected << "'\n";
      return false;
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
      catch (const std::exception &error)
      {
        std::cerr << "[FAIL] " << name << ": unexpected exception: " << error.what() << '\n';
        return false;
      }
    }
    std::cout << "[PASS] " << name << '\n';
    return true;
  }

  auto run_self_tests() -> int
  {
    bool passed = true;

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
    passed &= expect_robustness("random byte robustness");

    std::cout << (passed ? "All tokenizer tests passed.\n" : "Tokenizer tests failed.\n");
    return passed ? 0 : 1;
  }

  auto read_file(const std::string &path) -> std::string
  {
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
      throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
  }

  auto print_error(const std::string &source, const parser::parse_error &error) -> void
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
    std::cerr << "lexical error at " << line << ':' << column << ": " << error.what() << '\n';
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

  const std::string path = argc == 2 ? argv[1] : "examples/tokenizer_demo.sagan";
  try
  {
    const std::string source = read_file(path);
    const auto result = tokenize(source);

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
      print_error(read_file(path), error);
    }
    catch (const std::exception &)
    {
      std::cerr << "lexical error: " << error.what() << '\n';
    }
    return 1;
  }
  catch (const std::exception &error)
  {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
