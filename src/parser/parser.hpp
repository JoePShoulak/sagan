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
    std::size_t vector_literal_depth = 0;
    std::size_t loop_depth = 0;

    auto at_end() const -> bool;
    auto peek() const -> const token *;
    auto previous() const -> const token &;
    auto advance() -> const token &;
    auto check(int token_id) const -> bool;
    auto match(int token_id) -> bool;
    auto expect(int token_id, const std::string &description) -> const token &;
    auto skip_newlines() -> void;
    auto match_after_newlines(int token_id) -> bool;
    auto parse_documentation_comments() -> std::vector<documentation_comment>;
    auto parse_statement() -> statement_ref;
    auto parse_let_declaration(bool allow_private = false, bool weak_member = false) -> statement_ref;
    auto parse_const_declaration(bool allow_private = false) -> statement_ref;
    auto parse_function_declaration(bool body_optional = false, bool allow_private = false,
                                    bool allow_mutating = false) -> statement_ref;
    auto parse_test_declaration() -> statement_ref;
    auto parse_constructor_declaration() -> statement_ref;
    auto parse_type_declaration(type_declaration::kind type) -> statement_ref;
    auto parse_measurement_declaration(measurement_declaration::kind type) -> statement_ref;
    auto parse_module_declaration() -> statement_ref;
    auto parse_import_declaration() -> statement_ref;
    auto parse_export_declaration() -> statement_ref;
    auto parse_expression_statement() -> statement_ref;
    auto parse_if_statement() -> statement_ref;
    auto parse_condition_loop(condition_loop_statement::kind type) -> statement_ref;
    auto parse_for_statement() -> statement_ref;
    auto parse_loop_control(loop_control_statement::kind type) -> statement_ref;
    auto parse_return_statement() -> statement_ref;
    auto parse_yield_statement() -> statement_ref;
    auto parse_match_statement() -> statement_ref;
    auto parse_hope_statement() -> statement_ref;
    auto parse_scream_statement() -> statement_ref;
    auto parse_block() -> std::unique_ptr<block_statement>;
    auto parse_statement_body(const std::string &description) -> std::unique_ptr<block_statement>;
    auto parse_type_annotation(const std::string &description) -> std::string;
    auto parse_unit_expression(bool allow_composite = true) -> std::string;
    auto parse_qualified_name(const std::string &description) -> std::string;
    auto parse_expression() -> expression_ref;
    auto parse_nested_expression() -> expression_ref;
    auto parse_assignment() -> expression_ref;
    auto parse_conditional() -> expression_ref;
    auto parse_coalesce() -> expression_ref;
    auto parse_or() -> expression_ref;
    auto parse_and() -> expression_ref;
    auto parse_equality() -> expression_ref;
    auto parse_comparison() -> expression_ref;
    auto parse_additive() -> expression_ref;
    auto parse_multiplicative() -> expression_ref;
    auto parse_unary() -> expression_ref;
    auto parse_exponent() -> expression_ref;
    auto parse_postfix() -> expression_ref;
    auto parse_primary() -> expression_ref;
    auto parse_string() -> expression_ref;
    auto parse_array() -> expression_ref;
    auto parse_dictionary() -> expression_ref;
    auto parse_vector(collection_expression::kind type = collection_expression::kind::vector,
                      std::optional<int> prefix_begin = {}) -> expression_ref;
    auto parse_parenthesized(collection_expression::kind type = collection_expression::kind::point,
                             std::optional<int> prefix_begin = {}) -> expression_ref;
    auto parse_lambda() -> expression_ref;

  public:
    explicit syntax_parser(std::vector<token> tokens);
    auto parse() -> program;
  };
}
