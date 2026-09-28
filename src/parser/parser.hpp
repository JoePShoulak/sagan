#pragma once

#include "ast_node.hpp"
#include "token.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace parser
{
  class syntax_parser
  {
    std::vector<token> input;
    std::size_t current = 0;

    auto at_end() const -> bool;
    auto peek() const -> const token *;
    auto previous() const -> const token &;
    auto advance() -> const token &;
    auto check(int token_id) const -> bool;
    auto match(int token_id) -> bool;
    auto expect(int token_id, const std::string &description) -> const token &;
    auto skip_newlines() -> void;
    auto parse_statement() -> statement_ref;
    auto parse_let_declaration() -> statement_ref;
    auto parse_expression() -> expression_ref;
    auto parse_primary() -> expression_ref;

  public:
    explicit syntax_parser(std::vector<token> tokens);
    auto parse() -> program;
  };
}
