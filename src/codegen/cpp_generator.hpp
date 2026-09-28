#pragma once

#include "../parser/ast_node.hpp"
#include "../semantic/type_checker.hpp"

#include <string>

namespace codegen
{
  auto generate_cpp(const parser::program &tree, const semantic::type_model &types) -> std::string;
}
