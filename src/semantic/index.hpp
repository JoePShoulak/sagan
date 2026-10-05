#pragma once

#include "analyzer.hpp"
#include "type_checker.hpp"
#include "../source/source.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace semantic
{
  struct indexed_symbol
  {
    symbol_id id;
    std::string name;
    symbol_kind kind;
    symbol_visibility visibility;
    symbol_origin origin;
    sagan::source::source_range declaration;
    std::size_t scope_id;
    std::vector<std::string> documentation;
    std::string owner_type;
  };

  struct indexed_reference
  {
    symbol_id target;
    reference_kind kind;
    sagan::source::source_range location;
  };

  struct overload_set
  {
    std::string id;
    std::string name;
    std::size_t scope_id;
    std::vector<symbol_id> candidates;
  };

  struct callable_parameters
  {
    symbol_id callable;
    std::vector<std::string> names;
    std::vector<std::string> generic_names;
    std::vector<std::string> types;
    std::vector<std::optional<std::string>> generic_constraints;
    std::string result_type;
  };

  struct specialization_record
  {
    symbol_id generic;
    std::vector<std::string> arguments;
    sagan::source::source_range use;
  };

  struct conformance_record
  {
    symbol_id implementer;
    symbol_id interface;
    sagan::source::source_range declaration;
  };

  struct type_id
  {
    std::string value;
    auto operator==(const type_id &) const -> bool = default;
  };

  struct type_record
  {
    type_id id;
    std::string display;
  };

  struct typed_range
  {
    sagan::source::source_range location;
    type_id type;
    bool declaration{};
  };

  struct member_resolution_record
  {
    sagan::source::source_range use;
    type_id receiver;
    std::string member;
    std::vector<symbol_id> candidates;
  };

  struct inferred_specialization_record
  {
    sagan::source::source_range use;
    std::string generic;
    std::vector<std::string> arguments;
    std::vector<symbol_id> candidates;
  };

  class semantic_index
  {
    sagan::source::document_identity document_;
    sagan::source::document_version version_{};
    std::vector<indexed_symbol> symbols_;
    std::vector<indexed_reference> references_;
    std::vector<overload_set> overloads_;
    std::vector<callable_parameters> callable_parameters_;
    std::vector<specialization_record> specializations_;
    std::vector<conformance_record> conformances_;
    std::vector<type_record> types_;
    std::vector<typed_range> typed_ranges_;
    std::vector<member_resolution_record> member_resolutions_;
    std::vector<inferred_specialization_record> inferred_specializations_;
    std::vector<unresolved_member_reference> unresolved_members_;
    std::unordered_map<std::string, std::size_t> symbol_lookup_;

  public:
    semantic_index(sagan::source::document_identity document, sagan::source::document_version version,
                   const semantic_model &model, const type_model *types = nullptr);

    auto document() const -> const sagan::source::document_identity &;
    auto version() const -> sagan::source::document_version;
    auto symbols() const -> const std::vector<indexed_symbol> &;
    auto references() const -> const std::vector<indexed_reference> &;
    auto overloads() const -> const std::vector<overload_set> &;
    auto parameters() const -> const std::vector<callable_parameters> &;
    auto specializations() const -> const std::vector<specialization_record> &;
    auto conformances() const -> const std::vector<conformance_record> &;
    auto types() const -> const std::vector<type_record> &;
    auto typed_ranges() const -> const std::vector<typed_range> &;
    auto member_resolutions() const -> const std::vector<member_resolution_record> &;
    auto inferred_specializations() const -> const std::vector<inferred_specialization_record> &;
    auto unresolved_members() const -> const std::vector<unresolved_member_reference> &;
    auto find(const symbol_id &id) const -> const indexed_symbol *;
    auto definition(const symbol_id &id) const -> std::optional<sagan::source::source_range>;
    auto references_to(const symbol_id &id, bool include_declaration = false) const
      -> std::vector<sagan::source::source_range>;
    auto symbol_at(sagan::source::byte_offset offset) const -> const indexed_symbol *;
  };

  auto build_index(const sagan::source::document_snapshot &document, const semantic_model &model,
                   const type_model *types = nullptr)
    -> semantic_index;

  auto callable_signature(const semantic_index &index, const symbol_id &id) -> std::string;
}
