#include "parser/lex.hpp"
#include "parser/ast_render.hpp"
#include "parser/parse_error.hpp"
#include "parser/parser.hpp"
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

  auto expect_ast(const std::string &name, const std::string &source, const std::string &expected) -> bool
  {
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    std::ostringstream output;
    tree.print(output);
    if (output.str() != expected)
    {
      std::cerr << "[FAIL] " << name << ":\n" << output.str();
      return false;
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
    std::cerr << "[FAIL] " << name << ": expected a syntax error\n";
    return false;
  }

  auto expect_visual_ast(const std::string &name, const std::string &source) -> bool
  {
    parser::syntax_parser syntax(tokenize(source));
    const auto tree = syntax.parse();
    const std::string dot = parser::render_ast_dot(tree);
    const std::string svg = parser::render_ast_svg(tree);
    const std::string html = parser::render_ast_html(source, tree, "AST test");
    const bool passed = dot.contains("digraph SaganAST") && dot.contains("Identifier") &&
                        svg.contains("<svg") && svg.contains("Identifier") &&
                        html.contains("Input source") && html.contains(source) &&
                        html.contains("data-action=\"fit\"") && html.contains("Wheel to zoom");
    if (!passed)
    {
      std::cerr << "[FAIL] " << name << ": visual output was incomplete\n";
      return false;
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
    passed &= expect_syntax_error("top-level expression statement", "launch()\n");
    passed &= expect_syntax_error("top-level control flow", "if ready {\n}\n");
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
    passed &= expect_visual_ast("visual AST renderers", "let result = 1 + value\n");
    passed &= expect_visual_ast("visual statement AST",
                                "fun update(items) {\n  for item in items {\n    if item.ready {\n"
                                "      return item\n    }\n  }\n  return\n}\n");

    std::cout << (passed ? "All front-end tests passed.\n" : "Front-end tests failed.\n");
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

  auto write_file(const std::string &path, const std::string &contents) -> void
  {
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
      throw std::runtime_error("Could not write '" + path + "'");
    }
    output << contents;
  }

  auto print_error(const std::string &source, const parser::parse_error &error, const std::string &category) -> void
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
                 "--ast-svg FILE OUTPUT | --ast-html FILE OUTPUT | FILE]\n";
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
      if (mode == output_mode::ast_text)
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
