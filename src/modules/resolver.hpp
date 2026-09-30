#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../parser/ast_node.hpp"

namespace modules
{
  enum class application_mode
  {
    console,
    windowed,
  };

  struct package_manifest
  {
    std::string name;
    std::string version;
    std::filesystem::path manifest_path;
    std::filesystem::path package_root;
    std::filesystem::path source_root;
    std::string entry_module;
    application_mode mode{application_mode::console};
  };

  auto application_mode_name(application_mode mode) -> std::string_view;

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
    std::optional<package_manifest> package;
    std::vector<module_info> modules;

    auto print(std::ostream &stream) const -> void;
  };

  auto resolve(const std::filesystem::path &entry_path) -> module_graph;
  auto link(const std::filesystem::path &entry_path) -> parser::program;
  auto load_package(const std::filesystem::path &package_path) -> package_manifest;
  auto discover_package(const std::filesystem::path &entry_path) -> std::optional<package_manifest>;
  auto resolve_package(const std::filesystem::path &package_path) -> module_graph;
  auto link_package(const std::filesystem::path &package_path) -> parser::program;
}
