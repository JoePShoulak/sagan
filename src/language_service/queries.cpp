#include "queries.hpp"
#include "refactor.hpp"
#include "../parser/lex.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"
#include "../semantic/builtin_members.hpp"

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

    struct builtin_member_use
    {
      source::source_range selection;
      semantic::builtin_member member;
      semantic::symbol_id id;
    };

    auto builtin_member_at(const source::document_snapshot &document,
                           const std::vector<syntax::lossless_token> &tokens,
                           const semantic::semantic_index &index,
                           const source::byte_offset offset) -> std::optional<builtin_member_use>
    {
      for (const auto &record : index.member_resolutions())
      {
        const auto type = std::find_if(index.types().begin(), index.types().end(),
                                       [&](const auto &entry) { return entry.id == record.receiver; });
        if (type == index.types().end()) continue;
        auto builtin = semantic::integer_builtin_member(type->display, record.member);
        if (!builtin) builtin = semantic::integer_builtin_static_method(type->display, record.member);
        if (!builtin) builtin = semantic::vector_builtin_method(type->display, record.member);
        if (!builtin) continue;
        if (const auto selection = name_range(document, tokens, record.use, builtin->name, true);
            selection && contains(selection->bytes, offset))
          return builtin_member_use{*selection, *builtin,
              {type->display.starts_with("Vector")
                   ? "sagan-core-member:Vector." + std::string(builtin->name)
                   : builtin->callable ? "sagan-core-member:Int.round"
                                              : "sagan-core-member:Int.times"}};
      }
      return {};
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
                                     const semantic::semantic_model *model,
                                     const semantic::type_model *types)
      : document_(document), index_(index), workspace_(workspace), model_(model), types_(types)
  {
    auto analyzed = syntax::analyze(document);
    if (analyzed.value)
    {
      strict_syntax_ = static_cast<bool>(analyzed.value->strict_ast);
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
    for (const auto &member : index_.member_resolutions())
      if (member.candidates.size() == 1)
        if (const auto *symbol = index_.find(member.candidates.front()))
          if (const auto selection = name_range(document_, tokens_, member.use, symbol->name, true))
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
    if (workspace_)
    {
      for (const auto &module : workspace_->modules())
        if (module.index.document().id == document_.identity().id &&
            module.index.version() == document_.version())
          if (const auto *binding = module.index.symbol_at(offset))
            if (const auto type = workspace_->declared_type(binding->id))
              return result<std::string>(document_.version(), diagnostics::result_state::complete, *type);
    }
    return result<std::string>(document_.version(), diagnostics::result_state::complete);
  }

  auto document_queries::hover(const source::byte_offset offset) const
    -> diagnostics::analysis_result<hover_information>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return result<hover_information>(document_.version(), selected.state);
    if (!selected.value)
    {
      if (const auto builtin = builtin_member_at(document_, tokens_, index_, offset))
      {
        return result<hover_information>(document_.version(), selected.state,
            hover_information{{builtin->id, builtin->member.callable ? semantic::symbol_kind::method
                                                           : semantic::symbol_kind::field,
                               semantic::symbol_origin::builtin,
                               builtin->selection, builtin->selection, std::string(builtin->member.name)},
                              std::string(builtin->member.result_type),
                              {std::string(builtin->member.documentation)}});
      }
      return result<hover_information>(document_.version(), selected.state);
    }
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
      if (symbol.origin == semantic::symbol_origin::builtin ||
          symbol.origin == semantic::symbol_origin::generated) continue;
      if (symbol.kind == semantic::symbol_kind::parameter ||
          symbol.kind == semantic::symbol_kind::type_parameter ||
          symbol.kind == semantic::symbol_kind::loop_binding ||
          symbol.kind == semantic::symbol_kind::match_binding ||
          symbol.kind == semantic::symbol_kind::self_value) continue;
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
                                                symbol.origin == semantic::symbol_origin::builtin,
                                                std::any_of(symbol.documentation.begin(), symbol.documentation.end(),
                                                            [](const auto &line)
                                                            { return line.starts_with("@deprecated"); }),
                                                false, symbol.origin == semantic::symbol_origin::generated});
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
    for (const auto &member : index_.unresolved_members())
    {
      if (member.use.begin < 0 || member.use.end < member.use.begin) continue;
      const source::byte_range use{static_cast<source::byte_offset>(member.use.begin),
                                   static_cast<source::byte_offset>(member.use.end)};
      const bool resolved = std::any_of(index_.member_resolutions().begin(),
                                        index_.member_resolutions().end(), [&](const auto &record)
                                        { return record.use.bytes == use && !record.candidates.empty(); });
      if (resolved) continue;
      if (const auto selection = name_range(document_, tokens_, {document_.identity().id, use},
                                            member.member, true))
        values.push_back(semantic_classification{{}, semantic::symbol_kind::field, *selection, false,
                                                  false, false, false, false, true, false});
    }
    for (const auto &record : index_.member_resolutions())
      if (const auto builtin = builtin_member_at(document_, tokens_, index_, record.use.bytes.end - 1))
        values.push_back(semantic_classification{builtin->id,
                                                  builtin->member.callable ? semantic::symbol_kind::method
                                                                           : semantic::symbol_kind::field,
                                                  builtin->selection,
                                                  false, true, false, true, false, false, false});
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
    const syntax::lossless_token *previous = nullptr;
    const syntax::lossless_token *before_previous = nullptr;
    for (const auto &token : tokens_)
    {
      if (token.range.end > replacement.bytes.begin) break;
      if (token.kind == tokens::NEWLINE) { previous = nullptr; before_previous = nullptr; continue; }
      before_previous = previous;
      previous = &token;
    }
    if (previous && (previous->kind == tokens::KWD_IMPORT || previous->kind == tokens::KWD_FROM))
    {
      if (workspace_)
        for (const auto &module : workspace_->modules())
          if (module.name.starts_with(prefix))
            values.push_back(completion_item{module.name, {"sagan-module-v1:" + module.name},
                                             semantic::symbol_kind::module, replacement, module.name,
                                             "module", {}, module.name, module.name, module.name, false, {}});
      std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
      { return left.label < right.label; });
      return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::complete,
                                                  std::move(values));
    }
    if (previous && before_previous &&
        (previous->kind == tokens::DOT || previous->kind == tokens::SAFE_DOT))
    {
      const auto receiver = symbol_at(before_previous->range.begin);
      if (receiver.value && receiver.value->kind == semantic::symbol_kind::imported_namespace && workspace_)
      {
        for (const auto &imported : workspace_->imports())
          if (imported.binding == receiver.value->id && imported.whole_module)
            for (const auto &module : workspace_->modules())
              if (module.name == imported.source_module)
                for (const auto &exported : workspace_->exported_symbols())
                  if (exported.module == module.name && exported.public_name.starts_with(prefix))
                    for (const auto &target : exported.targets)
                      if (const auto *symbol = module.index.find(target);
                          symbol && symbol->visibility == semantic::symbol_visibility::public_access)
                      {
                        const auto signature = semantic::callable_signature(module.index, target);
                        values.push_back(completion_item{exported.public_name, symbol->id,
                                                         symbol->kind, replacement,
                                                         exported.public_name,
                                                         signature.empty() ?
                                                             std::string(semantic::name(symbol->kind)) :
                                                             signature,
                                                         symbol->documentation, module.name,
                                                         exported.public_name, exported.public_name,
                                                         false, {}});
                      }
      }
      else
      {
        std::string type_name;
        if (const auto type = resolved_type(before_previous->range.begin); type.value) type_name = *type.value;
        if (receiver.value && receiver.value->kind == semantic::symbol_kind::type)
          type_name = receiver.value->name;
        if (receiver.value && receiver.value->kind == semantic::symbol_kind::builtin_type)
          type_name = receiver.value->name;
        const std::string receiver_type = type_name;
        type_name = type_name.substr(0, type_name.find_first_of("< (["));
        if (receiver.value && receiver.value->kind == semantic::symbol_kind::builtin_type)
          if (const auto builtin = semantic::integer_builtin_static_method(type_name, "round");
              builtin && builtin->name.starts_with(prefix))
            values.push_back(completion_item{std::string(builtin->name), {"sagan-core-member:Int.round"},
                                             semantic::symbol_kind::method, replacement,
                                             std::string(builtin->name) + "(", "(Float) => Int64",
                                             {std::string(builtin->documentation)}, "sagan/core",
                                             std::string(builtin->name), std::string(builtin->name), false, {}});
        if (const auto builtin = semantic::integer_builtin_member(type_name, "times");
            builtin && (!receiver.value || receiver.value->kind != semantic::symbol_kind::builtin_type) &&
            builtin->name.starts_with(prefix))
          values.push_back(completion_item{std::string(builtin->name), {"sagan-core-member:Int.times"},
                                           semantic::symbol_kind::field, replacement,
                                           std::string(builtin->name), std::string(builtin->result_type),
                                           {std::string(builtin->documentation)}, "sagan/core",
                                           std::string(builtin->name), std::string(builtin->name), false, {}});
        if (!receiver_type.empty() &&
            (!receiver.value || receiver.value->kind != semantic::symbol_kind::builtin_type))
          for (const std::string_view name : {"length", "squared_length", "normalized", "normalized!"})
            if (const auto builtin = semantic::vector_builtin_method(receiver_type, name);
                builtin && builtin->name.starts_with(prefix))
            {
              const auto open = receiver_type.find('<');
              const bool measured = open != std::string::npos &&
                                    receiver_type.find('<', open + 1) != std::string::npos;
              if (measured && name == "normalized!") continue;
              values.push_back(completion_item{std::string(builtin->name),
                                               {"sagan-core-member:Vector." + std::string(builtin->name)},
                                               semantic::symbol_kind::method, replacement,
                                               std::string(builtin->name) + "(",
                                               "() => " + std::string(builtin->result_type),
                                               {std::string(builtin->documentation)}, "sagan/core",
                                               std::string(builtin->name), std::string(builtin->name), false, {}});
            }
        const auto inside_owner = [&]()
        {
          for (const auto &symbol : index_.symbols())
            if (symbol.kind == semantic::symbol_kind::type && symbol.name == type_name &&
                symbol.declaration.bytes.begin <= offset && offset < symbol.declaration.bytes.end)
              return true;
          return false;
        }();
        std::set<std::string> provided;
        const auto append_members = [&](const semantic::semantic_index &index, const std::string &module,
                                        const std::string &owner_name, const bool same_type)
        {
          for (const auto &symbol : index.symbols())
            if (symbol.owner_type == owner_name && symbol.name.starts_with(prefix) &&
                (symbol.kind == semantic::symbol_kind::field ||
                 symbol.kind == semantic::symbol_kind::constant_field ||
                 symbol.kind == semantic::symbol_kind::method ||
                 symbol.kind == semantic::symbol_kind::enum_case) &&
                (symbol.visibility == semantic::symbol_visibility::public_access || same_type) &&
                provided.insert(symbol.name).second)
            {
              const auto signature = semantic::callable_signature(index, symbol.id);
              values.push_back(completion_item{symbol.name, symbol.id, symbol.kind, replacement,
                                               symbol.name, signature.empty() ?
                                                   std::string(semantic::name(symbol.kind)) : signature,
                                               symbol.documentation, module, symbol.name, symbol.name, false, {}});
            }
        };
        if (!type_name.empty())
        {
          struct type_candidate
          {
            const semantic::semantic_index *index;
            const semantic::indexed_symbol *symbol;
            std::string module;
          };
          std::vector<type_candidate> roots;
          const auto collect_type = [&](const semantic::semantic_index &index, const std::string &module)
          {
            for (const auto &symbol : index.symbols())
              if (symbol.kind == semantic::symbol_kind::type &&
                  symbol.origin == semantic::symbol_origin::source && symbol.name == type_name)
                roots.push_back({&index, &symbol, module});
          };
          collect_type(index_, "local");
          if (roots.empty() && workspace_)
            for (const auto &module : workspace_->modules())
              if (module.index.document().id != document_.identity().id)
                collect_type(module.index, module.name);
          const auto accessible = type_definitions(before_previous->range.begin);
          if (!accessible.value || accessible.value->size() != 1) roots.clear();
          else std::erase_if(roots, [&](const auto &candidate)
          { return candidate.symbol->declaration != accessible.value->front(); });
          if (roots.size() == 1)
          {
            std::vector<type_candidate> pending{roots.front()};
            std::set<std::string> visited;
            while (!pending.empty())
            {
              const auto current = pending.front();
              pending.erase(pending.begin());
              if (!visited.insert(current.symbol->id.value).second) continue;
              append_members(*current.index, current.module, current.symbol->name,
                             inside_owner && current.symbol->id == roots.front().symbol->id);
              for (const auto &edge : current.index->conformances())
                if (edge.implementer == current.symbol->id)
                {
                  const auto enqueue = [&](const semantic::semantic_index &index, const std::string &module)
                  {
                    if (const auto *related = index.find(edge.interface))
                      pending.push_back({&index, related, module});
                  };
                  enqueue(index_, "local");
                  if (workspace_)
                    for (const auto &module : workspace_->modules()) enqueue(module.index, module.name);
                }
            }
          }
        }
      }
      std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
      {
        if (left.label != right.label) return left.label < right.label;
        return left.id.value < right.id.value;
      });
      values.erase(std::unique(values.begin(), values.end(), [](const auto &left, const auto &right)
      { return left.id == right.id && left.label == right.label; }), values.end());
      return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::complete,
                                                  std::move(values));
    }
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
    if (workspace_ && tree_ && strict_syntax_)
    {
      std::string current_module;
      if (!tree_->statements.empty())
      {
        if (const auto *module = dynamic_cast<const parser::module_declaration *>(
                tree_->statements.front().get()))
          current_module = module->name;
      }
      if (!current_module.empty())
      {
        std::size_t auto_import_count = 0;
        for (const auto &exported : workspace_->exported_symbols())
        {
          if (auto_import_count >= 128) break;
          if (exported.module == current_module || exported.targets.empty() ||
              !exported.public_name.starts_with(prefix) || seen.contains(exported.public_name)) continue;
          if (std::any_of(index_.symbols().begin(), index_.symbols().end(), [&](const auto &symbol)
                          { return symbol.name == exported.public_name; })) continue;
          const bool imported = std::any_of(workspace_->imports().begin(), workspace_->imports().end(),
                                            [&](const auto &link)
                                            {
                                              if (link.whole_module || !index_.find(link.binding)) return false;
                                              return std::any_of(exported.targets.begin(), exported.targets.end(),
                                                                 [&](const auto &target)
                                                                 { return std::find(link.targets.begin(),
                                                                                    link.targets.end(), target) !=
                                                                          link.targets.end(); });
                                            });
          if (imported) continue;
          const auto *target = workspace_->find(exported.targets.front());
          if (!target) continue;
          std::string detail(semantic::name(target->kind));
          for (const auto &module : workspace_->modules())
            if (module.name == exported.module)
            {
              std::vector<std::string> signatures;
              for (const auto &overload : exported.targets)
              {
                const auto signature = semantic::callable_signature(module.index, overload);
                if (!signature.empty() &&
                    std::find(signatures.begin(), signatures.end(), signature) == signatures.end())
                  signatures.push_back(signature);
              }
              if (!signatures.empty())
              {
                detail.clear();
                for (const auto &signature : signatures)
                {
                  if (!detail.empty()) detail += " | ";
                  detail += signature;
                }
              }
              break;
            }
          const auto plan = add_missing_import(document_, *workspace_, target->id);
          if (plan.state != edit_state::ready || plan.edits.documents.size() != 1 ||
              plan.edits.documents.front().uri != document_.identity().uri ||
              plan.edits.documents.front().expected_version != document_.version() ||
              plan.edits.documents.front().edits.size() != 1) continue;
          const auto &edit = plan.edits.documents.front().edits.front();
          values.push_back(completion_item{exported.public_name, target->id, target->kind, replacement,
                                           exported.public_name, std::move(detail),
                                           target->documentation, exported.module, exported.public_name,
                                           exported.public_name + ":" + exported.module, false,
                                           {edit}});
          ++auto_import_count;
        }
      }
    }
    if (previous && (previous->kind == tokens::KWD_IS || previous->kind == tokens::KWD_HAS))
      std::erase_if(values, [](const auto &item)
      {
        return item.kind != semantic::symbol_kind::type &&
               item.kind != semantic::symbol_kind::builtin_type &&
               item.kind != semantic::symbol_kind::type_parameter;
      });
    const auto text = document_.text();
    const auto line_break = replacement.bytes.begin == 0 ? std::string_view::npos :
                            text.rfind('\n', replacement.bytes.begin - 1);
    const auto line_begin = line_break == std::string_view::npos ? 0 : line_break + 1;
    const bool statement_start = std::all_of(text.begin() + static_cast<std::ptrdiff_t>(line_begin),
                                              text.begin() + static_cast<std::ptrdiff_t>(replacement.bytes.begin),
                                              [](const char byte) { return byte == ' ' || byte == '\t'; });
    bool inside_literal_or_comment = false;
    for (const auto &token : tokens_)
    {
      if (token.range.begin <= offset && offset < token.range.end &&
          (token.kind == tokens::STRING || token.kind == tokens::STRING_SEGMENT))
        inside_literal_or_comment = true;
      for (const auto &trivia : token.leading_trivia)
        if (trivia.range.begin <= offset && offset < trivia.range.end &&
            trivia.kind != syntax::trivia_kind::whitespace) inside_literal_or_comment = true;
    }
    if (statement_start && !inside_literal_or_comment)
    {
      bool in_function = false;
      bool in_loop = false;
      bool in_type = false;
      auto context = innermost;
      while (context < model_->scopes.size())
      {
        const auto &scope = model_->scopes[context];
        in_function |= scope.label.starts_with("function ") || scope.label == "lambda";
        in_loop |= scope.label == "loop body" || scope.label == "for loop";
        in_type |= scope.label.starts_with("type ");
        if (context == 0) break;
        context = scope.parent;
      }
      const auto allowed = [&](const int token)
      {
        if (innermost == 0)
        {
          if (token == tokens::KWD_MODULE) return tree_ && tree_->statements.empty();
          return token == tokens::KWD_FUN || token == tokens::KWD_CLASS ||
                 token == tokens::KWD_FACE || token == tokens::KWD_ENUM ||
                 token == tokens::KWD_LET || token == tokens::KWD_CONST ||
                 token == tokens::KWD_IMPORT || token == tokens::KWD_FROM ||
                 token == tokens::KWD_EXPORT || token == tokens::KWD_DIMENSION ||
                 token == tokens::KWD_QUANTITY || token == tokens::KWD_UNIT ||
                 token == tokens::KWD_AFFINE;
        }
        if (in_type && !in_function) return token == tokens::KWD_LET || token == tokens::KWD_CONST ||
                                              token == tokens::KWD_FUN || token == tokens::KWD_NEW;
        if (in_function)
          return token == tokens::KWD_LET || token == tokens::KWD_CONST ||
                 token == tokens::KWD_IF || token == tokens::KWD_MATCH ||
                 token == tokens::KWD_FOR || token == tokens::KWD_WHILE ||
                 token == tokens::KWD_UNTIL || token == tokens::KWD_HOPE ||
                 token == tokens::KWD_SCREAM || token == tokens::KWD_RETURN ||
                 token == tokens::KWD_FUN ||
                 (in_loop && (token == tokens::KWD_BREAK || token == tokens::KWD_CONTINUE));
        return false;
      };
      for (const auto &[spelling, token] : language_keywords())
        if (allowed(token) && spelling.starts_with(prefix) && !seen.contains(spelling))
          values.push_back(completion_item{spelling, {"sagan-keyword-v1:" + spelling},
                                           semantic::symbol_kind::builtin_value, replacement, spelling,
                                           "keyword", {}, "sagan/syntax", spelling, spelling, false, {}});
    }
    std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
    {
      if (left.sort_text != right.sort_text) return left.sort_text < right.sort_text;
      return left.id.value < right.id.value;
    });
    return result<std::vector<completion_item>>(document_.version(), diagnostics::result_state::complete,
                                                std::move(values));
  }

  auto document_queries::signature_help(const source::byte_offset offset) const
    -> diagnostics::analysis_result<signature_information>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<signature_information>(document_.version(), diagnostics::result_state::stale);
    if (!document_.to_utf16(offset))
      return result<signature_information>(document_.version(), diagnostics::result_state::incomplete);

    if (!types_)
    {
      std::vector<const syntax::lossless_token *> open_calls;
      for (const auto &token : tokens_)
      {
        if (token.range.begin >= offset) break;
        if (token.kind == tokens::LPAREN) open_calls.push_back(&token);
        else if (token.kind == tokens::RPAREN && !open_calls.empty()) open_calls.pop_back();
      }
      if (open_calls.empty() || !tree_)
        return result<signature_information>(document_.version(), diagnostics::result_state::incomplete);
      const auto *opening = open_calls.back();
      const syntax::lossless_token *callee = nullptr;
      const syntax::lossless_token *before_callee = nullptr;
      for (const auto &token : tokens_)
      {
        if (token.range.end > opening->range.begin) break;
        before_callee = callee;
        callee = &token;
      }
      if (!callee || (callee->kind != tokens::IDENTIFIER && callee->kind != tokens::METHOD_IDENTIFIER) ||
          (before_callee && before_callee->kind == tokens::KWD_FUN))
        return result<signature_information>(document_.version(), diagnostics::result_state::incomplete);
      const parser::function_declaration *declaration = nullptr;
      std::size_t candidate_count = 0;
      const auto find_function = [&](const parser::statement &statement)
      {
        if (const auto *function = dynamic_cast<const parser::function_declaration *>(&statement);
            function && function->name == unicode::normalize_nfc(callee->source_text))
        {
          declaration = function;
          ++candidate_count;
        }
      };
      for (const auto &statement : tree_->statements)
      {
        find_function(*statement);
        if (const auto *type = dynamic_cast<const parser::type_declaration *>(statement.get()))
          for (const auto &member : type->members) find_function(*member);
      }
      if (candidate_count != 1)
        return result<signature_information>(document_.version(), diagnostics::result_state::incomplete);
      std::vector<std::string> names;
      std::vector<std::string> types;
      for (const auto &parameter : declaration->parameters)
      {
        names.push_back(parameter.name);
        types.push_back(parameter.type_name.value_or("Unknown"));
      }
      std::size_t active = 0;
      int depth = 1;
      int brackets = 0;
      int braces = 0;
      for (const auto &token : tokens_)
      {
        if (token.range.begin < opening->range.end) continue;
        if (token.range.begin >= offset) break;
        if (token.kind == tokens::LPAREN) ++depth;
        else if (token.kind == tokens::RPAREN) --depth;
        else if (token.kind == tokens::LBRACKET) ++brackets;
        else if (token.kind == tokens::RBRACKET) --brackets;
        else if (token.kind == tokens::LBRACE) ++braces;
        else if (token.kind == tokens::RBRACE) --braces;
        else if (token.kind == tokens::COMMA && depth == 1 && brackets == 0 && braces == 0) ++active;
      }
      std::string label = declaration->name + "(";
      for (std::size_t i = 0; i < names.size(); ++i)
      {
        if (i) label += ", ";
        label += names[i] + ": " + types[i];
      }
      label += "): " + declaration->return_type.value_or("Void");
      std::vector<std::string> docs;
      for (const auto &comment : declaration->documentation) docs.push_back(comment.text);
      return result<signature_information>(
          document_.version(), diagnostics::result_state::recovered,
          signature_information{{document_.identity().id, {callee->range.begin, offset}}, std::move(label),
                                std::move(types), std::move(names), declaration->type_parameters,
                                std::move(docs), declaration->return_type.value_or("Void"), active, {}, 0});
    }

    const semantic::resolved_call *selected = nullptr;
    for (const auto &call : types_->calls)
    {
      if (call.range.begin < 0 || call.callee_end < call.range.begin ||
          static_cast<std::size_t>(call.callee_end) > offset ||
          offset >= static_cast<std::size_t>(call.range.end)) continue;
      if (!selected || call.range.end - call.range.begin < selected->range.end - selected->range.begin)
        selected = &call;
    }
    if (!selected)
      return result<signature_information>(document_.version(), diagnostics::result_state::complete);

    const syntax::lossless_token *opening = nullptr;
    for (const auto &token : tokens_)
      if (token.kind == tokens::LPAREN &&
          token.range.begin >= static_cast<std::size_t>(selected->callee_end) &&
          token.range.end <= static_cast<std::size_t>(selected->range.end))
      {
        opening = &token;
        break;
      }
    if (!opening || offset < opening->range.end)
      return result<signature_information>(document_.version(), diagnostics::result_state::complete);

    std::size_t active_parameter = 0;
    int parentheses = 1;
    int brackets = 0;
    int braces = 0;
    for (const auto &token : tokens_)
    {
      if (token.range.begin < opening->range.end) continue;
      if (token.range.begin >= offset) break;
      if (token.kind == tokens::LPAREN) ++parentheses;
      else if (token.kind == tokens::RPAREN) --parentheses;
      else if (token.kind == tokens::LBRACKET) ++brackets;
      else if (token.kind == tokens::RBRACKET) --brackets;
      else if (token.kind == tokens::LBRACE) ++braces;
      else if (token.kind == tokens::RBRACE) --braces;
      else if (token.kind == tokens::COMMA && parentheses == 1 && brackets == 0 && braces == 0)
        ++active_parameter;
      if (parentheses <= 0) break;
    }
    const semantic::indexed_symbol *callable = nullptr;
    for (const auto &reference : index_.references())
      if (reference.kind == semantic::reference_kind::call &&
          reference.location.bytes.begin >= static_cast<source::byte_offset>(selected->range.begin) &&
          reference.location.bytes.end <= static_cast<source::byte_offset>(selected->callee_end))
      {
        callable = index_.find(reference.target);
        if (workspace_)
          for (const auto &external : workspace_->external_references())
            if (external.location == reference.location)
              if (const auto *resolved = workspace_->find(external.target)) callable = resolved;
        break;
      }
    std::vector<std::string> parameter_names;
    std::vector<std::string> generic_names;
    std::vector<std::string> builtin_documentation;
    if (!callable && selected->callee_end > selected->range.begin)
      if (const auto builtin = builtin_member_at(document_, tokens_, index_,
                                                 static_cast<source::byte_offset>(selected->callee_end - 1));
          builtin && builtin->member.callable)
      {
        parameter_names = {"value"};
        builtin_documentation = {std::string(builtin->member.documentation)};
      }
    if (callable)
    {
      const auto find_parameters = [&](const semantic::semantic_index &index)
      {
        for (const auto &record : index.parameters())
          if (record.callable == callable->id)
          {
            parameter_names = record.names;
            generic_names = record.generic_names;
            break;
          }
      };
      find_parameters(index_);
      if (parameter_names.empty() && workspace_)
        for (const auto &module : workspace_->modules()) find_parameters(module.index);
    }
    std::vector<signature_variant> alternatives;
    const auto append_signature = [&](const semantic::semantic_index &index, const semantic::symbol_id &id)
    {
      const auto *symbol = index.find(id);
      if (!symbol) return;
      for (const auto &record : index.parameters())
        if (record.callable == id)
        {
          std::string display = symbol->name + "(";
          for (std::size_t i = 0; i < record.types.size(); ++i)
          {
            if (i) display += ", ";
            if (i < record.names.size()) display += record.names[i] + ": ";
            display += record.types[i];
          }
          display += "): " + record.result_type;
          alternatives.push_back({id, std::move(display), record.names, record.types,
                                  record.result_type, symbol->documentation});
          break;
        }
    };
    if (callable)
    {
      const auto collect = [&](const semantic::semantic_index &index)
      {
        if (!index.find(callable->id)) return;
        for (const auto &overload : index.overloads())
          if (std::find(overload.candidates.begin(), overload.candidates.end(), callable->id) !=
              overload.candidates.end())
          {
            for (const auto &candidate : overload.candidates) append_signature(index, candidate);
            return;
          }
        append_signature(index, callable->id);
      };
      collect(index_);
      if (alternatives.empty() && workspace_)
        for (const auto &module : workspace_->modules()) collect(module.index);
    }
    std::size_t active_signature = 0;
    const auto canonical = [](const std::string_view type) -> std::string_view
    {
      if (type == "Int") return "Int64";
      if (type == "Float") return "Float64";
      return type;
    };
    for (std::size_t candidate = 0; candidate < alternatives.size(); ++candidate)
    {
      if (alternatives[candidate].parameter_types.size() != selected->parameter_types.size()) continue;
      bool matches = true;
      for (std::size_t i = 0; i < selected->parameter_types.size(); ++i)
        matches &= canonical(alternatives[candidate].parameter_types[i]) ==
                   canonical(selected->parameter_types[i]);
      if (matches) { active_signature = candidate; break; }
    }
    if (!alternatives.empty()) parameter_names = alternatives[active_signature].parameter_names;
    std::string label = std::string(document_.text().substr(
        static_cast<std::size_t>(selected->range.begin),
        static_cast<std::size_t>(selected->callee_end - selected->range.begin)));
    label += "(";
    for (std::size_t i = 0; i < selected->parameter_types.size(); ++i)
    {
      if (i > 0) label += ", ";
      if (i < parameter_names.size()) label += parameter_names[i] + ": ";
      label += selected->parameter_types[i];
    }
    label += "): " + selected->result_type;
    return result<signature_information>(
        document_.version(), diagnostics::result_state::complete,
        signature_information{{document_.identity().id,
                               {static_cast<source::byte_offset>(selected->range.begin),
                                static_cast<source::byte_offset>(selected->range.end)}},
                              std::move(label), selected->parameter_types, std::move(parameter_names),
                              std::move(generic_names), alternatives.empty() ?
                                  (callable ? callable->documentation : builtin_documentation) :
                                  alternatives[active_signature].documentation,
                              selected->result_type,
                              active_parameter, std::move(alternatives), active_signature});
  }

  auto document_queries::selection_ranges(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    using ranges = std::vector<source::source_range>;
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return result<ranges>(document_.version(), diagnostics::result_state::stale);
    if (!document_.to_utf16(offset))
      return result<ranges>(document_.version(), diagnostics::result_state::incomplete);

    const auto id = document_.identity().id;
    std::vector<source::byte_range> candidates;
    const auto add = [&](const source::byte_range range)
    {
      if (range.begin <= offset && (offset < range.end ||
                                    (range.begin == range.end && offset == range.end)))
        candidates.push_back(range);
    };
    const auto add_trivia = [&](const std::vector<syntax::trivia> &entries)
    {
      for (const auto &entry : entries) add(entry.range);
    };
    struct opening { int kind; source::byte_offset begin; };
    std::vector<opening> delimiters;
    for (const auto &token : tokens_)
    {
      add_trivia(token.leading_trivia);
      add(token.range);
      if (token.kind == tokens::LPAREN || token.kind == tokens::LBRACKET ||
          token.kind == tokens::LBRACE)
        delimiters.push_back({token.kind, token.range.begin});
      else if (token.kind == tokens::RPAREN || token.kind == tokens::RBRACKET ||
               token.kind == tokens::RBRACE)
      {
        const int expected = token.kind == tokens::RPAREN ? tokens::LPAREN :
                             token.kind == tokens::RBRACKET ? tokens::LBRACKET : tokens::LBRACE;
        if (!delimiters.empty() && delimiters.back().kind == expected)
        {
          add({delimiters.back().begin, token.range.end});
          delimiters.pop_back();
        }
        else delimiters.clear();
      }
    }
    add_trivia(trailing_trivia_);
    if (tree_)
      for (const auto &statement : tree_->statements)
        if (statement->range.begin >= 0 && statement->range.end >= statement->range.begin)
          add({static_cast<source::byte_offset>(statement->range.begin),
               static_cast<source::byte_offset>(statement->range.end)});
    if (types_)
      for (const auto &expression : types_->expressions)
        if (expression.range.begin >= 0 && expression.range.end >= expression.range.begin)
          add({static_cast<source::byte_offset>(expression.range.begin),
               static_cast<source::byte_offset>(expression.range.end)});
    const auto document_end = static_cast<source::byte_offset>(document_.text().size());
    candidates.push_back({0, document_end});
    std::sort(candidates.begin(), candidates.end(), [](const auto left, const auto right)
    {
      const auto left_width = left.end - left.begin;
      const auto right_width = right.end - right.begin;
      if (left_width != right_width) return left_width < right_width;
      return left.begin > right.begin;
    });
    ranges chain;
    for (const auto candidate : candidates)
    {
      if (!chain.empty() && (candidate.begin > chain.back().bytes.begin ||
                             candidate.end < chain.back().bytes.end ||
                             candidate == chain.back().bytes)) continue;
      chain.push_back({id, candidate});
    }
    return result<ranges>(document_.version(), diagnostics::result_state::complete,
                          std::move(chain));
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
        if (symbol.scope_id != 0 && symbol.owner_type.empty()) continue;
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
