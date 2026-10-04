#include "workspace_index.hpp"

#include "analyzer.hpp"
#include "../parser/lex.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"
#include "../parser/tokens.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
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

    auto nominal_base(const std::string &type) -> std::string
    {
      return type.substr(0, type.find_first_of("< (["));
    }

    auto tokens(const sagan::source::document_snapshot &document) -> std::vector<parser::token>
    {
      parser::tokenizer lexer(parser::programText{std::string(document.text())}, get_token);
      std::vector<parser::token> result;
      while (auto next = lexer.next()) result.push_back(std::move(*next));
      return result;
    }

    auto imported_type(const semantic_index &index, const std::vector<import_link> &imports,
                       const std::vector<indexed_module> &modules, const parser::expression *initializer)
      -> std::optional<std::string>
    {
      const auto *call = dynamic_cast<const parser::call_expression *>(initializer);
      const auto *callee = call ? dynamic_cast<const parser::identifier_expression *>(call->callee.get()) : nullptr;
      if (!callee) return {};
      const auto *binding = index.symbol_at(static_cast<sagan::source::byte_offset>(callee->range.begin));
      if (!binding) return {};
      const auto imported = std::find_if(imports.begin(), imports.end(), [&](const auto &entry)
      { return entry.binding == binding->id; });
      if (imported == imports.end()) return {};
      for (const auto &target : imported->targets)
        for (const auto &module : modules)
        {
          const auto *symbol = module.index.find(target);
          if (!symbol) continue;
          if (symbol->kind == symbol_kind::type) return symbol->name;
          if (symbol->kind == symbol_kind::function)
            for (const auto &signature : module.index.parameters())
              if (signature.callable == target && !signature.result_type.empty() &&
                  signature.result_type != "Unknown") return nominal_base(signature.result_type);
        }
      return {};
    }

    auto declared_binding_types(const parser::program &tree, const semantic_index &index,
                                const std::vector<import_link> &imports,
                                const std::vector<indexed_module> &modules)
      -> std::unordered_map<std::string, std::string>
    {
      std::unordered_map<std::string, std::string> result;
      const auto record = [&](const std::string &name, const parser::span declaration,
                              const std::optional<std::string> &annotation,
                              const parser::expression *initializer)
      {
        auto type = annotation ? std::optional<std::string>{nominal_base(*annotation)} :
                                 imported_type(index, imports, modules, initializer);
        if (!type || type->empty()) return;
        for (const auto &symbol : index.symbols())
          if (symbol.origin == symbol_origin::source && symbol.name == name &&
              symbol.declaration.bytes.begin == static_cast<sagan::source::byte_offset>(declaration.begin) &&
              symbol.declaration.bytes.end == static_cast<sagan::source::byte_offset>(declaration.end))
            result.insert_or_assign(symbol.id.value, *type);
      };
      std::function<void(const parser::statement &)> visit;
      const auto block = [&](const parser::block_statement *value)
      {
        if (value) for (const auto &statement : value->statements) visit(*statement);
      };
      visit = [&](const parser::statement &statement)
      {
        if (const auto *value = dynamic_cast<const parser::let_declaration *>(&statement))
          record(value->name, value->range, value->type_name, value->initializer.get());
        else if (const auto *value = dynamic_cast<const parser::parallel_let_declaration *>(&statement))
          for (const auto &binding : value->bindings)
            record(binding.name, binding.name_range, binding.type_name, binding.initializer.get());
        else if (const auto *value = dynamic_cast<const parser::function_declaration *>(&statement))
        {
          for (const auto &parameter : value->parameters)
            record(parameter.name, value->range, parameter.type_name, nullptr);
          block(value->body.get());
        }
        else if (const auto *value = dynamic_cast<const parser::type_declaration *>(&statement))
          for (const auto &member : value->members) visit(*member);
        else if (const auto *value = dynamic_cast<const parser::block_statement *>(&statement)) block(value);
        else if (const auto *value = dynamic_cast<const parser::if_statement *>(&statement))
        { block(value->then_branch.get()); if (value->else_branch) visit(*value->else_branch); }
        else if (const auto *value = dynamic_cast<const parser::condition_loop_statement *>(&statement))
          block(value->body.get());
        else if (const auto *value = dynamic_cast<const parser::for_statement *>(&statement))
          block(value->body.get());
        else if (const auto *value = dynamic_cast<const parser::match_statement *>(&statement))
          for (const auto &branch : value->cases) block(branch.body.get());
        else if (const auto *value = dynamic_cast<const parser::hope_statement *>(&statement))
        {
          block(value->protected_body.get());
          for (const auto &handler : value->handlers) block(handler.body.get());
          block(value->cleanup.get());
        }
      };
      for (const auto &statement : tree.statements) visit(*statement);
      return result;
    }
  }

  workspace_semantic_index::workspace_semantic_index(
      std::vector<indexed_module> modules, std::vector<import_link> imports,
      std::unordered_map<std::string, std::vector<symbol_id>> exports,
      std::vector<exported_symbol> exported_symbols,
      std::vector<external_reference> external_references,
      std::unordered_map<std::string, std::string> binding_types)
      : modules_(std::move(modules)), imports_(std::move(imports)), exports_(std::move(exports)),
        exported_symbols_(std::move(exported_symbols)),
        external_references_(std::move(external_references)), binding_types_(std::move(binding_types)) {}

  auto workspace_semantic_index::modules() const -> const std::vector<indexed_module> & { return modules_; }
  auto workspace_semantic_index::imports() const -> const std::vector<import_link> & { return imports_; }
  auto workspace_semantic_index::external_references() const -> const std::vector<external_reference> &
  {
    return external_references_;
  }

  auto workspace_semantic_index::declared_type(const symbol_id &binding) const -> std::optional<std::string>
  {
    const auto found = binding_types_.find(binding.value);
    if (found == binding_types_.end()) return {};
    return found->second;
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
    std::unordered_map<std::string, std::string> binding_types;
    for (const auto &module : indexed)
    {
      const auto document = source.read(module.index.document().uri);
      if (!document) continue;
      const auto tree = parse(*document.value);
      const auto module_binding_types = declared_binding_types(tree, module.index, imports, indexed);
      binding_types.insert(module_binding_types.begin(), module_binding_types.end());
      const auto source_tokens = tokens(*document.value);
      for (std::size_t token_index = 0; token_index + 2 < source_tokens.size(); ++token_index)
      {
        const auto &receiver_token = source_tokens[token_index];
        const auto &separator = source_tokens[token_index + 1];
        const auto &member_token = source_tokens[token_index + 2];
        if (receiver_token.id != tokens::IDENTIFIER && receiver_token.id != tokens::METHOD_IDENTIFIER) continue;
        if (separator.id != tokens::DOT && separator.id != tokens::SAFE_DOT) continue;
        if (member_token.id != tokens::IDENTIFIER && member_token.id != tokens::METHOD_IDENTIFIER) continue;

        std::vector<symbol_id> receiver_types;
        const auto *receiver = module.index.symbol_at(
            static_cast<sagan::source::byte_offset>(receiver_token.range.begin));
        if (receiver)
        {
          std::optional<std::string> declared_type;
          if (const auto declared = binding_types.find(receiver->id.value); declared != binding_types.end())
            declared_type = declared->second;
          if (!declared_type)
            for (std::size_t declaration_token = 0; declaration_token + 2 < source_tokens.size();
                 ++declaration_token)
            {
              const auto &name = source_tokens[declaration_token];
              if (name.range.begin < static_cast<int>(receiver->declaration.bytes.begin) ||
                  name.range.end > static_cast<int>(receiver->declaration.bytes.end) ||
                  name.text != receiver->name) continue;
              if (source_tokens[declaration_token + 1].id == tokens::COLON &&
                  source_tokens[declaration_token + 2].id == tokens::IDENTIFIER)
              { declared_type = nominal_base(source_tokens[declaration_token + 2].text); break; }
              if (source_tokens[declaration_token + 1].id == tokens::EQUAL &&
                  source_tokens[declaration_token + 2].id == tokens::IDENTIFIER)
              {
                const auto *constructor = module.index.symbol_at(static_cast<sagan::source::byte_offset>(
                    source_tokens[declaration_token + 2].range.begin));
                if (!constructor) continue;
                for (const auto &entry : imports)
                  if (entry.binding == constructor->id)
                    for (const auto &target : entry.targets)
                      for (const auto &owner : indexed)
                        if (const auto *type = owner.index.find(target);
                            type && type->kind == symbol_kind::type)
                        { declared_type = type->name; break; }
                if (declared_type) break;
              }
            }
          if (declared_type)
          {
            std::vector<symbol_id> imported_types;
            for (const auto &binding : module.index.symbols())
              if (binding.origin == symbol_origin::imported && binding.name == *declared_type)
                for (const auto &entry : imports)
                  if (entry.binding == binding.id)
                    for (const auto &target : entry.targets)
                      for (const auto &owner : indexed)
                        if (const auto *symbol = owner.index.find(target);
                            symbol && symbol->kind == symbol_kind::type)
                        { imported_types.push_back(target); break; }
            if (!imported_types.empty()) receiver_types = std::move(imported_types);
            else
              for (const auto &owner : indexed)
                for (const auto &symbol : owner.index.symbols())
                  if (symbol.kind == symbol_kind::type && symbol.origin == symbol_origin::source &&
                      symbol.name == *declared_type) receiver_types.push_back(symbol.id);
          }
          if (receiver->origin == symbol_origin::imported)
            for (const auto &entry : imports)
              if (entry.binding == receiver->id)
                for (const auto &target : entry.targets)
                  if (const auto *symbol = [&]() -> const indexed_symbol *
                      {
                        for (const auto &owner : indexed)
                          if (const auto *found = owner.index.find(target)) return found;
                        return nullptr;
                      }(); symbol && symbol->kind == symbol_kind::type) receiver_types.push_back(target);
        }
        for (const auto &external : external_references)
          if (external.location.document == module.index.document().id &&
              external.location.bytes.begin <= static_cast<sagan::source::byte_offset>(receiver_token.range.begin) &&
              static_cast<sagan::source::byte_offset>(receiver_token.range.end) <= external.location.bytes.end)
            if (const auto *symbol = [&]() -> const indexed_symbol *
                {
                  for (const auto &owner : indexed)
                    if (const auto *found = owner.index.find(external.target)) return found;
                  return nullptr;
                }(); symbol && symbol->kind == symbol_kind::type)
              receiver_types.push_back(external.target);
        std::sort(receiver_types.begin(), receiver_types.end(), [](const auto &left, const auto &right)
        { return left.value < right.value; });
        receiver_types.erase(std::unique(receiver_types.begin(), receiver_types.end()), receiver_types.end());
        if (receiver_types.size() != 1) continue;
        const indexed_symbol *receiver_type = nullptr;
        const indexed_module *receiver_module = nullptr;
        for (const auto &owner : indexed)
          if (const auto *found = owner.index.find(receiver_types.front()))
          { receiver_type = found; receiver_module = &owner; break; }
        if (!receiver_type || !receiver_module) continue;
        for (const auto &candidate : receiver_module->index.symbols())
          if (candidate.owner_type == receiver_type->name && candidate.name == member_token.text &&
              candidate.visibility == symbol_visibility::public_access &&
              (candidate.kind == symbol_kind::field || candidate.kind == symbol_kind::constant_field ||
               candidate.kind == symbol_kind::method || candidate.kind == symbol_kind::enum_case) &&
              candidate.declaration.document != module.index.document().id)
            external_references.push_back(external_reference{
                candidate.id,
                {module.index.document().id,
                 {static_cast<sagan::source::byte_offset>(member_token.range.begin),
                  static_cast<sagan::source::byte_offset>(member_token.range.end)}},
                candidate.kind == symbol_kind::method ? reference_kind::call : reference_kind::read});
      }
    }
    std::sort(external_references.begin(), external_references.end(), [](const auto &left, const auto &right)
    {
      if (left.target.value != right.target.value) return left.target.value < right.target.value;
      if (left.location.document.value != right.location.document.value)
        return left.location.document.value < right.location.document.value;
      return left.location.bytes.begin < right.location.bytes.begin;
    });
    external_references.erase(std::unique(external_references.begin(), external_references.end(),
        [](const auto &left, const auto &right)
        { return left.target == right.target && left.location == right.location; }), external_references.end());
    return workspace_semantic_index(std::move(indexed), std::move(imports), std::move(exports),
                                    std::move(exported_symbols),
                                    std::move(external_references), std::move(binding_types));
  }
}
