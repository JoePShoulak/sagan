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

  struct type_model
  {
    std::vector<typed_declaration> declarations;
    std::vector<typed_expression> expressions;

    auto print(std::ostream &stream) const -> void;
  };

  auto check_types(const parser::program &tree) -> type_model;
}
