#include "index.hpp"

#include <algorithm>
#include <utility>

namespace semantic
{
  namespace
  {
    auto source_range(const sagan::source::document_id document, const parser::span range)
      -> sagan::source::source_range
    {
      const auto begin = static_cast<sagan::source::byte_offset>(std::max(range.begin, 0));
      const auto end = static_cast<sagan::source::byte_offset>(std::max(range.end, range.begin));
      return {document, {begin, end}};
    }

    auto contains(const sagan::source::byte_range range, const sagan::source::byte_offset offset) -> bool
    {
      return range.begin <= offset && offset < range.end;
    }
  }

  semantic_index::semantic_index(sagan::source::document_identity document,
                                 const sagan::source::document_version version,
                                 const semantic_model &model)
      : document_(std::move(document)), version_(version)
  {
    for (const auto &scope : model.scopes)
      for (const auto &entry : scope.symbols)
      {
        symbol_lookup_.insert_or_assign(entry.id.value, symbols_.size());
        symbols_.push_back(indexed_symbol{entry.id, entry.name, entry.kind, entry.visibility, entry.origin,
                                          source_range(document_.id, entry.declaration), entry.scope_id});
      }
    for (const auto &entry : model.resolutions)
      references_.push_back(indexed_reference{entry.target, reference_kind::unclassified,
                                               source_range(document_.id, entry.use)});
  }

  auto semantic_index::document() const -> const sagan::source::document_identity & { return document_; }
  auto semantic_index::version() const -> sagan::source::document_version { return version_; }
  auto semantic_index::symbols() const -> const std::vector<indexed_symbol> & { return symbols_; }
  auto semantic_index::references() const -> const std::vector<indexed_reference> & { return references_; }

  auto semantic_index::find(const symbol_id &id) const -> const indexed_symbol *
  {
    const auto found = symbol_lookup_.find(id.value);
    return found == symbol_lookup_.end() ? nullptr : &symbols_[found->second];
  }

  auto semantic_index::definition(const symbol_id &id) const -> std::optional<sagan::source::source_range>
  {
    if (const auto *entry = find(id)) return entry->declaration;
    return {};
  }

  auto semantic_index::references_to(const symbol_id &id, const bool include_declaration) const
    -> std::vector<sagan::source::source_range>
  {
    std::vector<sagan::source::source_range> result;
    if (include_declaration)
      if (const auto declaration = definition(id)) result.push_back(*declaration);
    for (const auto &entry : references_)
      if (entry.target == id) result.push_back(entry.location);
    return result;
  }

  auto semantic_index::symbol_at(const sagan::source::byte_offset offset) const -> const indexed_symbol *
  {
    for (const auto &entry : references_)
      if (contains(entry.location.bytes, offset)) return find(entry.target);
    const indexed_symbol *best = nullptr;
    for (const auto &entry : symbols_)
      if (contains(entry.declaration.bytes, offset) &&
          (!best || entry.declaration.bytes.end - entry.declaration.bytes.begin <
                        best->declaration.bytes.end - best->declaration.bytes.begin)) best = &entry;
    return best;
  }

  auto build_index(const sagan::source::document_snapshot &document, const semantic_model &model)
    -> semantic_index
  {
    return semantic_index(document.identity(), document.version(), model);
  }
}
