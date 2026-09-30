#include "queries.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <set>
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
                                     const semantic::workspace_semantic_index *workspace,
                                     const semantic::semantic_model *model)
      : document_(document), index_(index), workspace_(workspace), model_(model)
  {
    auto analyzed = syntax::analyze(document);
    if (analyzed.value)
    {
      tokens_ = std::move(analyzed.value->tokens);
      trailing_trivia_ = std::move(analyzed.value->trailing_trivia);
      tree_ = analyzed.value->strict_ast ? std::move(analyzed.value->strict_ast)
                                         : std::move(analyzed.value->recovered_ast);
    }
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

  auto document_queries::semantic_classifications() const
    -> diagnostics::analysis_result<std::vector<semantic_classification>>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::vector<semantic_classification>>(document_.version(), diagnostics::result_state::stale);
    std::vector<semantic_classification> values;
    const auto append = [&](const semantic::indexed_symbol &symbol, const source::source_range range,
                            const bool declaration)
    {
      values.push_back(semantic_classification{symbol.id, symbol.kind, range, declaration,
                                                symbol.kind == semantic::symbol_kind::constant ||
                                                    symbol.kind == semantic::symbol_kind::constant_field,
                                                symbol.visibility == semantic::symbol_visibility::private_access,
                                                symbol.origin == semantic::symbol_origin::builtin});
    };
    for (const auto &symbol : index_.symbols())
      if (symbol.origin == semantic::symbol_origin::source ||
          symbol.origin == semantic::symbol_origin::imported)
        if (const auto selection = name_range(document_, tokens_, symbol.declaration, symbol.name, false))
          append(symbol, *selection, true);
    for (const auto &reference : index_.references())
      if (const auto *symbol = index_.find(reference.target))
        if (const auto selection = name_range(document_, tokens_, reference.location, symbol->name, true))
          append(*symbol, *selection, false);
    if (workspace_)
      for (const auto &reference : workspace_->external_references())
        if (reference.location.document == document_.identity().id)
          if (const auto *symbol = workspace_->find(reference.target))
            if (const auto selection = name_range(document_, tokens_, reference.location, symbol->name, true))
              append(*symbol, *selection, false);
    std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
    {
      if (left.range.bytes.begin != right.range.bytes.begin)
        return left.range.bytes.begin < right.range.bytes.begin;
      if (left.range.bytes.end != right.range.bytes.end)
        return left.range.bytes.end < right.range.bytes.end;
      return left.declaration && !right.declaration;
    });
    values.erase(std::unique(values.begin(), values.end(), [](const auto &left, const auto &right)
    {
      return left.range == right.range;
    }), values.end());
    return result<std::vector<semantic_classification>>(document_.version(), diagnostics::result_state::complete,
                                                        std::move(values));
  }

  auto document_queries::folding_regions() const
    -> diagnostics::analysis_result<std::vector<folding_region>>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::vector<folding_region>>(document_.version(), diagnostics::result_state::stale);
    std::vector<folding_region> folds;
    std::vector<source::byte_offset> braces;
    const auto append_if_multiline = [&](const source::byte_range range, const folding_kind kind)
    {
      const auto first = document_.to_utf16(range.begin);
      const auto last = document_.to_utf16(range.end);
      if (first && last && last->line > first->line)
        folds.push_back(folding_region{{document_.identity().id, range}, kind});
    };
    const auto inspect_trivia = [&](const std::vector<syntax::trivia> &entries)
    {
      for (const auto &entry : entries)
        if (entry.kind == syntax::trivia_kind::block_comment)
          append_if_multiline(entry.range, folding_kind::comment);
    };
    for (const auto &token : tokens_)
    {
      inspect_trivia(token.leading_trivia);
      if (token.kind == tokens::LBRACE) braces.push_back(token.range.begin);
      else if (token.kind == tokens::RBRACE && !braces.empty())
      {
        append_if_multiline({braces.back(), token.range.end}, folding_kind::block);
        braces.pop_back();
      }
      else if (token.kind == tokens::STRING || token.kind == tokens::STRING_SEGMENT)
        append_if_multiline(token.range, folding_kind::string_literal);
    }
    inspect_trivia(trailing_trivia_);
    std::sort(folds.begin(), folds.end(), [](const auto &left, const auto &right)
    {
      if (left.range.bytes.begin != right.range.bytes.begin)
        return left.range.bytes.begin < right.range.bytes.begin;
      return left.range.bytes.end < right.range.bytes.end;
    });
    return result<std::vector<folding_region>>(document_.version(), diagnostics::result_state::complete,
                                               std::move(folds));
  }

  auto document_queries::import_links() const
    -> diagnostics::analysis_result<std::vector<document_link>>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::vector<document_link>>(document_.version(), diagnostics::result_state::stale);
    std::vector<document_link> links;
    if (!workspace_ || !tree_)
      return result<std::vector<document_link>>(document_.version(), diagnostics::result_state::complete,
                                                std::move(links));
    for (const auto &statement : tree_->statements)
    {
      const auto *imported = dynamic_cast<const parser::import_declaration *>(statement.get());
      if (!imported) continue;
      const auto &module_name = imported->source_module.value_or(imported->imported_name);
      const auto target = std::find_if(workspace_->modules().begin(), workspace_->modules().end(),
                                       [&](const auto &module) { return module.name == module_name; });
      if (target == workspace_->modules().end()) continue;
      const int pivot = imported->source_module ? tokens::KWD_FROM : tokens::KWD_IMPORT;
      bool reading = false;
      std::optional<source::byte_range> range;
      for (const auto &token : tokens_)
      {
        if (token.range.begin < static_cast<source::byte_offset>(imported->range.begin) ||
            token.range.end > static_cast<source::byte_offset>(imported->range.end)) continue;
        if (token.kind == pivot) { reading = true; continue; }
        if (!reading) continue;
        if (token.kind == tokens::IDENTIFIER)
        {
          if (!range) range = token.range;
          else range->end = token.range.end;
        }
        else if (token.kind != tokens::DOT) break;
      }
      if (range)
        links.push_back(document_link{{document_.identity().id, *range},
                                      target->index.document().uri});
    }
    return result<std::vector<document_link>>(document_.version(), diagnostics::result_state::complete,
                                              std::move(links));
  }

  auto document_queries::completions(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<completion_item>>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::stale);
    if (!document_.to_utf16(offset))
      return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::incomplete);
    source::source_range replacement{document_.identity().id, {offset, offset}};
    std::string prefix;
    for (const auto &token : tokens_)
      if ((token.kind == tokens::IDENTIFIER || token.kind == tokens::METHOD_IDENTIFIER) &&
          token.range.begin <= offset && offset <= token.range.end)
      {
        replacement.bytes = token.range;
        prefix = unicode::normalize_nfc(std::string_view(token.source_text).substr(0, offset - token.range.begin));
        break;
      }
    std::vector<completion_item> values;
    if (!model_ || model_->scopes.empty())
      return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::incomplete);
    std::size_t innermost = 0;
    auto smallest = document_.text().size() + 1;
    for (const auto &scope : model_->scopes)
    {
      if (scope.id == 0 || scope.range.begin < 0 || scope.range.end < scope.range.begin) continue;
      if (static_cast<std::size_t>(scope.range.begin) <= offset &&
          offset <= static_cast<std::size_t>(scope.range.end))
      {
        const auto width = static_cast<std::size_t>(scope.range.end - scope.range.begin);
        if (width < smallest) { smallest = width; innermost = scope.id; }
      }
    }
    std::set<std::string> seen;
    auto scope_id = innermost;
    while (scope_id < model_->scopes.size())
    {
      const auto &scope = model_->scopes[scope_id];
      for (const auto &symbol : scope.symbols)
      {
        if (symbol.origin == semantic::symbol_origin::generated ||
            (!prefix.empty() && !symbol.name.starts_with(prefix)) || seen.contains(symbol.name)) continue;
        const bool local_variable = symbol.kind == semantic::symbol_kind::variable ||
                                    symbol.kind == semantic::symbol_kind::constant;
        if (scope_id != 0 && local_variable &&
            (symbol.declaration.begin > static_cast<int>(offset) ||
             symbol.declaration.end > static_cast<int>(offset))) continue;
        if (scope_id != 0 && !local_variable &&
            symbol.declaration.begin > static_cast<int>(offset)) continue;
        seen.insert(symbol.name);
        std::string detail(semantic::name(symbol.kind));
        for (const auto &typed : index_.typed_ranges())
          if (typed.declaration && typed.location.bytes.begin ==
                                       static_cast<source::byte_offset>(std::max(symbol.declaration.begin, 0)) &&
              typed.location.bytes.end ==
                  static_cast<source::byte_offset>(std::max(symbol.declaration.end, symbol.declaration.begin)))
            for (const auto &type : index_.types())
              if (type.id == typed.type) detail += ": " + type.display;
        values.push_back(completion_item{symbol.name, symbol.id, symbol.kind, replacement,
                                         symbol.name, std::move(detail), symbol.documentation,
                                         symbol.origin == semantic::symbol_origin::builtin ? "sagan/core" : "local",
                                         symbol.name, symbol.name, false, {}});
      }
      if (scope_id == 0) break;
      scope_id = scope.parent;
    }
    std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
    {
      if (left.sort_text != right.sort_text) return left.sort_text < right.sort_text;
      return left.id.value < right.id.value;
    });
    return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::complete,
                                                std::move(values));
  }

  auto search_workspace_symbols(const semantic::workspace_semantic_index &workspace,
                                const std::string_view query, const std::size_t limit)
    -> std::vector<workspace_symbol>
  {
    std::vector<workspace_symbol> found;
    for (const auto &module : workspace.modules())
      for (const auto &symbol : module.index.symbols())
      {
        if (symbol.origin != semantic::symbol_origin::source || symbol.name.find(query) == std::string::npos)
          continue;
        found.push_back(workspace_symbol{symbol.id, symbol.name, symbol.kind, symbol.declaration,
                                          module.name, symbol.visibility});
      }
    std::sort(found.begin(), found.end(), [](const auto &left, const auto &right)
    {
      if (left.name != right.name) return left.name < right.name;
      if (left.module != right.module) return left.module < right.module;
      return left.declaration.bytes.begin < right.declaration.bytes.begin;
    });
    if (found.size() > limit) found.resize(limit);
    return found;
  }
}
