#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "../parser/ast_node.hpp"

namespace modules
{
  struct export_symbol
  {
    std::string local_name;
    std::string public_name;
  };

  struct import_edge
  {
    std::string module_name;
    std::string imported_name;
    std::string binding_name;
    bool whole_module;
  };

  struct module_info
  {
    std::string name;
    std::filesystem::path path;
    std::vector<export_symbol> exports;
    std::vector<import_edge> imports;
  };

  struct module_graph
  {
    std::filesystem::path source_root;
    std::filesystem::path entry_path;
    std::vector<module_info> modules;

    auto print(std::ostream &stream) const -> void;
  };

  auto resolve(const std::filesystem::path &entry_path) -> module_graph;
  auto link(const std::filesystem::path &entry_path) -> parser::program;
}
