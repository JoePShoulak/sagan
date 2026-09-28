#pragma once

#include "../parser/ast_node.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace semantic
{
  struct symbol
  {
    std::string name;
    std::string kind;
    parser::span declaration;
  };

  struct resolution
  {
    std::string name;
    parser::span use;
    parser::span declaration;
  };

  struct scope
  {
    std::size_t id;
    std::size_t parent;
    std::string label;
    std::vector<symbol> symbols;
  };

  struct semantic_model
  {
    std::vector<scope> scopes;
    std::vector<resolution> resolutions;

    auto print(std::ostream &stream) const -> void;
  };

  auto analyze(const parser::program &tree) -> semantic_model;
}
