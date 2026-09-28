#include "parser.hpp"

#include "parse_error.hpp"
#include "tokens.hpp"

#include <utility>

namespace parser
{
  syntax_parser::syntax_parser(std::vector<token> tokens) : input(std::move(tokens)) {}

  auto syntax_parser::at_end() const -> bool
  {
    return current >= input.size();
  }

  auto syntax_parser::peek() const -> const token *
  {
    return at_end() ? nullptr : &input[current];
  }

  auto syntax_parser::previous() const -> const token &
  {
    return input[current - 1];
  }

  auto syntax_parser::advance() -> const token &
  {
    if (!at_end())
    {
      current++;
    }
    return previous();
  }

  auto syntax_parser::check(const int token_id) const -> bool
  {
    return !at_end() && peek()->id == token_id;
  }

  auto syntax_parser::match(const int token_id) -> bool
  {
    if (!check(token_id))
    {
      return false;
    }
    advance();
    return true;
  }

  auto syntax_parser::expect(const int token_id, const std::string &description) -> const token &
  {
    if (match(token_id))
    {
      return previous();
    }
    const span error_range = at_end()
                                 ? (input.empty() ? span{0, 0}
                                                  : span{input.back().range.end, input.back().range.end})
                                 : peek()->range;
    const std::string found = at_end() ? "end of file" : "'" + peek()->text + "'";
    throw parse_error("Expected " + description + ", but found " + found, error_range);
  }

  auto syntax_parser::skip_newlines() -> void
  {
    while (match(tokens::NEWLINE))
    {
    }
  }

  auto syntax_parser::parse() -> program
  {
    std::vector<statement_ref> body;
    skip_newlines();
    while (!at_end())
    {
      body.push_back(parse_statement());
      if (!at_end())
      {
        expect(tokens::NEWLINE, "a newline after the declaration");
        skip_newlines();
      }
    }
    return program(std::move(body));
  }

  auto syntax_parser::parse_statement() -> statement_ref
  {
    if (match(tokens::KWD_LET))
    {
      return parse_let_declaration();
    }
    const span error_range = at_end() ? span{0, 0} : peek()->range;
    throw parse_error("Expected a declaration or statement", error_range);
  }

  auto syntax_parser::parse_let_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "an identifier after 'let'");
    std::optional<std::string> type_name;
    if (match(tokens::COLON))
    {
      type_name = expect(tokens::IDENTIFIER, "a type name after ':'").text;
    }

    expression_ref initializer;
    if (match(tokens::EQUAL))
    {
      initializer = parse_expression();
    }

    const int end = initializer ? initializer->range.end : (type_name ? previous().range.end : name.range.end);
    return std::make_unique<let_declaration>(span{keyword.range.begin, end}, name.text, std::move(type_name),
                                             std::move(initializer));
  }

  auto syntax_parser::parse_expression() -> expression_ref
  {
    return parse_primary();
  }

  auto syntax_parser::parse_primary() -> expression_ref
  {
    if (match(tokens::IDENTIFIER))
    {
      const token &value = previous();
      return std::make_unique<identifier_expression>(value.range, value.text);
    }
    if (match(tokens::INTEGER) || match(tokens::FLOAT))
    {
      const token &value = previous();
      const auto type = value.id == tokens::INTEGER ? literal_expression::kind::integer
                                                    : literal_expression::kind::floating_point;
      return std::make_unique<literal_expression>(value.range, type, value.text, value.value);
    }
    if (match(tokens::LPAREN))
    {
      const token &open = previous();
      auto value = parse_expression();
      const token &close = expect(tokens::RPAREN, "')' after the grouped expression");
      return std::make_unique<grouping_expression>(span{open.range.begin, close.range.end}, std::move(value));
    }
    const span error_range = at_end()
                                 ? (input.empty() ? span{0, 0}
                                                  : span{input.back().range.end, input.back().range.end})
                                 : peek()->range;
    throw parse_error("Expected an expression", error_range);
  }
}
