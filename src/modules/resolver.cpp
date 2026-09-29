#include "resolver.hpp"

#include "../parser/ast_node.hpp"
#include "../parser/lex.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace modules
{
  namespace
  {
    auto read_file(const std::filesystem::path &path) -> std::string
    {
      std::ifstream input(path, std::ios::binary);
      if (!input) throw std::runtime_error("Could not open module '" + path.string() + "'");
      std::ostringstream contents;
      contents << input.rdbuf();
      return contents.str();
    }

    auto parse_file(const std::filesystem::path &path) -> parser::program
    {
      try
      {
        const std::string source = read_file(path);
        parser::tokenizer lexer(parser::programText{source}, get_token);
        std::vector<parser::token> tokens;
        while (auto next = lexer.next()) tokens.push_back(std::move(*next));
        parser::syntax_parser syntax(std::move(tokens));
        return syntax.parse();
      }
      catch (const parser::parse_error &error)
      {
        throw std::runtime_error("Invalid module '" + path.string() + "' at byte " +
                                 std::to_string(error.range.begin) + ": " + error.what());
      }
    }

    auto declared_name(const parser::program &tree) -> std::optional<std::string>
    {
      for (const auto &entry : tree.statements)
        if (const auto *module = dynamic_cast<const parser::module_declaration *>(entry.get()))
          return module->name;
      return {};
    }

    auto local_declarations(const parser::program &tree) -> std::unordered_set<std::string>
    {
      std::unordered_set<std::string> names;
      for (const auto &entry : tree.statements)
      {
        if (const auto *value = dynamic_cast<const parser::let_declaration *>(entry.get())) names.insert(value->name);
        else if (const auto *function = dynamic_cast<const parser::function_declaration *>(entry.get()))
          names.insert(function->name);
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get()))
          names.insert(type->name);
      }
      return names;
    }

    class resolver
    {
      std::filesystem::path source_root;
      std::filesystem::path entry;
      std::unordered_map<std::string, int> states;
      std::vector<std::string> stack;
      std::vector<module_info> resolved;

      auto module_path(const std::string &name) const -> std::filesystem::path
      {
        return source_root / (name + ".sagan");
      }

      auto visit(const std::string &name, const std::filesystem::path &path, const bool imported) -> void
      {
        if (states[name] == 2) return;
        if (states[name] == 1)
        {
          const auto beginning = std::find(stack.begin(), stack.end(), name);
          std::ostringstream cycle;
          for (auto item = beginning; item != stack.end(); ++item)
          {
            if (item != beginning) cycle << " -> ";
            cycle << *item;
          }
          cycle << " -> " << name;
          throw std::runtime_error("Cyclic module dependency: " + cycle.str());
        }

        states[name] = 1;
        stack.push_back(name);
        parser::program tree = parse_file(path);
        const auto declaration = declared_name(tree);
        if (imported && !declaration)
          throw std::runtime_error("Imported module '" + name + "' must declare 'module " + name + "'");
        if (declaration && *declaration != name)
          throw std::runtime_error("Module file '" + path.filename().string() + "' declares '" + *declaration +
                                   "', expected '" + name + "'");

        module_info current{name, path, {}, {}};
        const auto locals = local_declarations(tree);
        std::unordered_set<std::string> public_names;
        for (const auto &entry_value : tree.statements)
        {
          if (const auto *exported = dynamic_cast<const parser::export_declaration *>(entry_value.get()))
          {
            if (!locals.contains(exported->exported_name))
              throw std::runtime_error("Module '" + name + "' exports undefined declaration '" +
                                       exported->exported_name + "'");
            const std::string public_name = exported->alias.value_or(exported->exported_name);
            if (!public_names.insert(public_name).second)
              throw std::runtime_error("Module '" + name + "' exports duplicate public name '" + public_name + "'");
            current.exports.push_back(export_symbol{exported->exported_name, public_name});
          }
        }

        for (const auto &entry_value : tree.statements)
        {
          const auto *imported_value = dynamic_cast<const parser::import_declaration *>(entry_value.get());
          if (!imported_value) continue;
          const bool whole_module = !imported_value->source_module.has_value();
          const std::string dependency = imported_value->source_module.value_or(imported_value->imported_name);
          const std::string binding = imported_value->alias.value_or(imported_value->imported_name);
          visit(dependency, module_path(dependency), true);
          const auto dependency_info = std::find_if(resolved.begin(), resolved.end(), [&](const module_info &candidate)
          {
            return candidate.name == dependency;
          });
          if (!whole_module)
          {
            const bool exported = std::any_of(dependency_info->exports.begin(), dependency_info->exports.end(),
                                              [&](const export_symbol &symbol)
            {
              return symbol.public_name == imported_value->imported_name;
            });
            if (!exported)
              throw std::runtime_error("Module '" + dependency + "' does not export '" +
                                       imported_value->imported_name + "'");
          }
          current.imports.push_back(import_edge{dependency, imported_value->imported_name, binding, whole_module});
        }

        stack.pop_back();
        states[name] = 2;
        resolved.push_back(std::move(current));
      }

    public:
      explicit resolver(std::filesystem::path entry_path)
          : source_root(std::filesystem::absolute(entry_path).parent_path()),
            entry(std::filesystem::absolute(std::move(entry_path)).lexically_normal())
      {
      }

      auto run() -> module_graph
      {
        const std::string entry_name = entry.stem().string();
        visit(entry_name, entry, false);
        return module_graph{source_root, entry, std::move(resolved)};
      }
    };
  }

  auto module_graph::print(std::ostream &stream) const -> void
  {
    stream << "ModuleGraph\n  SourceRoot(" << source_root.string() << ")\n  Entry(" << entry_path.string() << ")\n";
    for (const auto &module : modules)
    {
      stream << "  Module(" << module.name << ", " << module.path.filename().string() << ")\n";
      for (const auto &symbol : module.exports)
        stream << "    Export(" << symbol.local_name << " as " << symbol.public_name << ")\n";
      for (const auto &imported : module.imports)
      {
        stream << "    Import(";
        if (imported.whole_module) stream << "module " << imported.module_name;
        else stream << imported.imported_name << " from " << imported.module_name;
        stream << " as " << imported.binding_name << ")\n";
      }
    }
  }

  auto resolve(const std::filesystem::path &entry_path) -> module_graph
  {
    if (entry_path.extension() != ".sagan")
      throw std::runtime_error("Module entry file must use the .sagan extension");
    return resolver(entry_path).run();
  }
}
