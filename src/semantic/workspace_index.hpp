#pragma once

#include "index.hpp"
#include "../modules/resolver.hpp"
#include "../source/provider.hpp"

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

namespace semantic
{
  struct indexed_module
  {
    std::string name;
    semantic_index index;
    bool external{};
  };

  struct import_link
  {
    symbol_id binding;
    std::string source_module;
    std::string imported_name;
    std::string binding_name;
    std::vector<symbol_id> targets;
    bool whole_module{};
    sagan::source::source_range declaration;
  };

  struct external_reference
  {
    symbol_id target;
    sagan::source::source_range location;
    reference_kind kind;
  };

  struct exported_symbol
  {
    std::string module;
    std::string local_name;
    std::string public_name;
    std::vector<symbol_id> targets;
    sagan::source::source_range declaration;
  };

  class workspace_semantic_index
  {
    std::vector<indexed_module> modules_;
    std::vector<import_link> imports_;
    std::unordered_map<std::string, std::vector<symbol_id>> exports_;
    std::vector<exported_symbol> exported_symbols_;
    std::vector<external_reference> external_references_;
    std::unordered_map<std::string, std::string> binding_types_;

  public:
    workspace_semantic_index(std::vector<indexed_module> modules, std::vector<import_link> imports,
                             std::unordered_map<std::string, std::vector<symbol_id>> exports,
                             std::vector<exported_symbol> exported_symbols,
                             std::vector<external_reference> external_references,
                             std::unordered_map<std::string, std::string> binding_types);

    auto modules() const -> const std::vector<indexed_module> &;
    auto imports() const -> const std::vector<import_link> &;
    auto external_references() const -> const std::vector<external_reference> &;
    auto declared_type(const symbol_id &binding) const -> std::optional<std::string>;
    auto find(const symbol_id &id) const -> const indexed_symbol *;
    auto definitions(const symbol_id &id) const -> std::vector<sagan::source::source_range>;
    auto references_to(const symbol_id &id, bool include_declaration = false) const
      -> std::vector<sagan::source::source_range>;
    auto exported(const std::string &module, const std::string &public_name) const
      -> std::vector<symbol_id>;
    auto exported_symbols() const -> std::vector<exported_symbol>;
  };

  auto build_workspace_index(const modules::module_graph &graph,
                             const sagan::source::source_provider &source)
    -> workspace_semantic_index;
}
