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
