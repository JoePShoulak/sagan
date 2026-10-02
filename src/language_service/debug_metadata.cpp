#include "debug_metadata.hpp"

#include <algorithm>
#include <unordered_map>

namespace sagan::language_service
{
  namespace
  {
    auto range_for(const source::document_id id, const parser::span span) -> source::source_range
    {
      return {id, {static_cast<source::byte_offset>(std::max(0, span.begin)),
                   static_cast<source::byte_offset>(std::max(span.begin, span.end))}};
    }

    auto value_representation(const semantic::symbol_kind kind) -> debug_value_representation
    {
      switch (kind)
      {
        case semantic::symbol_kind::variable:
        case semantic::symbol_kind::parameter:
        case semantic::symbol_kind::loop_binding:
        case semantic::symbol_kind::match_binding:
          return debug_value_representation::shared_value;
        case semantic::symbol_kind::constant:
          return debug_value_representation::direct_value;
        default:
          return debug_value_representation::unavailable;
      }
    }
  }

  auto derive_debug_metadata(const source::document_snapshot &document,
                             const semantic::semantic_model &model,
                             const semantic::type_model &types,
                             const codegen::generated_cpp &generated,
                             const source::source_provider *provider) -> debug_metadata
  {
    debug_metadata result;
    result.document = document.identity();
    result.version = document.version();
    std::unordered_map<std::string, source::document_id> document_ids;
    const auto id_for = [&](const std::optional<std::filesystem::path> &path) -> source::document_id
    {
      if (!provider || !path) return document.identity().id;
      const auto key = path->lexically_normal().generic_string();
      if (const auto found = document_ids.find(key); found != document_ids.end()) return found->second;
      const auto loaded = provider->read_path(*path);
      const auto id = loaded ? loaded.value->identity().id : document.identity().id;
      document_ids.insert_or_assign(key, id);
      return id;
    };
    for (const auto &entry : generated.mappings)
    {
      if (entry.breakpoint)
        result.breakpoints.push_back({range_for(id_for(entry.source_path), entry.source),
                                      entry.source_path, entry.generated_begin,
                                      entry.generated_end, entry.generated_function});
      else if (!entry.generated_function.empty())
      {
        debug_function function{{}, entry.generated_function,
                                range_for(id_for(entry.source_path), entry.source), entry.source_path,
                                entry.generated_begin, entry.generated_end};
        for (const auto &scope : model.scopes)
          for (const auto &symbol : scope.symbols)
            if ((symbol.kind == semantic::symbol_kind::function ||
                 symbol.kind == semantic::symbol_kind::method ||
                 symbol.kind == semantic::symbol_kind::constructor) &&
                symbol.declaration.begin == entry.source.begin &&
                symbol.declaration.end == entry.source.end &&
                (entry.generated_function == codegen::generated_identifier(symbol.name) ||
                 entry.generated_function.ends_with("::" + codegen::generated_identifier(symbol.name))))
              function.symbol = symbol.id;
        result.functions.push_back(std::move(function));
      }
    }
    std::vector<std::optional<std::filesystem::path>> scope_paths(model.scopes.size());
    // Linked modules may supply their own provenance below, but the entry
    // document's top-level bindings still belong to the entry source file.
    if (!scope_paths.empty()) scope_paths.front() = document.identity().canonical_path;
    for (const auto &scope : model.scopes)
      for (const auto &symbol : scope.symbols)
        if (symbol.kind == semantic::symbol_kind::function ||
            symbol.kind == semantic::symbol_kind::method ||
            symbol.kind == semantic::symbol_kind::constructor)
          for (const auto &function : result.functions)
            if (function.symbol && *function.symbol == symbol.id)
              for (const auto &candidate : model.scopes)
                if (candidate.parent == symbol.scope_id &&
                    candidate.label == "function " + symbol.name &&
                    candidate.id < scope_paths.size())
                  scope_paths[candidate.id] = function.source_path;
    for (const auto &scope : model.scopes)
      if (scope.id < scope_paths.size() && !scope_paths[scope.id] &&
          scope.parent < scope_paths.size())
        scope_paths[scope.id] = scope_paths[scope.parent];
    for (const auto &scope : model.scopes)
      result.scopes.push_back({scope.id, scope.parent, scope.label,
                               range_for(id_for(scope.id < scope_paths.size()
                                                    ? scope_paths[scope.id] : std::nullopt),
                                         scope.range),
                               scope.id < scope_paths.size() ? scope_paths[scope.id] : std::nullopt});
    for (const auto &scope : model.scopes)
      for (const auto &symbol : scope.symbols)
      {
        const auto representation = value_representation(symbol.kind);
        if (symbol.origin != semantic::symbol_origin::source ||
            representation == debug_value_representation::unavailable) continue;
        std::string type;
        for (const auto &declaration : types.declarations)
          if (declaration.name == symbol.name &&
              declaration.range.begin == symbol.declaration.begin &&
              declaration.range.end == symbol.declaration.end)
          { type = declaration.type; break; }
        const auto lifetime = symbol.scope_id < model.scopes.size()
                                  ? model.scopes[symbol.scope_id].range : symbol.declaration;
        const auto path = symbol.scope_id < scope_paths.size()
                              ? scope_paths[symbol.scope_id] : std::nullopt;
        if (provider && !path) continue;
        result.variables.push_back({symbol.id, symbol.name,
                                    codegen::generated_identifier(symbol.name), type,
                                    symbol.scope_id, range_for(id_for(path), lifetime), path,
                                    representation, false});
      }
    for (const auto &resolution : model.resolutions)
    {
      const auto variable = std::find_if(result.variables.begin(), result.variables.end(),
                                         [&](const auto &entry) { return entry.symbol == resolution.target; });
      if (variable == result.variables.end()) continue;
      const auto expression = variable->representation == debug_value_representation::shared_value
                                  ? "*" + variable->generated_name : variable->generated_name;
      result.expression_hooks.push_back({variable->symbol,
                                         range_for(variable->lifetime.document, resolution.use),
                                         expression, variable->available_in_optimized});
    }
    std::sort(result.breakpoints.begin(), result.breakpoints.end(), [](const auto &left, const auto &right)
    {
      if (left.source.bytes.begin != right.source.bytes.begin)
        return left.source.bytes.begin < right.source.bytes.begin;
      return left.generated_begin < right.generated_begin;
    });
    return result;
  }

  auto source_for_stack_frame(const debug_metadata &metadata,
                              const codegen::generated_cpp &generated,
                              const std::string_view generated_function,
                              const std::size_t one_based_line,
                              const std::size_t one_based_column)
    -> std::optional<source::source_range>
  {
    const auto offset = codegen::generated_offset(generated.text, one_based_line, one_based_column);
    if (!offset) return {};
    const debug_breakpoint *narrowest = nullptr;
    for (const auto &breakpoint : metadata.breakpoints)
      if (breakpoint.generated_function == generated_function &&
          breakpoint.generated_begin <= *offset && *offset < breakpoint.generated_end &&
          (!narrowest || breakpoint.generated_end - breakpoint.generated_begin <
                          narrowest->generated_end - narrowest->generated_begin))
        narrowest = &breakpoint;
    if (narrowest) return narrowest->source;
    for (const auto &function : metadata.functions)
      if (function.generated_name == generated_function &&
          function.generated_begin <= *offset && *offset < function.generated_end)
        return function.source;
    return {};
  }
}
