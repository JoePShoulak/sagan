#pragma once

#include "../parser/ast_node.hpp"

#include <ostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace semantic
{
  struct symbol_id
  {
    std::string value;

    auto operator==(const symbol_id &) const -> bool = default;
  };

  enum class symbol_kind
  {
    module,
    imported_namespace,
    variable,
    constant,
    parameter,
    loop_binding,
    match_binding,
    function,
    constructor,
    type,
    type_parameter,
    dimension,
    quantity,
    unit,
    field,
    constant_field,
    method,
    enum_case,
    enum_constructor,
    self_value,
    builtin_type,
    builtin_value,
  };

  enum class symbol_visibility { public_access, private_access };
  enum class symbol_origin { source, imported, builtin, generated };
  enum class reference_kind { unclassified, read, write, type, call, import, conformance, export_reference };

  auto name(symbol_kind value) -> std::string_view;
  // Naming gate used before a future symbol-identity rename edit is offered.
  // Other identifier and conflict checks remain the rename operation's job.
  auto rename_preserves_binding_convention(symbol_kind kind, std::string_view proposed) -> bool;

  struct symbol
  {
    symbol_id id;
    std::string name;
    symbol_kind kind;
    symbol_visibility visibility;
    symbol_origin origin;
    parser::span declaration;
    std::size_t scope_id;
    std::vector<std::string> documentation;
  };

  struct resolution
  {
    std::string name;
    parser::span use;
    parser::span declaration;
    symbol_id target;
    reference_kind kind;
  };

  struct generic_specialization
  {
    symbol_id generic;
    std::vector<std::string> arguments;
    parser::span use;
  };

  struct callable_signature
  {
    symbol_id callable;
    std::vector<std::string> parameter_names;
    std::vector<std::string> parameter_types;
    std::vector<std::string> generic_names;
    std::vector<std::optional<std::string>> generic_constraints;
    std::string result_type;
  };

  struct unresolved_member_reference
  {
    symbol_id receiver;
    std::string member;
    parser::span use;
    reference_kind kind;
  };

  struct scope
  {
    std::size_t id;
    std::size_t parent;
    std::string label;
    parser::span range;
    std::vector<symbol> symbols;
  };

  struct semantic_model
  {
    std::vector<scope> scopes;
    std::vector<resolution> resolutions;
    std::vector<generic_specialization> specializations;
    std::vector<callable_signature> callable_signatures;
    std::vector<unresolved_member_reference> unresolved_members;

    auto print(std::ostream &stream) const -> void;
  };

  struct analysis_identity
  {
    std::string package{"local"};
    std::string module{"main"};
  };

  auto analyze(const parser::program &tree, analysis_identity identity = {}) -> semantic_model;
  auto analyze_partial(const parser::program &tree, analysis_identity identity = {}) -> semantic_model;
}
