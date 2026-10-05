#include "index.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace semantic
{
  auto callable_signature(const semantic_index &index, const symbol_id &id) -> std::string
  {
    for (const auto &call : index.parameters())
    {
      if (call.callable != id) continue;
      std::string text{"("};
      for (std::size_t i = 0; i < call.types.size(); ++i)
      {
        if (i) text += ", ";
        if (i < call.names.size()) text += call.names[i] + ": ";
        text += call.types[i];
      }
      text += ")";
      if (!call.result_type.empty()) text += ": " + call.result_type;
      return text;
    }
    return {};
  }
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

    auto stable_type_id(const std::string &type) -> type_id
    {
      std::uint64_t hash = 14695981039346656037ULL;
      for (const unsigned char byte : std::string("sagan-type-v1|") + type)
      {
        hash ^= byte;
        hash *= 1099511628211ULL;
      }
      std::ostringstream encoded;
      encoded << "sagan-type-v1:" << std::hex << std::setfill('0') << std::setw(16) << hash;
      return {encoded.str()};
    }
  }

  semantic_index::semantic_index(sagan::source::document_identity document,
                                 const sagan::source::document_version version,
                                 const semantic_model &model, const type_model *types)
      : document_(std::move(document)), version_(version)
  {
    for (const auto &scope : model.scopes)
      for (const auto &entry : scope.symbols)
      {
        symbol_lookup_.insert_or_assign(entry.id.value, symbols_.size());
        std::string owner_type;
        if (entry.scope_id < model.scopes.size() && model.scopes[entry.scope_id].label.starts_with("type "))
          owner_type = model.scopes[entry.scope_id].label.substr(5);
        symbols_.push_back(indexed_symbol{entry.id, entry.name, entry.kind, entry.visibility, entry.origin,
                                          source_range(document_.id, entry.declaration), entry.scope_id,
                                          entry.documentation, std::move(owner_type)});
      }
    for (const auto &entry : model.resolutions)
      references_.push_back(indexed_reference{entry.target, entry.kind,
                                               source_range(document_.id, entry.use)});
    for (const auto &entry : model.specializations)
      specializations_.push_back(specialization_record{entry.generic, entry.arguments,
                                                        source_range(document_.id, entry.use)});
    unresolved_members_ = model.unresolved_members;
    for (const auto &scope : model.scopes)
    {
      std::unordered_map<std::string, std::vector<symbol_id>> callable_groups;
      for (const auto &entry : scope.symbols)
        if (entry.kind == symbol_kind::function || entry.kind == symbol_kind::method ||
            entry.kind == symbol_kind::constructor || entry.kind == symbol_kind::enum_constructor)
          callable_groups[entry.name].push_back(entry.id);
      for (auto &[name, candidates] : callable_groups)
        if (candidates.size() > 1)
          overloads_.push_back(overload_set{"sagan-overload-v1:" + candidates.front().value, name,
                                             scope.id, std::move(candidates)});
    }
    for (const auto &symbol : symbols_)
      if (symbol.origin == symbol_origin::source &&
          (symbol.kind == symbol_kind::function || symbol.kind == symbol_kind::method ||
           symbol.kind == symbol_kind::constructor))
        for (const auto &scope : model.scopes)
          if (scope.parent == symbol.scope_id && scope.label == "function " + symbol.name &&
              scope.range.begin == static_cast<int>(symbol.declaration.bytes.begin) &&
              scope.range.end == static_cast<int>(symbol.declaration.bytes.end))
          {
            callable_parameters parameters{symbol.id, {}, {}, {}, {}, {}};
            for (const auto &entry : scope.symbols)
            {
              if (entry.kind == symbol_kind::parameter) parameters.names.push_back(entry.name);
              else if (entry.kind == symbol_kind::type_parameter)
                parameters.generic_names.push_back(entry.name);
            }
            callable_parameters_.push_back(std::move(parameters));
            break;
          }
    for (const auto &signature : model.callable_signatures)
      for (auto &record : callable_parameters_)
        if (record.callable == signature.callable)
        {
          record.names = signature.parameter_names;
          record.generic_names = signature.generic_names;
          record.types = signature.parameter_types;
          record.generic_constraints = signature.generic_constraints;
          record.result_type = signature.result_type;
          break;
        }
    for (const auto &reference : references_)
    {
      if (reference.kind != reference_kind::conformance) continue;
      const indexed_symbol *implementer = nullptr;
      for (const auto &candidate : symbols_)
      {
        if (candidate.kind != symbol_kind::type || candidate.origin != symbol_origin::source) continue;
        if (candidate.declaration.bytes.begin <= reference.location.bytes.begin &&
            candidate.declaration.bytes.end >= reference.location.bytes.end &&
            (!implementer || candidate.declaration.bytes.end - candidate.declaration.bytes.begin <
                                implementer->declaration.bytes.end - implementer->declaration.bytes.begin))
          implementer = &candidate;
      }
      if (implementer)
        conformances_.push_back(conformance_record{implementer->id, reference.target,
                                                    implementer->declaration});
    }
    if (types)
    {
      std::unordered_map<std::string, type_id> known;
      const auto record_type = [&](const std::string &display) -> type_id
      {
        if (const auto found = known.find(display); found != known.end()) return found->second;
        auto id = stable_type_id(display);
        known.emplace(display, id);
        types_.push_back(type_record{id, display});
        return id;
      };
      for (const auto &entry : types->declarations)
        typed_ranges_.push_back(typed_range{source_range(document_.id, entry.range), record_type(entry.type), true});
      for (const auto &entry : types->expressions)
        typed_ranges_.push_back(typed_range{source_range(document_.id, entry.range), record_type(entry.type), false});
      for (const auto &entry : types->members)
      {
        std::vector<symbol_id> candidates;
        std::vector<std::size_t> type_scopes;
        for (const auto &scope : model.scopes)
          if (scope.label == "type " + entry.receiver_type) type_scopes.push_back(scope.id);
        for (const auto &symbol : symbols_)
          if (symbol.name == entry.member &&
              std::find(type_scopes.begin(), type_scopes.end(), symbol.scope_id) != type_scopes.end())
            candidates.push_back(symbol.id);
        member_resolutions_.push_back(member_resolution_record{
            source_range(document_.id, entry.use), record_type(entry.receiver_type), entry.member,
            std::move(candidates)});
      }
      for (const auto &entry : types->inferred_specializations)
      {
        std::vector<symbol_id> candidates;
        for (const auto &symbol : symbols_)
          if (symbol.name == entry.generic &&
              (symbol.kind == symbol_kind::function || symbol.kind == symbol_kind::method ||
               symbol.kind == symbol_kind::constructor)) candidates.push_back(symbol.id);
        inferred_specializations_.push_back(inferred_specialization_record{
            source_range(document_.id, entry.use), entry.generic, entry.arguments, std::move(candidates)});
      }
    }
  }

  auto semantic_index::document() const -> const sagan::source::document_identity & { return document_; }
  auto semantic_index::version() const -> sagan::source::document_version { return version_; }
  auto semantic_index::symbols() const -> const std::vector<indexed_symbol> & { return symbols_; }
  auto semantic_index::references() const -> const std::vector<indexed_reference> & { return references_; }
  auto semantic_index::overloads() const -> const std::vector<overload_set> & { return overloads_; }
  auto semantic_index::parameters() const -> const std::vector<callable_parameters> &
  {
    return callable_parameters_;
  }
  auto semantic_index::specializations() const -> const std::vector<specialization_record> &
  {
    return specializations_;
  }
  auto semantic_index::conformances() const -> const std::vector<conformance_record> &
  {
    return conformances_;
  }
  auto semantic_index::types() const -> const std::vector<type_record> & { return types_; }
  auto semantic_index::typed_ranges() const -> const std::vector<typed_range> & { return typed_ranges_; }
  auto semantic_index::member_resolutions() const -> const std::vector<member_resolution_record> &
  {
    return member_resolutions_;
  }
  auto semantic_index::inferred_specializations() const
    -> const std::vector<inferred_specialization_record> &
  {
    return inferred_specializations_;
  }
  auto semantic_index::unresolved_members() const -> const std::vector<unresolved_member_reference> &
  {
    return unresolved_members_;
  }

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
    for (const auto &entry : member_resolutions_)
      if (entry.candidates.size() == 1 && entry.candidates.front() == id &&
          std::find(result.begin(), result.end(), entry.use) == result.end())
        result.push_back(entry.use);
    return result;
  }

  auto semantic_index::symbol_at(const sagan::source::byte_offset offset) const -> const indexed_symbol *
  {
    const indexed_reference *best_reference = nullptr;
    for (const auto &entry : references_)
      if (contains(entry.location.bytes, offset) &&
          (!best_reference || entry.location.bytes.end - entry.location.bytes.begin <
                                  best_reference->location.bytes.end - best_reference->location.bytes.begin))
        best_reference = &entry;
    if (best_reference) return find(best_reference->target);
    const indexed_symbol *best = nullptr;
    for (const auto &entry : symbols_)
      if (contains(entry.declaration.bytes, offset) &&
          (!best || entry.declaration.bytes.end - entry.declaration.bytes.begin <
                        best->declaration.bytes.end - best->declaration.bytes.begin)) best = &entry;
    return best;
  }

  auto build_index(const sagan::source::document_snapshot &document, const semantic_model &model,
                   const type_model *types)
    -> semantic_index
  {
    return semantic_index(document.identity(), document.version(), model, types);
  }
}
