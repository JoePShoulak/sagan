#include "queries.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <string_view>

namespace sagan::language_service
{
  namespace
  {
    auto contains(const source::byte_range range, const source::byte_offset offset) -> bool
    {
      return range.begin <= offset && offset < range.end;
    }

    auto name_range(const source::document_snapshot &document,
                    const std::vector<syntax::lossless_token> &tokens,
                    const source::source_range broad,
                    const std::string_view name, const bool prefer_last) -> std::optional<source::source_range>
    {
      if (name.empty() || broad.document != document.identity().id) return {};
      std::optional<source::source_range> selected;
      for (const auto &token : tokens)
      {
        if (token.range.begin < broad.bytes.begin || token.range.end > broad.bytes.end) continue;
        if (token.kind != tokens::IDENTIFIER && token.kind != tokens::METHOD_IDENTIFIER &&
            token.kind != tokens::KWD_NEW) continue;
        if (unicode::normalize_nfc(token.source_text) != name) continue;
        selected = source::source_range{broad.document, token.range};
        if (!prefer_last) break;
      }
      return selected;
    }

    auto ordered_unique(std::vector<source::source_range> ranges) -> std::vector<source::source_range>
    {
      std::sort(ranges.begin(), ranges.end(), [](const auto &left, const auto &right)
      {
        if (left.document.value != right.document.value) return left.document.value < right.document.value;
        if (left.bytes.begin != right.bytes.begin) return left.bytes.begin < right.bytes.begin;
        return left.bytes.end < right.bytes.end;
      });
      ranges.erase(std::unique(ranges.begin(), ranges.end()), ranges.end());
      return ranges;
    }

    template<class T>
    auto result(const source::document_version version, const diagnostics::result_state state,
                std::optional<T> value = {}) -> diagnostics::analysis_result<T>
    {
      return {state, std::move(value), {}, version};
    }
  }

  document_queries::document_queries(const source::document_snapshot &document,
                                     const semantic::semantic_index &index,
                                     const semantic::workspace_semantic_index *workspace)
      : document_(document), index_(index), workspace_(workspace)
  {
    auto analyzed = syntax::analyze(document);
    if (analyzed.value) tokens_ = std::move(analyzed.value->tokens);
  }

  auto document_queries::occurrence_at(const source::byte_offset offset) const
    -> std::optional<symbol_occurrence>
  {
    const auto text = document_.text();
    if (offset >= text.size() || !document_.to_utf16(offset)) return {};
    std::optional<symbol_occurrence> best;
    auto accept = [&](const semantic::indexed_symbol &symbol, const source::source_range selection)
    {
      if (!contains(selection.bytes, offset)) return;
      if (best && best->selection.bytes.end - best->selection.bytes.begin <=
                      selection.bytes.end - selection.bytes.begin) return;
      best = symbol_occurrence{symbol.id, symbol.kind, symbol.origin, selection,
                               symbol.declaration, symbol.name};
    };
    for (const auto &reference : index_.references())
      if (const auto *symbol = index_.find(reference.target))
        if (const auto selection = name_range(document_, tokens_, reference.location, symbol->name, true))
          accept(*symbol, *selection);
    if (workspace_)
      for (const auto &reference : workspace_->external_references())
        if (reference.location.document == document_.identity().id)
          if (const auto *symbol = workspace_->find(reference.target))
            if (const auto selection = name_range(document_, tokens_, reference.location, symbol->name, true))
              accept(*symbol, *selection);
    for (const auto &symbol : index_.symbols())
    {
      if (symbol.origin == semantic::symbol_origin::builtin ||
          symbol.origin == semantic::symbol_origin::generated) continue;
      if (const auto selection = name_range(document_, tokens_, symbol.declaration, symbol.name, false))
        accept(symbol, *selection);
    }
    return best;
  }

  auto document_queries::symbol_at(const source::byte_offset offset) const
    -> diagnostics::analysis_result<symbol_occurrence>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<symbol_occurrence>(document_.version(), diagnostics::result_state::stale);
    return result<symbol_occurrence>(document_.version(), diagnostics::result_state::complete,
                                     occurrence_at(offset));
  }

  auto document_queries::symbol_at(const source::utf16_position position) const
    -> diagnostics::analysis_result<symbol_occurrence>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<symbol_occurrence>(document_.version(), diagnostics::result_state::stale);
    const auto offset = document_.to_byte(position);
    if (!offset) return result<symbol_occurrence>(document_.version(), diagnostics::result_state::incomplete);
    return symbol_at(*offset);
  }

  auto document_queries::definitions(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return result<std::vector<source::source_range>>(document_.version(), selected.state);
    if (!selected.value)
      return result<std::vector<source::source_range>>(document_.version(), selected.state,
                                                       std::vector<source::source_range>{});
    auto locations = workspace_ ? workspace_->definitions(selected.value->id)
                                : std::vector<source::source_range>{selected.value->declaration};
    for (auto &location : locations)
      if (location.document == document_.identity().id)
        if (const auto exact = name_range(document_, tokens_, location, selected.value->name, false)) location = *exact;
    return result<std::vector<source::source_range>>(document_.version(), selected.state,
                                                     ordered_unique(std::move(locations)));
  }

  auto document_queries::references(const source::byte_offset offset, const bool include_declaration) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return result<std::vector<source::source_range>>(document_.version(), selected.state);
    auto locations = !selected.value ? std::vector<source::source_range>{}
                           : workspace_ ? workspace_->references_to(selected.value->id, include_declaration)
                                        : index_.references_to(selected.value->id, include_declaration);
    if (selected.value && include_declaration)
      for (auto &location : locations)
        if (location.document == document_.identity().id &&
            location == selected.value->declaration)
          if (const auto exact = name_range(document_, tokens_, location, selected.value->name, false))
            location = *exact;
    return result<std::vector<source::source_range>>(document_.version(), selected.state,
                                                     ordered_unique(std::move(locations)));
  }

  auto document_queries::implementations(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return result<std::vector<source::source_range>>(document_.version(), selected.state);
    std::vector<source::source_range> locations;
    if (selected.value)
    {
      const auto collect = [&](const semantic::semantic_index &index)
      {
        for (const auto &conformance : index.conformances())
          if (conformance.interface == selected.value->id)
            locations.push_back(conformance.declaration);
      };
      if (workspace_)
        for (const auto &module : workspace_->modules()) collect(module.index);
      else collect(index_);
    }
    return result<std::vector<source::source_range>>(document_.version(), selected.state,
                                                     ordered_unique(std::move(locations)));
  }

  auto document_queries::document_highlights(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    auto found = references(offset, true);
    if (found.value)
      std::erase_if(*found.value, [&](const auto &location)
      {
        return location.document != document_.identity().id;
      });
    return found;
  }

  auto document_queries::resolved_type(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::string>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::string>(document_.version(), diagnostics::result_state::stale);
    const semantic::typed_range *best = nullptr;
    for (const auto &typed : index_.typed_ranges())
      if (contains(typed.location.bytes, offset) &&
          (!best || typed.location.bytes.end - typed.location.bytes.begin <
                        best->location.bytes.end - best->location.bytes.begin)) best = &typed;
    if (best)
      for (const auto &type : index_.types())
        if (type.id == best->type)
          return result<std::string>(document_.version(), diagnostics::result_state::complete, type.display);
    return result<std::string>(document_.version(), diagnostics::result_state::complete);
  }

  auto document_queries::hover(const source::byte_offset offset) const
    -> diagnostics::analysis_result<hover_information>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return result<hover_information>(document_.version(), selected.state);
    if (!selected.value) return result<hover_information>(document_.version(), selected.state);
    const auto *symbol = index_.find(selected.value->id);
    if (!symbol && workspace_) symbol = workspace_->find(selected.value->id);
    hover_information info{*selected.value, {}, symbol ? symbol->documentation : std::vector<std::string>{}};
    info.type = resolved_type(offset).value;
    return result<hover_information>(document_.version(), selected.state, std::move(info));
  }

  auto document_queries::document_symbols() const
    -> diagnostics::analysis_result<std::vector<document_symbol>>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::vector<document_symbol>>(document_.version(), diagnostics::result_state::stale);
    std::vector<document_symbol> symbols;
    for (const auto &symbol : index_.symbols())
    {
      if (symbol.origin != semantic::symbol_origin::source) continue;
      if (symbol.kind != semantic::symbol_kind::function && symbol.kind != semantic::symbol_kind::type &&
          symbol.kind != semantic::symbol_kind::variable && symbol.kind != semantic::symbol_kind::method &&
          symbol.kind != semantic::symbol_kind::field && symbol.kind != semantic::symbol_kind::constructor &&
          symbol.kind != semantic::symbol_kind::enum_case) continue;
      const auto selection = name_range(document_, tokens_, symbol.declaration, symbol.name, false);
      if (!selection) continue;
      symbols.push_back(document_symbol{symbol_occurrence{symbol.id, symbol.kind, symbol.origin, *selection,
                                                           symbol.declaration, symbol.name}, {}});
    }
    std::sort(symbols.begin(), symbols.end(), [](const auto &left, const auto &right)
    {
      if (left.symbol.selection.bytes.begin != right.symbol.selection.bytes.begin)
        return left.symbol.selection.bytes.begin < right.symbol.selection.bytes.begin;
      return left.symbol.name < right.symbol.name;
    });
    const auto build = [&](const auto &self, std::size_t &cursor, const source::byte_offset limit)
      -> std::vector<document_symbol>
    {
      std::vector<document_symbol> children;
      while (cursor < symbols.size() && symbols[cursor].symbol.selection.bytes.begin < limit)
      {
        auto current = std::move(symbols[cursor++]);
        const auto end = current.symbol.declaration.bytes.end;
        if (end > current.symbol.selection.bytes.end && end <= limit)
          current.children = self(self, cursor, end);
        children.push_back(std::move(current));
      }
      return children;
    };
    std::size_t cursor = 0;
    return result<std::vector<document_symbol>>(document_.version(), diagnostics::result_state::complete,
                                                build(build, cursor, static_cast<source::byte_offset>(document_.text().size())));
  }
}
