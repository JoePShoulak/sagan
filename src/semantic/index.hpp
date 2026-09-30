#pragma once

#include "analyzer.hpp"
#include "../source/source.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace semantic
{
  enum class reference_kind { unclassified, read, write, type, call, import, conformance, export_reference };

  struct indexed_symbol
  {
    symbol_id id;
    std::string name;
    symbol_kind kind;
    symbol_visibility visibility;
    symbol_origin origin;
    sagan::source::source_range declaration;
    std::size_t scope_id;
  };

  struct indexed_reference
  {
    symbol_id target;
    reference_kind kind;
    sagan::source::source_range location;
  };

  class semantic_index
  {
    sagan::source::document_identity document_;
    sagan::source::document_version version_{};
    std::vector<indexed_symbol> symbols_;
    std::vector<indexed_reference> references_;
    std::unordered_map<std::string, std::size_t> symbol_lookup_;

  public:
    semantic_index(sagan::source::document_identity document, sagan::source::document_version version,
                   const semantic_model &model);

    auto document() const -> const sagan::source::document_identity &;
    auto version() const -> sagan::source::document_version;
    auto symbols() const -> const std::vector<indexed_symbol> &;
    auto references() const -> const std::vector<indexed_reference> &;
    auto find(const symbol_id &id) const -> const indexed_symbol *;
    auto definition(const symbol_id &id) const -> std::optional<sagan::source::source_range>;
    auto references_to(const symbol_id &id, bool include_declaration = false) const
      -> std::vector<sagan::source::source_range>;
    auto symbol_at(sagan::source::byte_offset offset) const -> const indexed_symbol *;
  };

  auto build_index(const sagan::source::document_snapshot &document, const semantic_model &model)
    -> semantic_index;
}
