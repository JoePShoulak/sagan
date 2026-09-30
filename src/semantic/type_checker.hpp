#pragma once

#include "../parser/ast_node.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace semantic
{
  struct typed_expression
  {
    parser::span range;
    std::string type;
  };

  struct typed_declaration
  {
    parser::span range;
    std::string name;
    std::string type;
  };

  struct resolved_member
  {
    parser::span use;
    std::string receiver_type;
    std::string member;
  };

  struct inferred_specialization
  {
    parser::span use;
    std::string generic;
    std::vector<std::string> arguments;
  };

  struct resolved_call
  {
    parser::span range;
    std::vector<std::string> parameter_types;
    std::string result_type;
  };

  struct type_model
  {
    std::vector<typed_declaration> declarations;
    std::vector<typed_expression> expressions;
    std::vector<resolved_member> members;
    std::vector<inferred_specialization> inferred_specializations;
    std::vector<resolved_call> calls;

    auto print(std::ostream &stream) const -> void;
  };

  auto check_types(const parser::program &tree) -> type_model;
  auto validate_entry_point(const parser::program &tree) -> void;
}
