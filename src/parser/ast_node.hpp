#pragma once

#include "span.hpp"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace parser
{
  struct function_parameter
  {
    std::string name;
    std::optional<std::string> type_name;

    function_parameter(std::string identifier, std::optional<std::string> annotation);
  };

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

  struct lambda_expression final : expression
  {
    std::vector<function_parameter> parameters;
    std::optional<std::string> return_type;
    expression_ref body;

    lambda_expression(span source_range, std::vector<function_parameter> declared_parameters,
                      std::optional<std::string> result_type, expression_ref expression_body);
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

  struct expression_statement final : statement
  {
    expression_ref value;

    expression_statement(span source_range, expression_ref statement_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct assignment_statement final : statement
  {
    expression_ref target;
    expression_ref value;

    assignment_statement(span source_range, expression_ref target_value, expression_ref assigned_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct block_statement final : statement
  {
    std::vector<statement_ref> statements;

    block_statement(span source_range, std::vector<statement_ref> body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct if_statement final : statement
  {
    expression_ref condition;
    std::unique_ptr<block_statement> then_branch;
    statement_ref else_branch;

    if_statement(span source_range, expression_ref condition_value,
                 std::unique_ptr<block_statement> true_branch, statement_ref false_branch);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct condition_loop_statement final : statement
  {
    enum class kind
    {
      while_loop,
      until_loop,
    };

    kind loop_kind;
    expression_ref condition;
    std::unique_ptr<block_statement> body;

    condition_loop_statement(span source_range, kind type, expression_ref condition_value,
                             std::unique_ptr<block_statement> loop_body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct for_statement final : statement
  {
    std::string binding;
    expression_ref iterable;
    std::unique_ptr<block_statement> body;

    for_statement(span source_range, std::string binding_name, expression_ref iterable_value,
                  std::unique_ptr<block_statement> loop_body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct loop_control_statement final : statement
  {
    enum class kind
    {
      break_loop,
      continue_loop,
    };

    kind control_kind;

    loop_control_statement(span source_range, kind type);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct return_statement final : statement
  {
    expression_ref value;

    return_statement(span source_range, expression_ref returned_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct match_case
  {
    expression_ref pattern;
    std::unique_ptr<block_statement> body;
    span range;

    match_case(expression_ref matched_pattern, std::unique_ptr<block_statement> case_body,
               span source_range);
  };

  struct match_statement final : statement
  {
    expression_ref subject;
    std::vector<match_case> cases;

    match_statement(span source_range, expression_ref matched_subject,
                    std::vector<match_case> branches);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct exception_handler
  {
    expression_ref pattern;
    std::unique_ptr<block_statement> body;
    span range;

    exception_handler(expression_ref matched_pattern, std::unique_ptr<block_statement> handler_body,
                      span source_range);
  };

  struct hope_statement final : statement
  {
    std::unique_ptr<block_statement> protected_body;
    std::vector<exception_handler> handlers;
    std::unique_ptr<block_statement> cleanup;

    hope_statement(span source_range, std::unique_ptr<block_statement> body,
                   std::vector<exception_handler> exception_handlers,
                   std::unique_ptr<block_statement> cleanup_body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct scream_statement final : statement
  {
    expression_ref value;

    scream_statement(span source_range, expression_ref exception_value);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct function_declaration final : statement
  {
    std::string name;
    bool private_member;
    std::vector<function_parameter> parameters;
    std::optional<std::string> return_type;
    std::unique_ptr<block_statement> body;
    expression_ref expression_body;

    function_declaration(span source_range, std::string identifier,
                         bool is_private,
                         std::vector<function_parameter> declared_parameters,
                         std::optional<std::string> result_type,
                         std::unique_ptr<block_statement> function_body,
                         expression_ref function_expression_body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct type_declaration final : statement
  {
    enum class kind
    {
      interface_type,
      class_type,
      enum_type,
    };

    kind type_kind;
    std::string name;
    std::optional<std::string> composition_keyword;
    std::vector<std::string> composed_interfaces;
    std::vector<statement_ref> members;
    std::vector<std::string> enum_members;

    type_declaration(span source_range, kind declared_kind, std::string identifier,
                     std::optional<std::string> composition,
                     std::vector<std::string> interfaces,
                     std::vector<statement_ref> declared_members,
                     std::vector<std::string> declared_enum_members);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct module_declaration final : statement
  {
    std::string name;

    module_declaration(span source_range, std::string module_name);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct import_declaration final : statement
  {
    std::string imported_name;
    std::optional<std::string> source_module;
    std::optional<std::string> alias;

    import_declaration(span source_range, std::string name,
                       std::optional<std::string> source,
                       std::optional<std::string> imported_alias);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct export_declaration final : statement
  {
    std::string exported_name;
    std::optional<std::string> alias;

    export_declaration(span source_range, std::string name,
                       std::optional<std::string> exported_alias);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };

  struct program final : ast_node
  {
    std::vector<statement_ref> statements;

    explicit program(std::vector<statement_ref> body);
    auto print(std::ostream &stream, int indent = 0) const -> void override;
  };
}
