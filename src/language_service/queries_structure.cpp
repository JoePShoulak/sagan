#include "queries.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <functional>

namespace sagan::language_service
{
  namespace
  {
    template<class T>
    auto answer(const source::document_version version, const diagnostics::result_state state,
                std::optional<T> value = {}) -> diagnostics::analysis_result<T>
    {
      return {state, std::move(value), {}, version};
    }

    auto is_type(const semantic::indexed_symbol &symbol) -> bool
    {
      return symbol.kind == semantic::symbol_kind::type;
    }

    auto nominal_base(const std::string_view type) -> std::string_view
    {
      const auto end = type.find_first_of("< ([");
      return type.substr(0, end);
    }
  }

  auto document_queries::type_definitions(const source::byte_offset offset) const
    -> diagnostics::analysis_result<std::vector<source::source_range>>
  {
    using locations = std::vector<source::source_range>;
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return answer<locations>(document_.version(), selected.state);
    if (!document_.to_utf16(offset))
      return answer<locations>(document_.version(), diagnostics::result_state::incomplete);
    if (selected.value && selected.value->kind == semantic::symbol_kind::type)
      return definitions(offset);
    const auto type = resolved_type(offset);
    if (!type.value) return answer<locations>(document_.version(), diagnostics::result_state::complete, locations{});
    const auto base = nominal_base(*type.value);
    locations matches;
    const auto collect = [&](const semantic::semantic_index &index)
    {
      for (const auto &symbol : index.symbols())
        if (is_type(symbol) && symbol.origin == semantic::symbol_origin::source && symbol.name == base)
          matches.push_back(symbol.declaration);
    };
    collect(index_);
    if (matches.empty() && workspace_)
      for (const auto &binding : index_.symbols())
        if (binding.origin == semantic::symbol_origin::imported && binding.name == base)
          for (const auto &imported : workspace_->imports())
            if (imported.binding == binding.id)
              for (const auto &target : imported.targets)
                if (const auto *symbol = workspace_->find(target); symbol && is_type(*symbol))
                  matches.push_back(symbol->declaration);
    // A display type alone cannot disambiguate two imported nominal types.
    if (matches.size() != 1) matches.clear();
    return answer<locations>(document_.version(), diagnostics::result_state::complete, std::move(matches));
  }

  auto document_queries::type_hierarchy(const source::byte_offset offset) const
    -> diagnostics::analysis_result<type_hierarchy_information>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return answer<type_hierarchy_information>(document_.version(), selected.state);
    if (!selected.value || selected.value->kind != semantic::symbol_kind::type)
      return answer<type_hierarchy_information>(document_.version(), diagnostics::result_state::complete);
    type_hierarchy_information hierarchy{*selected.value, {}, {}};
    const auto lookup = [&](const semantic::symbol_id &id) -> const semantic::indexed_symbol *
    {
      if (const auto *local = index_.find(id)) return local;
      return workspace_ ? workspace_->find(id) : nullptr;
    };
    const auto collect = [&](const semantic::semantic_index &index)
    {
      for (const auto &edge : index.conformances())
      {
        const auto *related = lookup(edge.implementer == selected.value->id ? edge.interface : edge.implementer);
        if (!related) continue;
        auto &destination = edge.implementer == selected.value->id ? hierarchy.supertypes : hierarchy.subtypes;
        if (edge.implementer != selected.value->id && edge.interface != selected.value->id) continue;
        destination.push_back({related->id, related->kind, related->origin,
                               related->declaration, related->declaration, related->name});
      }
    };
    if (workspace_)
      for (const auto &module : workspace_->modules()) collect(module.index);
    else collect(index_);
    const auto ordered = [](auto &values)
    {
      std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
      {
        if (left.name != right.name) return left.name < right.name;
        return left.id.value < right.id.value;
      });
      values.erase(std::unique(values.begin(), values.end(), [](const auto &left, const auto &right)
      { return left.id == right.id; }), values.end());
    };
    ordered(hierarchy.supertypes);
    ordered(hierarchy.subtypes);
    return answer<type_hierarchy_information>(document_.version(), diagnostics::result_state::complete,
                                              std::move(hierarchy));
  }

  auto document_queries::inlay_hints(const source::byte_range range) const
    -> diagnostics::analysis_result<std::vector<inlay_hint>>
  {
    using hints = std::vector<inlay_hint>;
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return answer<hints>(document_.version(), diagnostics::result_state::stale);
    if (range.begin > range.end || !document_.to_utf16(range.begin) || !document_.to_utf16(range.end))
      return answer<hints>(document_.version(), diagnostics::result_state::incomplete);
    if (!tree_ || !types_)
      return answer<hints>(document_.version(), diagnostics::result_state::incomplete);
    hints values;
    std::function<void(const parser::statement &)> visit = [&](const parser::statement &statement)
    {
      if (const auto *binding = dynamic_cast<const parser::let_declaration *>(&statement))
      {
        if (binding->type_name || !binding->initializer) return;
        const auto typed = std::find_if(types_->declarations.begin(), types_->declarations.end(),
                                        [&](const auto &entry)
                                        { return entry.range.begin == statement.range.begin &&
                                                 entry.range.end == statement.range.end &&
                                                 entry.name == binding->name; });
        if (typed == types_->declarations.end() || typed->type.empty() || typed->type == "Unknown") return;
        for (const auto &token : tokens_)
          if (token.kind == tokens::IDENTIFIER &&
              token.range.begin >= static_cast<source::byte_offset>(statement.range.begin) &&
              token.range.end <= static_cast<source::byte_offset>(statement.range.end) &&
              unicode::normalize_nfc(token.source_text) == binding->name)
          {
            if (range.begin <= token.range.end && token.range.end <= range.end)
            {
              const auto symbol = index_.symbol_at(token.range.begin);
              values.push_back({token.range.end, ": " + typed->type, symbol ? symbol->id : semantic::symbol_id{}});
            }
            break;
          }
      }
      else if (const auto *group = dynamic_cast<const parser::parallel_let_declaration *>(&statement))
      {
        for (const auto &binding : group->bindings)
        {
          if (binding.type_name) continue;
          const auto typed = std::find_if(types_->declarations.begin(), types_->declarations.end(),
                                          [&](const auto &entry)
                                          { return entry.range.begin == binding.name_range.begin &&
                                                   entry.range.end == binding.name_range.end &&
                                                   entry.name == binding.name; });
          if (typed == types_->declarations.end() || typed->type.empty() || typed->type == "Unknown") continue;
          const auto end = static_cast<source::byte_offset>(binding.name_range.end);
          if (range.begin <= end && end <= range.end)
          {
            const auto symbol = index_.symbol_at(static_cast<source::byte_offset>(binding.name_range.begin));
            values.push_back({end, ": " + typed->type, symbol ? symbol->id : semantic::symbol_id{}});
          }
        }
      }
      else if (const auto *block = dynamic_cast<const parser::block_statement *>(&statement))
        for (const auto &child : block->statements) visit(*child);
      else if (const auto *function = dynamic_cast<const parser::function_declaration *>(&statement))
      {
        if (function->body) visit(*function->body);
      }
      else if (const auto *type = dynamic_cast<const parser::type_declaration *>(&statement))
        for (const auto &member : type->members) visit(*member);
      else if (const auto *branch = dynamic_cast<const parser::if_statement *>(&statement))
      {
        visit(*branch->then_branch);
        if (branch->else_branch) visit(*branch->else_branch);
      }
      else if (const auto *loop = dynamic_cast<const parser::condition_loop_statement *>(&statement))
        visit(*loop->body);
      else if (const auto *loop = dynamic_cast<const parser::for_statement *>(&statement))
        visit(*loop->body);
      else if (const auto *match = dynamic_cast<const parser::match_statement *>(&statement))
        for (const auto &branch : match->cases) visit(*branch.body);
      else if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&statement))
      {
        visit(*hope->protected_body);
        for (const auto &handler : hope->handlers) visit(*handler.body);
        if (hope->cleanup) visit(*hope->cleanup);
      }
    };
    for (const auto &statement : tree_->statements) visit(*statement);
    for (const auto &call : types_->calls)
    {
      const syntax::lossless_token *opening = nullptr;
      for (const auto &token : tokens_)
        if (token.kind == tokens::LPAREN && token.range.begin >= static_cast<source::byte_offset>(call.callee_end) &&
            token.range.end <= static_cast<source::byte_offset>(call.range.end))
        {
          opening = &token;
          break;
        }
      if (!opening) continue;
      int parentheses = 1;
      int brackets = 0;
      int braces = 0;
      bool awaiting_argument = true;
      std::size_t argument_index = 0;
      for (const auto &token : tokens_)
      {
        if (token.range.begin < opening->range.end) continue;
        if (token.range.begin >= static_cast<source::byte_offset>(call.range.end)) break;
        if (token.kind == tokens::RPAREN && parentheses == 1) break;
        if (token.kind == tokens::COMMA && parentheses == 1 && brackets == 0 && braces == 0)
        {
          ++argument_index;
          awaiting_argument = true;
          continue;
        }
        if (awaiting_argument && parentheses == 1 && brackets == 0 && braces == 0 &&
            token.kind != tokens::NEWLINE)
        {
          awaiting_argument = false;
          const auto signature = signature_help(token.range.begin);
          if (signature.value && signature.value->call.bytes.begin ==
                                     static_cast<source::byte_offset>(call.range.begin) &&
              argument_index < signature.value->parameter_names.size())
          {
            const auto &name = signature.value->parameter_names[argument_index];
            if (!name.empty() && token.source_text != name &&
                range.begin <= token.range.begin && token.range.begin <= range.end)
              values.push_back({token.range.begin, name + ":", {}});
          }
        }
        if (token.kind == tokens::LPAREN) ++parentheses;
        else if (token.kind == tokens::RPAREN) --parentheses;
        else if (token.kind == tokens::LBRACKET) ++brackets;
        else if (token.kind == tokens::RBRACKET) --brackets;
        else if (token.kind == tokens::LBRACE) ++braces;
        else if (token.kind == tokens::RBRACE) --braces;
      }
    }
    std::sort(values.begin(), values.end(), [](const auto &left, const auto &right)
    { return left.position < right.position; });
    return answer<hints>(document_.version(), diagnostics::result_state::complete, std::move(values));
  }

  auto document_queries::call_hierarchy(const source::byte_offset offset) const
    -> diagnostics::analysis_result<call_hierarchy_information>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return answer<call_hierarchy_information>(document_.version(), selected.state);
    if (!selected.value) return answer<call_hierarchy_information>(document_.version(), selected.state);
    const auto callable = [](const semantic::symbol_kind kind)
    {
      return kind == semantic::symbol_kind::function || kind == semantic::symbol_kind::method ||
             kind == semantic::symbol_kind::constructor || kind == semantic::symbol_kind::enum_constructor;
    };
    if (!callable(selected.value->kind))
      return answer<call_hierarchy_information>(document_.version(), selected.state);
    call_hierarchy_information hierarchy{*selected.value, {}, {}};
    const auto lookup = [&](const semantic::symbol_id &id) -> const semantic::indexed_symbol *
    {
      if (const auto *local = index_.find(id)) return local;
      return workspace_ ? workspace_->find(id) : nullptr;
    };
    const auto occurrence = [](const semantic::indexed_symbol &symbol) -> symbol_occurrence
    {
      return {symbol.id, symbol.kind, symbol.origin, symbol.declaration, symbol.declaration, symbol.name};
    };
    const auto owner = [&](const semantic::semantic_index &index,
                           const source::source_range site) -> const semantic::indexed_symbol *
    {
      const semantic::indexed_symbol *found = nullptr;
      for (const auto &candidate : index.symbols())
        if (candidate.origin == semantic::symbol_origin::source && callable(candidate.kind) &&
            candidate.declaration.bytes.begin <= site.bytes.begin &&
            site.bytes.end <= candidate.declaration.bytes.end &&
            (!found || candidate.declaration.bytes.end - candidate.declaration.bytes.begin <
                           found->declaration.bytes.end - found->declaration.bytes.begin)) found = &candidate;
      return found;
    };
    const auto append = [&](const semantic::semantic_index &index, const semantic::symbol_id &target,
                            const source::source_range site)
    {
      const auto *caller = owner(index, site);
      const auto *callee = lookup(target);
      if (!caller || !callee || !callable(callee->kind) ||
          callee->origin == semantic::symbol_origin::builtin) return;
      auto &destination = caller->id == selected.value->id ? hierarchy.outgoing : hierarchy.incoming;
      if (caller->id != selected.value->id && callee->id != selected.value->id) return;
      auto existing = std::find_if(destination.begin(), destination.end(), [&](const auto &edge)
      { return edge.caller.id == caller->id && edge.callee.id == callee->id; });
      if (existing == destination.end())
        destination.push_back({occurrence(*caller), occurrence(*callee), {site}});
      else existing->call_sites.push_back(site);
    };
    const auto collect = [&](const semantic::semantic_index &index)
    {
      for (const auto &reference : index.references())
        if (reference.kind == semantic::reference_kind::call)
          if (const auto *target = lookup(reference.target);
              target && target->origin != semantic::symbol_origin::imported)
            append(index, reference.target, reference.location);
      for (const auto &member : index.member_resolutions())
      {
        if (member.candidates.size() != 1) continue;
        const auto called = std::any_of(index.unresolved_members().begin(), index.unresolved_members().end(),
                                        [&](const auto &reference)
                                        {
                                          return reference.kind == semantic::reference_kind::call &&
                                                 reference.use.begin == static_cast<int>(member.use.bytes.begin) &&
                                                 reference.use.end == static_cast<int>(member.use.bytes.end);
                                        });
        if (called) append(index, member.candidates.front(), member.use);
      }
    };
    if (workspace_)
    {
      for (const auto &module : workspace_->modules()) collect(module.index);
      for (const auto &reference : workspace_->external_references())
        if (reference.kind == semantic::reference_kind::call)
          for (const auto &module : workspace_->modules())
            if (module.index.document().id == reference.location.document)
              append(module.index, reference.target, reference.location);
    }
    else collect(index_);
    const auto ordered = [](auto &edges)
    {
      for (auto &edge : edges)
      {
        std::sort(edge.call_sites.begin(), edge.call_sites.end(), [](const auto &left, const auto &right)
        {
          if (left.document.value != right.document.value)
            return left.document.value < right.document.value;
          return left.bytes.begin < right.bytes.begin;
        });
        edge.call_sites.erase(std::unique(edge.call_sites.begin(), edge.call_sites.end()), edge.call_sites.end());
      }
      std::sort(edges.begin(), edges.end(), [](const auto &left, const auto &right)
      {
        if (left.caller.name != right.caller.name) return left.caller.name < right.caller.name;
        if (left.callee.name != right.callee.name) return left.callee.name < right.callee.name;
        return left.call_sites.front().bytes.begin < right.call_sites.front().bytes.begin;
      });
    };
    ordered(hierarchy.incoming);
    ordered(hierarchy.outgoing);
    return answer<call_hierarchy_information>(document_.version(), diagnostics::result_state::complete,
                                              std::move(hierarchy));
  }

  auto document_queries::documentation_at(const source::byte_offset offset) const
    -> diagnostics::analysis_result<documentation_entry>
  {
    const auto selected = symbol_at(offset);
    if (selected.state != diagnostics::result_state::complete)
      return answer<documentation_entry>(document_.version(), selected.state);
    if (!selected.value) return answer<documentation_entry>(document_.version(), selected.state);
    const semantic::indexed_symbol *symbol = index_.find(selected.value->id);
    std::string module = "local";
    if (!symbol && workspace_)
      for (const auto &entry : workspace_->modules())
        if (const auto *found = entry.index.find(selected.value->id))
        {
          symbol = found;
          module = entry.name;
          break;
        }
    if (!symbol) return answer<documentation_entry>(document_.version(), selected.state);
    if (symbol->origin == semantic::symbol_origin::builtin) module = "sagan/core";
    return answer<documentation_entry>(document_.version(), selected.state,
                                       documentation_for(*symbol, std::move(module)));
  }

  auto document_queries::context_at(const source::byte_offset offset) const
    -> diagnostics::analysis_result<position_context>
  {
    if (document_.version() != index_.version() || document_.identity().id != index_.document().id)
      return answer<position_context>(document_.version(), diagnostics::result_state::stale);
    if (!document_.to_utf16(offset))
      return answer<position_context>(document_.version(), diagnostics::result_state::incomplete);
    position_context context;
    const auto *declaration = static_cast<const semantic::indexed_symbol *>(nullptr);
    for (const auto &symbol : index_.symbols())
      if (symbol.origin == semantic::symbol_origin::source &&
          symbol.declaration.bytes.begin <= offset && offset < symbol.declaration.bytes.end &&
          (!declaration || symbol.declaration.bytes.end - symbol.declaration.bytes.begin <
                               declaration->declaration.bytes.end - declaration->declaration.bytes.begin))
        declaration = &symbol;
    if (declaration) context.containing_declaration = declaration->declaration;
    if (model_ && !model_->scopes.empty())
    {
      const semantic::scope *innermost = nullptr;
      for (const auto &scope : model_->scopes)
        if (scope.range.begin >= 0 && scope.range.begin <= static_cast<int>(offset) &&
            static_cast<int>(offset) < scope.range.end &&
            (!innermost || scope.range.end - scope.range.begin <
                               innermost->range.end - innermost->range.begin)) innermost = &scope;
      while (innermost)
      {
        context.scopes.push_back({innermost->id, innermost->label,
                                  {document_.identity().id,
                                   {static_cast<source::byte_offset>(innermost->range.begin),
                                    static_cast<source::byte_offset>(innermost->range.end)}}});
        if (innermost->id == 0 || innermost->parent >= model_->scopes.size()) break;
        innermost = &model_->scopes[innermost->parent];
      }
    }
    if (types_)
    {
      const semantic::typed_expression *best = nullptr;
      for (const auto &expression : types_->expressions)
        if (expression.range.begin >= 0 && expression.range.begin <= static_cast<int>(offset) &&
            static_cast<int>(offset) < expression.range.end &&
            (!best || expression.range.end - expression.range.begin <
                          best->range.end - best->range.begin)) best = &expression;
      if (best)
      {
        context.expression = source::source_range{document_.identity().id,
            {static_cast<source::byte_offset>(best->range.begin),
             static_cast<source::byte_offset>(best->range.end)}};
        context.expression_type = best->type;
      }
    }
    return answer<position_context>(document_.version(), model_ ? diagnostics::result_state::complete :
                                                       diagnostics::result_state::incomplete,
                                    std::move(context));
  }
}
