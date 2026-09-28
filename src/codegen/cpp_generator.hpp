#pragma once

#include "../parser/ast_node.hpp"

#include <string>

namespace codegen
{
  auto generate_cpp(const parser::program &tree) -> std::string;
}
