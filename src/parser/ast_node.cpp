#include "ast_node.hpp"

#include <utility>

namespace
{
  auto write_indent(std::ostream &stream, const int indent) -> void
  {
    stream << std::string(static_cast<std::size_t>(indent), ' ');
  }

  auto escaped_string_text(const std::string &text) -> std::string
  {
    std::string result;
    for (const char character : text)
    {
      switch (character)
      {
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      default: result.push_back(character); break;
      }
    }
    return result;
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

  call_expression::call_expression(const span source_range, expression_ref called_value,
                                   std::vector<expression_ref> passed_arguments)
      : expression(source_range), callee(std::move(called_value)), arguments(std::move(passed_arguments))
  {
  }

  auto call_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Call\n";
    callee->print(stream, indent + 2);
    for (const auto &argument : arguments)
    {
      argument->print(stream, indent + 2);
    }
  }

  index_expression::index_expression(const span source_range, expression_ref indexed_value,
                                     expression_ref index_value)
      : expression(source_range), target(std::move(indexed_value)), index(std::move(index_value))
  {
  }

  auto index_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Index\n";
    target->print(stream, indent + 2);
    index->print(stream, indent + 2);
  }

  member_expression::member_expression(const span source_range, expression_ref object, std::string name,
                                       const bool is_safe)
      : expression(source_range), target(std::move(object)), member_name(std::move(name)), safe(is_safe)
  {
  }

  auto member_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << (safe ? "SafeMember(" : "Member(") << member_name << ")\n";
    target->print(stream, indent + 2);
  }

  string_part::string_part(std::string segment) : text(std::move(segment)) {}

  string_part::string_part(expression_ref embedded_expression)
      : interpolation(std::move(embedded_expression))
  {
  }

  string_expression::string_expression(const span source_range, std::vector<string_part> string_parts,
                                       const bool is_raw, const bool is_multiline)
      : expression(source_range), parts(std::move(string_parts)), raw(is_raw), multiline(is_multiline)
  {
  }

  auto string_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << (raw ? "RawString" : (multiline ? "MultilineString" : "String")) << "\n";
    for (const auto &part : parts)
    {
      if (part.interpolation)
      {
        write_indent(stream, indent + 2);
        stream << "Interpolation\n";
        part.interpolation->print(stream, indent + 4);
      }
      else
      {
        write_indent(stream, indent + 2);
        stream << "Text(\"" << escaped_string_text(part.text) << "\")\n";
      }
    }
  }

  spread_expression::spread_expression(const span source_range, expression_ref spread_value)
      : expression(source_range), value(std::move(spread_value))
  {
  }

  auto spread_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Spread\n";
    value->print(stream, indent + 2);
  }

  collection_expression::collection_expression(const span source_range, const kind type,
                                               std::vector<expression_ref> values)
      : expression(source_range), collection_kind(type), elements(std::move(values))
  {
  }

  auto collection_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    const char *name = collection_kind == kind::array        ? "Array"
                       : collection_kind == kind::vector     ? "Vector"
                                                             : "Coordinate";
    stream << name << '\n';
    for (const auto &element : elements)
    {
      element->print(stream, indent + 2);
    }
  }

  dictionary_entry::dictionary_entry(expression_ref entry_key, expression_ref entry_value)
      : key(std::move(entry_key)), value(std::move(entry_value))
  {
  }

  dictionary_entry::dictionary_entry(expression_ref spread_value) : value(std::move(spread_value)) {}

  dictionary_expression::dictionary_expression(const span source_range, std::vector<dictionary_entry> values)
      : expression(source_range), entries(std::move(values))
  {
  }

  auto dictionary_expression::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Dictionary\n";
    for (const auto &entry : entries)
    {
      if (!entry.key)
      {
        entry.value->print(stream, indent + 2);
        continue;
      }
      write_indent(stream, indent + 2);
      stream << "Entry\n";
      entry.key->print(stream, indent + 4);
      entry.value->print(stream, indent + 4);
    }
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

  expression_statement::expression_statement(const span source_range, expression_ref statement_value)
      : statement(source_range), value(std::move(statement_value))
  {
  }

  auto expression_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "ExpressionStatement\n";
    value->print(stream, indent + 2);
  }

  assignment_statement::assignment_statement(const span source_range, expression_ref target_value,
                                               expression_ref assigned_value)
      : statement(source_range), target(std::move(target_value)), value(std::move(assigned_value))
  {
  }

  auto assignment_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Assignment(=)\n";
    target->print(stream, indent + 2);
    value->print(stream, indent + 2);
  }

  block_statement::block_statement(const span source_range, std::vector<statement_ref> body)
      : statement(source_range), statements(std::move(body))
  {
  }

  auto block_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Block\n";
    for (const auto &entry : statements)
    {
      entry->print(stream, indent + 2);
    }
  }

  if_statement::if_statement(const span source_range, expression_ref condition_value,
                             std::unique_ptr<block_statement> true_branch, statement_ref false_branch)
      : statement(source_range), condition(std::move(condition_value)), then_branch(std::move(true_branch)),
        else_branch(std::move(false_branch))
  {
  }

  auto if_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "If\n";
    write_indent(stream, indent + 2);
    stream << "Condition\n";
    condition->print(stream, indent + 4);
    write_indent(stream, indent + 2);
    stream << "Then\n";
    then_branch->print(stream, indent + 4);
    if (else_branch)
    {
      write_indent(stream, indent + 2);
      stream << "Else\n";
      else_branch->print(stream, indent + 4);
    }
  }

  condition_loop_statement::condition_loop_statement(const span source_range, const kind type,
                                                     expression_ref condition_value,
                                                     std::unique_ptr<block_statement> loop_body)
      : statement(source_range), loop_kind(type), condition(std::move(condition_value)), body(std::move(loop_body))
  {
  }

  auto condition_loop_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << (loop_kind == kind::while_loop ? "While\n" : "Until\n");
    write_indent(stream, indent + 2);
    stream << "Condition\n";
    condition->print(stream, indent + 4);
    body->print(stream, indent + 2);
  }

  for_statement::for_statement(const span source_range, std::string binding_name,
                               expression_ref iterable_value, std::unique_ptr<block_statement> loop_body)
      : statement(source_range), binding(std::move(binding_name)), iterable(std::move(iterable_value)),
        body(std::move(loop_body))
  {
  }

  auto for_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "For(" << binding << ")\n";
    write_indent(stream, indent + 2);
    stream << "Iterable\n";
    iterable->print(stream, indent + 4);
    body->print(stream, indent + 2);
  }

  loop_control_statement::loop_control_statement(const span source_range, const kind type)
      : statement(source_range), control_kind(type)
  {
  }

  auto loop_control_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << (control_kind == kind::break_loop ? "Break\n" : "Continue\n");
  }

  return_statement::return_statement(const span source_range, expression_ref returned_value)
      : statement(source_range), value(std::move(returned_value))
  {
  }

  auto return_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Return\n";
    if (value)
    {
      value->print(stream, indent + 2);
    }
  }

  match_case::match_case(expression_ref matched_pattern, std::unique_ptr<block_statement> case_body,
                         const span source_range)
      : pattern(std::move(matched_pattern)), body(std::move(case_body)), range(source_range)
  {
  }

  match_statement::match_statement(const span source_range, expression_ref matched_subject,
                                   std::vector<match_case> branches)
      : statement(source_range), subject(std::move(matched_subject)), cases(std::move(branches))
  {
  }

  auto match_statement::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Match\n";
    write_indent(stream, indent + 2);
    stream << "Subject\n";
    subject->print(stream, indent + 4);
    for (const auto &branch : cases)
    {
      write_indent(stream, indent + 2);
      stream << (branch.pattern ? "Case\n" : "CaseElse\n");
      if (branch.pattern)
      {
        branch.pattern->print(stream, indent + 4);
      }
      branch.body->print(stream, indent + 4);
    }
  }

  function_parameter::function_parameter(std::string identifier, std::optional<std::string> annotation)
      : name(std::move(identifier)), type_name(std::move(annotation))
  {
  }

  function_declaration::function_declaration(const span source_range, std::string identifier,
                                             std::vector<function_parameter> declared_parameters,
                                             std::optional<std::string> result_type,
                                             std::unique_ptr<block_statement> function_body)
      : statement(source_range), name(std::move(identifier)), parameters(std::move(declared_parameters)),
        return_type(std::move(result_type)), body(std::move(function_body))
  {
  }

  auto function_declaration::print(std::ostream &stream, const int indent) const -> void
  {
    write_indent(stream, indent);
    stream << "Function(" << name;
    if (return_type)
    {
      stream << ": " << *return_type;
    }
    stream << ")\n";
    for (const auto &parameter : parameters)
    {
      write_indent(stream, indent + 2);
      stream << "Parameter(" << parameter.name;
      if (parameter.type_name)
      {
        stream << ": " << *parameter.type_name;
      }
      stream << ")\n";
    }
    body->print(stream, indent + 2);
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
