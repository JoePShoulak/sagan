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
    return parse_assignment();
  }

  auto syntax_parser::parse_assignment() -> expression_ref
  {
    auto target = parse_conditional();
    if (!match(tokens::ASSIGN_VALUE))
    {
      return target;
    }
    auto value = parse_assignment();
    const span range{target->range.begin, value->range.end};
    return std::make_unique<assignment_expression>(range, std::move(target), std::move(value));
  }

  auto syntax_parser::parse_conditional() -> expression_ref
  {
    auto condition = parse_or();
    if (!match(tokens::QUESTION))
    {
      return condition;
    }
    auto when_true = parse_assignment();
    expect(tokens::SEMICOLON, "';' between conditional branches");
    auto when_false = parse_conditional();
    const span range{condition->range.begin, when_false->range.end};
    return std::make_unique<conditional_expression>(range, std::move(condition), std::move(when_true),
                                                     std::move(when_false));
  }

  auto syntax_parser::parse_or() -> expression_ref
  {
    auto left = parse_and();
    while (match(tokens::KWD_OR))
    {
      const token operation = previous();
      auto right = parse_and();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_and() -> expression_ref
  {
    auto left = parse_equality();
    while (match(tokens::KWD_AND))
    {
      const token operation = previous();
      auto right = parse_equality();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_equality() -> expression_ref
  {
    auto left = parse_comparison();
    while (match(tokens::EQUAL_EQUAL) || match(tokens::BANG_EQUAL))
    {
      const token operation = previous();
      auto right = parse_comparison();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_comparison() -> expression_ref
  {
    auto left = parse_additive();
    const bool has_comparison = match(tokens::LANGLE) || match(tokens::LESS_EQUAL) || match(tokens::RANGLE) ||
                                match(tokens::GREATER_EQUAL) || match(tokens::KWD_IS);
    if (!has_comparison)
    {
      return left;
    }
    const token operation = previous();
    auto right = parse_additive();
    if (check(tokens::LANGLE) || check(tokens::LESS_EQUAL) || check(tokens::RANGLE) ||
        check(tokens::GREATER_EQUAL) || check(tokens::KWD_IS))
    {
      throw parse_error("Chained comparisons are not allowed; combine comparisons with 'and'", peek()->range);
    }
    const span range{left->range.begin, right->range.end};
    return std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
  }

  auto syntax_parser::parse_additive() -> expression_ref
  {
    auto left = parse_multiplicative();
    while (match(tokens::PLUS) || match(tokens::MINUS))
    {
      const token operation = previous();
      auto right = parse_multiplicative();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_multiplicative() -> expression_ref
  {
    auto left = parse_unary();
    while (match(tokens::STAR) || match(tokens::SLASH) || match(tokens::PERCENT))
    {
      const token operation = previous();
      auto right = parse_unary();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_unary() -> expression_ref
  {
    if (match(tokens::PLUS_PLUS) || match(tokens::MINUS_MINUS) || match(tokens::BANG) ||
        match(tokens::KWD_NOT) || match(tokens::PLUS) || match(tokens::MINUS))
    {
      const token operation = previous();
      auto operand = parse_unary();
      return std::make_unique<unary_expression>(span{operation.range.begin, operand->range.end}, operation.text,
                                                std::move(operand));
    }
    return parse_exponent();
  }

  auto syntax_parser::parse_exponent() -> expression_ref
  {
    auto left = parse_postfix();
    if (!match(tokens::CARET))
    {
      return left;
    }
    const token operation = previous();
    auto right = parse_unary();
    const span range{left->range.begin, right->range.end};
    return std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
  }

  auto syntax_parser::parse_postfix() -> expression_ref
  {
    auto value = parse_primary();
    while (match(tokens::PLUS_PLUS) || match(tokens::MINUS_MINUS))
    {
      const token operation = previous();
      value = std::make_unique<unary_expression>(span{value->range.begin, operation.range.end}, operation.text,
                                                 std::move(value), true);
    }
    return value;
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
    if (match(tokens::KWD_TRUE) || match(tokens::KWD_FALSE))
    {
      const token &value = previous();
      return std::make_unique<literal_expression>(value.range, literal_expression::kind::boolean, value.text,
                                                  value.id == tokens::KWD_TRUE ? 1.0 : 0.0);
    }
    if (match(tokens::KWD_INF) || match(tokens::KWD_NAN))
    {
      const token &value = previous();
      return std::make_unique<literal_expression>(value.range, literal_expression::kind::floating_point,
                                                  value.text, value.value);
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
