#include "ast_node.hpp"

#include <utility>

namespace
{
  auto write_indent(std::ostream &stream, const int indent) -> void
  {
    stream << std::string(static_cast<std::size_t>(indent), ' ');
  }
}

namespace parser
{
  identifier_expression::identifier_expression(const span source_range, std::string identifier)
      : expression(source_range), name(std::move(identifier))
  {
  }

  auto identifier_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Identifier(" << name << ")\n";
  }

  literal_expression::literal_expression(const span source_range, const kind type, std::string source_spelling,
                                         const double value)
      : expression(source_range), literal_kind(type), spelling(std::move(source_spelling)), numeric_value(value)
  {
  }

  auto literal_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    const char *name = literal_kind == kind::integer          ? "Integer("
                       : literal_kind == kind::floating_point ? "Float("
                                                              : "Bool(";
    stream << name << spelling << ")\n";
  }

  grouping_expression::grouping_expression(const span source_range, expression_ref grouped_value)
      : expression(source_range), value(std::move(grouped_value))
  {
  }

  auto grouping_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Group\n";
    value->print(stream, indent + 2);
  }

  unary_expression::unary_expression(const span source_range, std::string operation, expression_ref value,
                                     const bool is_postfix)
      : expression(source_range), operator_text(std::move(operation)), operand(std::move(value)),
        postfix(is_postfix)
  {
  }

  auto unary_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << (postfix ? "Postfix(" : "Prefix(") << operator_text << ")\n";
    operand->print(stream, indent + 2);
  }

  binary_expression::binary_expression(const span source_range, expression_ref left_value, std::string operation,
                                       expression_ref right_value)
      : expression(source_range), left(std::move(left_value)), operator_text(std::move(operation)),
        right(std::move(right_value))
  {
  }

  auto binary_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Binary(" << operator_text << ")\n";
    left->print(stream, indent + 2);
    right->print(stream, indent + 2);
  }

  conditional_expression::conditional_expression(const span source_range, expression_ref condition_value,
                                                 expression_ref true_value, expression_ref false_value)
      : expression(source_range), condition(std::move(condition_value)), when_true(std::move(true_value)),
        when_false(std::move(false_value))
  {
  }

  auto conditional_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Conditional\n";
    condition->print(stream, indent + 2);
    when_true->print(stream, indent + 2);
    when_false->print(stream, indent + 2);
  }

  assignment_expression::assignment_expression(const span source_range, expression_ref target_value,
                                               expression_ref assigned_value)
      : expression(source_range), target(std::move(target_value)), value(std::move(assigned_value))
  {
  }

  auto assignment_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "AssignValue(:=)\n";
    target->print(stream, indent + 2);
    value->print(stream, indent + 2);
  }

  let_declaration::let_declaration(const span source_range, std::string identifier,
                                   std::optional<std::string> annotation, expression_ref initial_value)
      : statement(source_range), name(std::move(identifier)), type_name(std::move(annotation)),
        initializer(std::move(initial_value))
  {
  }

  auto let_declaration::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Let(" << name;
    if (type_name)
    {
      stream << ": " << *type_name;
    }
    stream << ")\n";
    if (initializer)
    {
      initializer->print(stream, indent + 2);
    }
  }

  program::program(std::vector<statement_ref> body)
      : ast_node(body.empty() ? span{0, 0} : span{body.front()->range.begin, body.back()->range.end}),
        statements(std::move(body))
  {
  }

  auto program::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Program\n";
    for (const auto &entry : statements)
    {
      entry->print(stream, indent + 2);
    }
  }
}
