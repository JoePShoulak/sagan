#include "workspace_index.hpp"

#include "analyzer.hpp"
#include "../parser/lex.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace semantic
{
  namespace
  {
    auto parse(const sagan::source::document_snapshot &document) -> parser::program
    {
      parser::tokenizer lexer(parser::programText{std::string(document.text())}, get_token);
      std::vector<parser::token> tokens;
      while (auto next = lexer.next()) tokens.push_back(std::move(*next));
      return parser::syntax_parser(std::move(tokens)).parse();
    }

    auto key(const std::string &module, const std::string &name) -> std::string
    {
      return module + '\n' + name;
    }

    auto top_level(const semantic_index &index, const std::string &name) -> std::vector<symbol_id>
    {
      std::vector<symbol_id> result;
      for (const auto &entry : index.symbols())
        if (entry.scope_id == 0 && entry.origin == symbol_origin::source && entry.name == name)
          result.push_back(entry.id);
      return result;
    }

    auto import_binding(const semantic_index &index, const std::string &name) -> const indexed_symbol *
    {
      const auto found = std::find_if(index.symbols().begin(), index.symbols().end(), [&](const auto &entry)
      {
        return entry.scope_id == 0 && entry.origin == symbol_origin::imported && entry.name == name;
      });
      return found == index.symbols().end() ? nullptr : &*found;
    }
  }

  workspace_semantic_index::workspace_semantic_index(
      std::vector<indexed_module> modules, std::vector<import_link> imports,
      std::unordered_map<std::string, std::vector<symbol_id>> exports,
      std::vector<exported_symbol> exported_symbols,
      std::vector<external_reference> external_references)
      : modules_(std::move(modules)), imports_(std::move(imports)), exports_(std::move(exports)),
        exported_symbols_(std::move(exported_symbols)),
        external_references_(std::move(external_references)) {}

  auto workspace_semantic_index::modules() const -> const std::vector<indexed_module> & { return modules_; }
  auto workspace_semantic_index::imports() const -> const std::vector<import_link> & { return imports_; }
  auto workspace_semantic_index::external_references() const -> const std::vector<external_reference> &
  {
    return external_references_;
  }

  auto workspace_semantic_index::find(const symbol_id &id) const -> const indexed_symbol *
  {
    for (const auto &module : modules_)
      if (const auto *entry = module.index.find(id)) return entry;
    return nullptr;
  }

  auto workspace_semantic_index::definitions(const symbol_id &id) const
    -> std::vector<sagan::source::source_range>
  {
    for (const auto &imported : imports_)
      if (imported.binding == id && !imported.targets.empty())
      {
        std::vector<sagan::source::source_range> result;
        for (const auto &target : imported.targets)
          if (const auto *entry = find(target)) result.push_back(entry->declaration);
        return result;
      }
    if (const auto *entry = find(id)) return {entry->declaration};
    return {};
  }

  auto workspace_semantic_index::references_to(const symbol_id &id, const bool include_declaration) const
    -> std::vector<sagan::source::source_range>
  {
    std::vector<sagan::source::source_range> result;
    if (include_declaration)
      if (const auto *entry = find(id)) result.push_back(entry->declaration);
    for (const auto &module : modules_)
    {
      auto local = module.index.references_to(id);
      result.insert(result.end(), local.begin(), local.end());
    }
    for (const auto &reference : external_references_)
      if (reference.target == id) result.push_back(reference.location);
    for (const auto &imported : imports_)
      if (std::find(imported.targets.begin(), imported.targets.end(), id) != imported.targets.end())
        for (const auto &module : modules_)
        {
          auto external = module.index.references_to(imported.binding);
          result.insert(result.end(), external.begin(), external.end());
        }
    return result;
  }

  auto workspace_semantic_index::exported(const std::string &module, const std::string &public_name) const
    -> std::vector<symbol_id>
  {
    const auto found = exports_.find(key(module, public_name));
    return found == exports_.end() ? std::vector<symbol_id>{} : found->second;
  }

  auto workspace_semantic_index::exported_symbols() const -> std::vector<exported_symbol>
  {
    auto result = exported_symbols_;
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right)
    {
      if (left.public_name != right.public_name) return left.public_name < right.public_name;
      return left.module < right.module;
    });
    return result;
  }

  auto build_workspace_index(const modules::module_graph &graph,
                             const sagan::source::source_provider &source)
    -> workspace_semantic_index
  {
    std::vector<indexed_module> indexed;
    const std::string package = graph.package ? graph.package->name : "local";
    for (const auto &module : graph.modules)
    {
      auto document = source.read_path(module.path);
      if (!document) throw std::runtime_error(document.error ? document.error->message : "Could not read module");
      const auto tree = parse(*document.value);
      const auto model = analyze(tree, analysis_identity{package, module.name});
      indexed.push_back(indexed_module{module.name, build_index(*document.value, model)});
    }

    std::unordered_map<std::string, std::vector<symbol_id>> exports;
    std::vector<exported_symbol> exported_symbols;
    for (const auto &module : graph.modules)
    {
      const auto indexed_module = std::find_if(indexed.begin(), indexed.end(), [&](const auto &candidate)
      {
        return candidate.name == module.name;
      });
      for (const auto &entry : module.exports)
      {
        auto targets = top_level(indexed_module->index, entry.local_name);
        exports.insert_or_assign(key(module.name, entry.public_name), targets);
        exported_symbols.push_back(exported_symbol{
            module.name, entry.local_name, entry.public_name, std::move(targets),
            {indexed_module->index.document().id,
             {static_cast<sagan::source::byte_offset>(std::max(entry.declaration.begin, 0)),
              static_cast<sagan::source::byte_offset>(std::max(entry.declaration.end,
                                                                entry.declaration.begin))}}});
      }
    }

    std::vector<import_link> imports;
    for (const auto &module : graph.modules)
    {
      const auto indexed_module = std::find_if(indexed.begin(), indexed.end(), [&](const auto &candidate)
      {
        return candidate.name == module.name;
      });
      for (const auto &entry : module.imports)
      {
        const auto *binding = import_binding(indexed_module->index, entry.binding_name);
        if (!binding) continue;
        std::vector<symbol_id> targets;
        if (!entry.whole_module)
        {
          const auto found = exports.find(key(entry.module_name, entry.imported_name));
          if (found != exports.end()) targets = found->second;
        }
        imports.push_back(import_link{
            binding->id, entry.module_name, entry.imported_name, entry.binding_name,
            std::move(targets), entry.whole_module,
            {indexed_module->index.document().id,
             {static_cast<sagan::source::byte_offset>(std::max(entry.declaration.begin, 0)),
              static_cast<sagan::source::byte_offset>(std::max(entry.declaration.end,
                                                                entry.declaration.begin))}}});
      }
    }
    std::vector<external_reference> external_references;
    for (const auto &module : indexed)
      for (const auto &member : module.index.unresolved_members())
      {
        const auto linked_import = std::find_if(imports.begin(), imports.end(), [&](const auto &entry)
        {
          return entry.binding == member.receiver && entry.whole_module;
        });
        if (linked_import == imports.end()) continue;
        const auto found = exports.find(key(linked_import->source_module, member.member));
        if (found == exports.end()) continue;
        for (const auto &target : found->second)
          external_references.push_back(external_reference{
              target, {module.index.document().id,
                       {static_cast<sagan::source::byte_offset>(std::max(member.use.begin, 0)),
                        static_cast<sagan::source::byte_offset>(std::max(member.use.end, member.use.begin))}},
              member.kind});
      }
    return workspace_semantic_index(std::move(indexed), std::move(imports), std::move(exports),
                                    std::move(exported_symbols),
                                    std::move(external_references));
  }
}
