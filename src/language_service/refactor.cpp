#include "refactor.hpp"
#include "language_service.hpp"
#include "queries.hpp"
#include "../parser/tokens.hpp"
#include "../semantic/analyzer.hpp"
#include "../syntax/syntax.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace sagan::language_service
{
  namespace
  {
    auto local_binding(const semantic::symbol_kind kind) -> bool
    {
      return kind == semantic::symbol_kind::variable || kind == semantic::symbol_kind::constant ||
             kind == semantic::symbol_kind::parameter || kind == semantic::symbol_kind::loop_binding ||
             kind == semantic::symbol_kind::match_binding;
    }

    auto name_is_identifier(const std::string_view name) -> bool
    {
      const source::document_snapshot probe(
          {{source::document_id{std::numeric_limits<std::uint64_t>::max()},
            source::document_uri{"untitled:rename-probe"}, {}}, 1,
           "fun probe(): Int {\nlet " + std::string(name) + " = 1\nreturn 0\n}\n"});
      const auto parsed = syntax::analyze(probe, {.recover = false});
      if (!parsed.value) return false;
      const auto begin = static_cast<source::byte_offset>(std::string("fun probe(): Int {\nlet ").size());
      return std::any_of(parsed.value->tokens.begin(), parsed.value->tokens.end(),
                         [&](const auto &token)
                         {
                           return (token.kind == tokens::IDENTIFIER ||
                                   token.kind == tokens::METHOD_IDENTIFIER) &&
                                  token.range.begin == begin &&
                                  token.range.end == begin + name.size() && token.source_text == name;
                         });
    }
  }

  auto rename_local(const source::document_snapshot &document, const semantic::semantic_index &index,
                    const source::byte_offset position, const std::string_view new_name) -> edit_plan
  {
    if (document.version() != index.version() || document.identity().id != index.document().id)
      return {edit_state::stale, "Semantic index belongs to another document version", {}};
    if (!document.to_utf16(position)) return {edit_state::invalid, "Invalid source position", {}};
    const document_queries queries(document, index);
    const auto selected = queries.symbol_at(position);
    const auto *symbol = selected.value ? index.find(selected.value->id) : nullptr;
    if (!symbol || symbol->origin != semantic::symbol_origin::source)
      return {edit_state::unsupported, "Only source declarations can be renamed safely", {}};
    const bool local = local_binding(symbol->kind) && symbol->scope_id != 0;
    const bool function = symbol->kind == semantic::symbol_kind::function;
    const bool private_member = symbol->visibility == semantic::symbol_visibility::private_access &&
        (symbol->kind == semantic::symbol_kind::field ||
         symbol->kind == semantic::symbol_kind::constant_field ||
         symbol->kind == semantic::symbol_kind::method);
    if (!local && !function && !private_member)
      return {edit_state::unsupported,
              "Only local bindings, private members, and non-exported functions can be renamed safely", {}};
    if (function || symbol->kind == semantic::symbol_kind::method)
    {
      if (std::any_of(index.references().begin(), index.references().end(), [&](const auto &reference)
          { return reference.target == symbol->id &&
                   reference.kind == semantic::reference_kind::export_reference; }))
        return {edit_state::unsupported, "Exported functions require a workspace-wide rename proof", {}};
      if (std::any_of(index.overloads().begin(), index.overloads().end(), [&](const auto &overload)
          { return overload.name == symbol->name && overload.candidates.size() > 1; }))
        return {edit_state::unsupported, "Overloaded functions require a whole-set rename proof", {}};
    }
    if (new_name == symbol->name)
      return {edit_state::ready, {}, {{{document.identity().uri, document.version(), {}}}}};
    if (new_name.empty() || unicode::normalize_nfc(new_name) != new_name ||
        !name_is_identifier(new_name) ||
        !semantic::rename_preserves_binding_convention(symbol->kind, new_name))
      return {edit_state::invalid, "Proposed name is not a valid binding identifier", {}};
    for (const auto &candidate : index.symbols())
      if (candidate.name == new_name)
        return {edit_state::conflict, "Proposed name already exists in the semantic scope set", {}};
    const auto syntax = syntax::analyze(document, {.recover = false});
    if (!syntax.value || !syntax.value->strict_ast)
      return {edit_state::unsupported, "Rename requires a complete source document", {}};
    for (const auto &token : syntax.value->tokens)
      if ((token.kind == tokens::IDENTIFIER || token.kind == tokens::METHOD_IDENTIFIER) &&
          unicode::normalize_nfc(token.source_text) == new_name)
        return {edit_state::conflict, "Proposed name already occurs in source", {}};
    const auto occurrences = index.references_to(symbol->id, true);
    if (occurrences.empty()) return {edit_state::unsupported, "No complete reference set", {}};
    const auto definitions = queries.definitions(position);
    std::optional<source::source_range> declaration_name;
    if (definitions.value)
      for (const auto &location : *definitions.value)
        if (location.document == document.identity().id)
        {
          declaration_name = location;
          break;
        }
    if (!declaration_name) return {edit_state::unsupported, "Declaration name was not found", {}};
    versioned_document_edits document_edit{document.identity().uri, document.version(), {}};
    std::optional<source::byte_offset> declaration_token_begin;
    for (const auto &location : occurrences)
    {
      auto replacement = location;
      if (location == symbol->declaration)
      {
        replacement = *declaration_name;
        declaration_token_begin = replacement.bytes.begin;
      }
      else if (unicode::normalize_nfc(document.text().substr(
                   replacement.bytes.begin, replacement.bytes.end - replacement.bytes.begin)) != symbol->name)
      {
        std::optional<source::byte_range> reference_name;
        for (const auto &token : syntax.value->tokens)
          if ((token.kind == tokens::IDENTIFIER || token.kind == tokens::METHOD_IDENTIFIER) &&
              token.range.begin >= replacement.bytes.begin && token.range.end <= replacement.bytes.end &&
              unicode::normalize_nfc(token.source_text) == symbol->name)
            reference_name = token.range;
        if (!reference_name)
          return {edit_state::unsupported, "A reference cannot be rewritten as one identifier", {}};
        replacement.bytes = *reference_name;
      }
      if (replacement.document != document.identity().id ||
          !document.to_utf16(replacement.bytes.begin) || !document.to_utf16(replacement.bytes.end) ||
          unicode::normalize_nfc(document.text().substr(
              replacement.bytes.begin, replacement.bytes.end - replacement.bytes.begin)) != symbol->name)
        return {edit_state::unsupported, "A reference cannot be rewritten as one identifier", {}};
      document_edit.edits.push_back({replacement, std::string(new_name)});
    }
    std::sort(document_edit.edits.begin(), document_edit.edits.end(), [](const auto &left, const auto &right)
    { return left.range.bytes.begin < right.range.bytes.begin; });
    workspace_edit edits{{std::move(document_edit)}};
    const auto preview = preview_edits(edits, {&document});
    if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
    const source::document_snapshot changed(document.identity(), document.version(),
                                             preview.documents.front().text);
    const auto checked = index_document(changed);
    if (!checked.value || checked.state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Renamed source did not pass strict semantic analysis", {}};
    const semantic::indexed_symbol *renamed = nullptr;
    for (const auto &candidate : checked.value->index.symbols())
      if (candidate.origin == semantic::symbol_origin::source && candidate.name == new_name &&
          candidate.kind == symbol->kind && candidate.owner_type == symbol->owner_type)
      {
        if (renamed) return {edit_state::unsupported, "Renamed declaration is ambiguous", {}};
        renamed = &candidate;
      }
    if (!renamed || checked.value->index.references_to(renamed->id, true).size() != occurrences.size())
      return {edit_state::unsupported, "Reference set changed after rename", {}};
    const document_queries changed_queries(changed, checked.value->index);
    std::int64_t delta = 0;
    for (const auto &replacement : edits.documents.front().edits)
    {
      const auto mapped = static_cast<source::byte_offset>(
          static_cast<std::int64_t>(replacement.range.bytes.begin) + delta);
      if (!declaration_token_begin || replacement.range.bytes.begin != *declaration_token_begin)
      {
        const auto rebound = changed_queries.symbol_at(mapped);
        if (!rebound.value || rebound.value->id != renamed->id)
          return {edit_state::unsupported, "Reference identity changed after rename", {}};
      }
      delta += static_cast<std::int64_t>(new_name.size()) -
               static_cast<std::int64_t>(replacement.range.bytes.end - replacement.range.bytes.begin);
    }
    return {edit_state::ready, {}, std::move(edits)};
  }
}
