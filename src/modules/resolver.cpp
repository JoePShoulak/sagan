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

    class symbol_rewriter
    {
      const std::unordered_map<std::string, std::string> &bindings;
      std::vector<std::unordered_set<std::string>> locals;

      auto annotation(std::optional<std::string> &name) const -> void
      {
        if (!name) return;
        if (const auto found = bindings.find(*name); found != bindings.end()) *name = found->second;
      }

      auto is_local(const std::string &name) const -> bool
      {
        return std::any_of(locals.rbegin(), locals.rend(), [&](const auto &scope) { return scope.contains(name); });
      }

      auto expression(parser::expression &value) -> void
      {
        if (auto *identifier = dynamic_cast<parser::identifier_expression *>(&value))
        {
          if (!is_local(identifier->name))
            if (const auto found = bindings.find(identifier->name); found != bindings.end())
              identifier->name = found->second;
        }
        else if (auto *group = dynamic_cast<parser::grouping_expression *>(&value)) expression(*group->value);
        else if (auto *unary = dynamic_cast<parser::unary_expression *>(&value)) expression(*unary->operand);
        else if (auto *binary = dynamic_cast<parser::binary_expression *>(&value))
        {
          expression(*binary->left);
          expression(*binary->right);
        }
        else if (auto *conditional = dynamic_cast<parser::conditional_expression *>(&value))
        {
          expression(*conditional->condition);
          expression(*conditional->when_true);
          expression(*conditional->when_false);
        }
        else if (auto *assignment = dynamic_cast<parser::assignment_expression *>(&value))
        {
          expression(*assignment->target);
          expression(*assignment->value);
        }
        else if (auto *call = dynamic_cast<parser::call_expression *>(&value))
        {
          expression(*call->callee);
          for (auto &argument : call->arguments) expression(*argument);
        }
        else if (auto *index = dynamic_cast<parser::index_expression *>(&value))
        {
          expression(*index->target);
          expression(*index->index);
        }
        else if (auto *member = dynamic_cast<parser::member_expression *>(&value)) expression(*member->target);
        else if (auto *string = dynamic_cast<parser::string_expression *>(&value))
        {
          for (auto &part : string->parts)
            if (part.interpolation) expression(*part.interpolation);
        }
        else if (auto *spread = dynamic_cast<parser::spread_expression *>(&value)) expression(*spread->value);
        else if (auto *collection = dynamic_cast<parser::collection_expression *>(&value))
        {
          for (auto &element : collection->elements) expression(*element);
        }
        else if (auto *dictionary = dynamic_cast<parser::dictionary_expression *>(&value))
        {
          for (auto &entry : dictionary->entries)
          {
            if (entry.key) expression(*entry.key);
            expression(*entry.value);
          }
        }
        else if (auto *lambda = dynamic_cast<parser::lambda_expression *>(&value))
        {
          annotation(lambda->return_type);
          locals.emplace_back();
          for (auto &parameter : lambda->parameters)
          {
            annotation(parameter.type_name);
            locals.back().insert(parameter.name);
          }
          expression(*lambda->body);
          locals.pop_back();
        }
      }

      auto block(parser::block_statement &value) -> void
      {
        locals.emplace_back();
        for (auto &entry : value.statements) statement(*entry, false, false);
        locals.pop_back();
      }

      auto function(parser::function_declaration &value, const bool top_level) -> void
      {
        if (top_level)
          if (const auto found = bindings.find(value.name); found != bindings.end()) value.name = found->second;
        annotation(value.return_type);
        locals.emplace_back();
        for (auto &parameter : value.parameters)
        {
          annotation(parameter.type_name);
          locals.back().insert(parameter.name);
        }
        if (value.body) block(*value.body);
        if (value.expression_body) expression(*value.expression_body);
        locals.pop_back();
      }

      auto statement(parser::statement &value, const bool top_level, const bool member) -> void
      {
        if (auto *declaration = dynamic_cast<parser::let_declaration *>(&value))
        {
          annotation(declaration->type_name);
          if (declaration->initializer) expression(*declaration->initializer);
          if (top_level)
          {
            if (const auto found = bindings.find(declaration->name); found != bindings.end())
              declaration->name = found->second;
          }
          else if (!member) locals.back().insert(declaration->name);
        }
        else if (auto *expression_value = dynamic_cast<parser::expression_statement *>(&value))
          expression(*expression_value->value);
        else if (auto *assignment = dynamic_cast<parser::assignment_statement *>(&value))
        {
          expression(*assignment->target);
          expression(*assignment->value);
        }
        else if (auto *nested = dynamic_cast<parser::block_statement *>(&value)) block(*nested);
        else if (auto *conditional = dynamic_cast<parser::if_statement *>(&value))
        {
          expression(*conditional->condition);
          block(*conditional->then_branch);
          if (conditional->else_branch) statement(*conditional->else_branch, false, false);
        }
        else if (auto *loop = dynamic_cast<parser::condition_loop_statement *>(&value))
        {
          expression(*loop->condition);
          block(*loop->body);
        }
        else if (auto *loop = dynamic_cast<parser::for_statement *>(&value))
        {
          expression(*loop->iterable);
          locals.emplace_back();
          locals.back().insert(loop->binding);
          block(*loop->body);
          locals.pop_back();
        }
        else if (auto *returned = dynamic_cast<parser::return_statement *>(&value))
        {
          if (returned->value) expression(*returned->value);
        }
        else if (auto *yielded = dynamic_cast<parser::yield_statement *>(&value))
        {
          if (yielded->value) expression(*yielded->value);
        }
        else if (auto *matched = dynamic_cast<parser::match_statement *>(&value))
        {
          expression(*matched->subject);
          for (auto &branch : matched->cases)
          {
            if (branch.pattern) expression(*branch.pattern);
            block(*branch.body);
          }
        }
        else if (auto *hope = dynamic_cast<parser::hope_statement *>(&value))
        {
          block(*hope->protected_body);
          for (auto &handler : hope->handlers)
          {
            expression(*handler.pattern);
            block(*handler.body);
          }
          if (hope->cleanup) block(*hope->cleanup);
        }
        else if (auto *scream = dynamic_cast<parser::scream_statement *>(&value)) expression(*scream->value);
        else if (auto *function_value = dynamic_cast<parser::function_declaration *>(&value))
          function(*function_value, top_level);
        else if (auto *type = dynamic_cast<parser::type_declaration *>(&value))
        {
          if (top_level)
            if (const auto found = bindings.find(type->name); found != bindings.end()) type->name = found->second;
          for (auto &face : type->composed_interfaces)
            if (const auto found = bindings.find(face); found != bindings.end()) face = found->second;
          locals.emplace_back();
          locals.back().insert("self");
          for (auto &entry : type->members) statement(*entry, false, true);
          locals.pop_back();
        }
      }

    public:
      explicit symbol_rewriter(const std::unordered_map<std::string, std::string> &module_bindings)
          : bindings(module_bindings)
      {
      }

      auto rewrite(parser::program &tree) -> void
      {
        locals.emplace_back();
        for (auto &entry : tree.statements) statement(*entry, true, false);
        locals.pop_back();
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

  auto link(const std::filesystem::path &entry_path) -> parser::program
  {
    const module_graph graph = resolve(entry_path);
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> linked_names;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> public_names;
    const std::string entry_name = graph.entry_path.stem().string();

    for (const auto &module : graph.modules)
    {
      parser::program tree = parse_file(module.path);
      for (const auto &entry : tree.statements)
      {
        std::string name;
        if (const auto *value = dynamic_cast<const parser::let_declaration *>(entry.get())) name = value->name;
        else if (const auto *function = dynamic_cast<const parser::function_declaration *>(entry.get()))
          name = function->name;
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get())) name = type->name;
        if (!name.empty()) linked_names[module.name][name] = module.name == entry_name ? name : module.name + "__" + name;
      }
      for (const auto &symbol : module.exports)
        public_names[module.name][symbol.public_name] = linked_names[module.name].at(symbol.local_name);
    }

    std::vector<parser::statement_ref> combined;
    for (const auto &module : graph.modules)
    {
      parser::program tree = parse_file(module.path);
      auto bindings = linked_names.at(module.name);
      for (const auto &imported : module.imports)
      {
        if (imported.whole_module) continue;
        bindings[imported.binding_name] = public_names.at(imported.module_name).at(imported.imported_name);
      }
      symbol_rewriter(bindings).rewrite(tree);
      for (auto &entry : tree.statements)
      {
        if (dynamic_cast<parser::module_declaration *>(entry.get()) ||
            dynamic_cast<parser::import_declaration *>(entry.get()) ||
            dynamic_cast<parser::export_declaration *>(entry.get())) continue;
        combined.push_back(std::move(entry));
      }
    }
    return parser::program(std::move(combined));
  }
}
