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
    passed &= expect_syntax_error("compound assignment without value", "fun invalid() {\n  total +=\n}\n");
    passed &= expect_syntax_error("chained compound assignment",
                                  "fun invalid() {\n  first += second += third\n}\n");
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
                                "fun choose(value) => value\nlet mapper = fun(value) => value * 2\n");
    passed &= expect_visual_ast("visual module AST",
                                "module demo\nimport Vector from math as Vector3\nexport Ship\nclass Ship {\n}\n"
                                "let vessel = Ship\n");

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
