#pragma once

#include "span.hpp"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace parser
{
  struct ast_node
  {
    span range;

    explicit ast_node(span source_range) : range(source_range) {}
    virtual ~ast_node() = default;
    virtual auto print(std::ostream &stream, int indent = 0) const -> void = 0;
  };

  struct expression : ast_node
  {
    using ast_node::ast_node;
  };

  using expression_ref = std::unique_ptr<expression>;

  struct identifier_expression final : expression
  {
    std::string name;

    identifier_expression(span source_range, std::string identifier);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct literal_expression final : expression
  {
    enum class kind
    {
      integer,
      floating_point,
      boolean,
    };

    kind literal_kind;
    std::string spelling;
    double numeric_value;

    literal_expression(span source_range, kind type, std::string source_spelling, double value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct grouping_expression final : expression
  {
    expression_ref value;

    grouping_expression(span source_range, expression_ref grouped_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct unary_expression final : expression
  {
    std::string operator_text;
    expression_ref operand;
    bool postfix;

    unary_expression(span source_range, std::string operation, expression_ref value, bool is_postfix = false);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct binary_expression final : expression
  {
    expression_ref left;
    std::string operator_text;
    expression_ref right;

    binary_expression(span source_range, expression_ref left_value, std::string operation,
                      expression_ref right_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct conditional_expression final : expression
  {
    expression_ref condition;
    expression_ref when_true;
    expression_ref when_false;

    conditional_expression(span source_range, expression_ref condition_value, expression_ref true_value,
                           expression_ref false_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct assignment_expression final : expression
  {
    expression_ref target;
    expression_ref value;

    assignment_expression(span source_range, expression_ref target_value, expression_ref assigned_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct call_expression final : expression
  {
    expression_ref callee;
    std::vector<expression_ref> arguments;

    call_expression(span source_range, expression_ref called_value, std::vector<expression_ref> passed_arguments);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct index_expression final : expression
  {
    expression_ref target;
    expression_ref index;

    index_expression(span source_range, expression_ref indexed_value, expression_ref index_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct member_expression final : expression
  {
    expression_ref target;
    std::string member_name;
    bool safe;

    member_expression(span source_range, expression_ref object, std::string name, bool is_safe);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct string_part
  {
    std::string text;
    expression_ref interpolation;

    explicit string_part(std::string segment);
    explicit string_part(expression_ref embedded_expression);
  };

  struct string_expression final : expression
  {
    std::vector<string_part> parts;
    bool raw;
    bool multiline;

    string_expression(span source_range, std::vector<string_part> string_parts, bool is_raw, bool is_multiline);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct spread_expression final : expression
  {
    expression_ref value;

    spread_expression(span source_range, expression_ref spread_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct collection_expression final : expression
  {
    enum class kind
    {
      array,
      vector,
      coordinate,
    };

    kind collection_kind;
    std::vector<expression_ref> elements;

    collection_expression(span source_range, kind type, std::vector<expression_ref> values);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct dictionary_entry
  {
    expression_ref key;
    expression_ref value;

    dictionary_entry(expression_ref entry_key, expression_ref entry_value);
    explicit dictionary_entry(expression_ref spread_value);
  };

  struct dictionary_expression final : expression
  {
    std::vector<dictionary_entry> entries;

    dictionary_expression(span source_range, std::vector<dictionary_entry> values);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct statement : ast_node
  {
    using ast_node::ast_node;
  };

  using statement_ref = std::unique_ptr<statement>;

  struct let_declaration final : statement
  {
    std::string name;
    std::optional<std::string> type_name;
    expression_ref initializer;

    let_declaration(span source_range, std::string identifier, std::optional<std::string> annotation,
                    expression_ref initial_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct program final : ast_node
  {
    std::vector<statement_ref> statements;

    explicit program(std::vector<statement_ref> body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };
}
