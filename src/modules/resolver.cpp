#include "resolver.hpp"

#include "../parser/ast_node.hpp"
#include "../parser/lex.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <regex>
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

    auto parse_file(const std::filesystem::path &path, const sagan::source::source_provider &source)
      -> parser::program
    {
      try
      {
        const auto snapshot = source.read_path(path);
        if (!snapshot)
        {
          if (snapshot.error->code == sagan::source::provider_error_code::not_found)
            throw std::runtime_error("Could not open module '" + path.string() + "'");
          throw std::runtime_error(snapshot.error->message);
        }
        parser::tokenizer lexer(parser::programText{std::string(snapshot.value->text())}, get_token);
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

    auto trim(std::string value) -> std::string
    {
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.pop_back();
      return value;
    }

    auto quoted_value(const std::string &value, const std::string &key) -> std::string
    {
      if (value.size() < 2 || value.front() != '"' || value.back() != '"')
        throw std::runtime_error("Package manifest value for '" + key + "' must be a quoted string");
      return value.substr(1, value.size() - 2);
    }

    auto manifest_from_file(const std::filesystem::path &manifest_path) -> package_manifest
    {
      const auto absolute_manifest = std::filesystem::absolute(manifest_path).lexically_normal();
      std::istringstream input(read_file(absolute_manifest));
      std::unordered_map<std::string, std::string> package_values;
      std::unordered_map<std::string, std::string> application_values;
      std::string section;
      std::string line;
      std::size_t line_number = 0;
      while (std::getline(input, line))
      {
        ++line_number;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line = trim(std::move(line));
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']')
        {
          section = trim(line.substr(1, line.size() - 2));
          if (section != "package" && section != "application")
            throw std::runtime_error("Unknown package manifest section '[" + section + "]'");
          continue;
        }
        if (section.empty())
          throw std::runtime_error("Package manifest values must appear under [package] or [application]");
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
          throw std::runtime_error("Invalid package manifest line " + std::to_string(line_number));
        const std::string key = trim(line.substr(0, equals));
        const bool package_key = section == "package" &&
                                 (key == "name" || key == "version" || key == "source" || key == "entry");
        const bool application_key = section == "application" && key == "mode";
        if (!package_key && !application_key)
          throw std::runtime_error("Unknown package manifest key '" + key + "'");
        auto &values = section == "package" ? package_values : application_values;
        if (!values.emplace(key, quoted_value(trim(line.substr(equals + 1)), key)).second)
          throw std::runtime_error("Duplicate package manifest key '" + key + "'");
      }
      for (const std::string_view required : {"name", "version", "source", "entry"})
        if (!package_values.contains(std::string(required)))
          throw std::runtime_error("Package manifest is missing required key '" + std::string(required) + "'");
      if (!std::regex_match(package_values.at("name"), std::regex{"[A-Za-z][A-Za-z0-9_-]*"}))
        throw std::runtime_error("Package name must begin with a letter and contain only letters, digits, '_' or '-'");
      if (!std::regex_match(package_values.at("version"), std::regex{"[0-9]+\\.[0-9]+\\.[0-9]+"}))
        throw std::runtime_error("Package version must use MAJOR.MINOR.PATCH");
      if (!std::regex_match(package_values.at("entry"), std::regex{"[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)*"}))
        throw std::runtime_error("Package entry must be a qualified module name");
      application_mode mode = application_mode::console;
      if (application_values.contains("mode"))
      {
        const std::string &configured = application_values.at("mode");
        if (configured == "windowed") mode = application_mode::windowed;
        else if (configured != "console")
          throw std::runtime_error("Application mode must be 'console' or 'windowed'");
      }
      const auto package_root = absolute_manifest.parent_path();
      const auto source_root = std::filesystem::absolute(package_root / package_values.at("source")).lexically_normal();
      const auto relative_source = source_root.lexically_relative(package_root);
      if (relative_source.empty() || (!relative_source.empty() && *relative_source.begin() == ".."))
        throw std::runtime_error("Package source directory must stay inside the package root");
      if (!std::filesystem::is_directory(source_root))
        throw std::runtime_error("Package source directory does not exist: " + source_root.string());
      return package_manifest{package_values.at("name"), package_values.at("version"), absolute_manifest,
                              package_root, source_root, package_values.at("entry"), mode};
    }

    auto discover_manifest(std::filesystem::path path) -> std::optional<package_manifest>
    {
      path = std::filesystem::absolute(std::move(path)).lexically_normal().parent_path();
      while (!path.empty())
      {
        const auto candidate = path / "sagan.toml";
        if (std::filesystem::is_regular_file(candidate)) return manifest_from_file(candidate);
        const auto parent = path.parent_path();
        if (parent == path) break;
        path = parent;
      }
      return {};
    }

    auto path_for_module(const std::filesystem::path &root, const std::string &name) -> std::filesystem::path
    {
      std::filesystem::path relative;
      std::size_t begin = 0;
      while (begin < name.size())
      {
        const std::size_t separator = name.find('.', begin);
        relative /= name.substr(begin, separator == std::string::npos ? std::string::npos : separator - begin);
        if (separator == std::string::npos) break;
        begin = separator + 1;
      }
      relative += ".sagan";
      return (root / relative).lexically_normal();
    }

    auto module_name_for_path(const std::filesystem::path &root, const std::filesystem::path &path) -> std::string
    {
      auto relative = std::filesystem::absolute(path).lexically_normal().lexically_relative(root);
      if (relative.empty() || *relative.begin() == "..")
        throw std::runtime_error("Module entry must stay inside the package source directory");
      relative.replace_extension();
      std::string result;
      for (const auto &component : relative)
      {
        if (!result.empty()) result += '.';
        result += component.string();
      }
      return result;
    }

    class resolver
    {
      std::filesystem::path source_root;
      std::filesystem::path entry;
      std::optional<package_manifest> package;
      const sagan::source::source_provider &source;
      std::unordered_map<std::string, int> states;
      std::vector<std::string> stack;
      std::vector<module_info> resolved;

      auto module_path(const std::string &name) const -> std::filesystem::path
      {
        return path_for_module(source_root, name);
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
        parser::program tree = parse_file(path, source);
        const auto declaration = declared_name(tree);
        if ((imported || package.has_value()) && !declaration)
          throw std::runtime_error("Module '" + name + "' must declare 'module " + name + "'");
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
            current.exports.push_back(export_symbol{exported->exported_name, public_name,
                                                     exported->range});
          }
        }

        for (const auto &entry_value : tree.statements)
        {
          const auto *imported_value = dynamic_cast<const parser::import_declaration *>(entry_value.get());
          if (!imported_value) continue;
          const bool whole_module = !imported_value->source_module.has_value();
          const std::string dependency = imported_value->source_module.value_or(imported_value->imported_name);
          const std::size_t separator = imported_value->imported_name.rfind('.');
          const std::string default_binding = separator == std::string::npos
                                                  ? imported_value->imported_name
                                                  : imported_value->imported_name.substr(separator + 1);
          const std::string binding = imported_value->alias.value_or(default_binding);
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
          current.imports.push_back(import_edge{dependency, imported_value->imported_name, binding,
                                                whole_module, imported_value->range});
        }

        stack.pop_back();
        states[name] = 2;
        resolved.push_back(std::move(current));
      }

    public:
      explicit resolver(std::filesystem::path entry_path, const sagan::source::source_provider &source_provider,
                        std::optional<package_manifest> manifest = {})
          : source_root(manifest ? manifest->source_root : std::filesystem::absolute(entry_path).parent_path()),
            entry(std::filesystem::absolute(std::move(entry_path)).lexically_normal()), package(std::move(manifest)),
            source(source_provider)
      {
      }

      auto run() -> module_graph
      {
        const std::string entry_name = package ? module_name_for_path(source_root, entry) : entry.stem().string();
        visit(entry_name, entry, false);
        return module_graph{source_root, entry, package, std::move(resolved)};
      }
    };

    class symbol_rewriter
    {
      const std::unordered_map<std::string, std::string> &bindings;
      const std::unordered_map<std::string, std::string> &namespaces;
      const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &public_names;
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

      auto expression(parser::expression_ref &value) -> void
      {
        if (const auto *member = dynamic_cast<const parser::member_expression *>(value.get()))
        {
          const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get());
          if (target && !is_local(target->name))
          {
            if (const auto namespace_value = namespaces.find(target->name); namespace_value != namespaces.end())
            {
              const auto module_exports = public_names.find(namespace_value->second);
              if (module_exports == public_names.end() || !module_exports->second.contains(member->member_name))
                throw std::runtime_error("Module '" + namespace_value->second + "' does not export '" +
                                         member->member_name + "'");
              const auto public_name = module_exports->second.find(member->member_name);
              value = std::make_unique<parser::identifier_expression>(member->range, public_name->second);
            }
          }
        }
        expression(*value);
      }

      auto expression(parser::expression &value) -> void
      {
        if (auto *identifier = dynamic_cast<parser::identifier_expression *>(&value))
        {
          if (!is_local(identifier->name))
            if (const auto found = bindings.find(identifier->name); found != bindings.end())
              identifier->name = found->second;
        }
        else if (auto *group = dynamic_cast<parser::grouping_expression *>(&value)) expression(group->value);
        else if (auto *unary = dynamic_cast<parser::unary_expression *>(&value)) expression(unary->operand);
        else if (auto *binary = dynamic_cast<parser::binary_expression *>(&value))
        {
          expression(binary->left);
          expression(binary->right);
        }
        else if (auto *conditional = dynamic_cast<parser::conditional_expression *>(&value))
        {
          expression(conditional->condition);
          expression(conditional->when_true);
          expression(conditional->when_false);
        }
        else if (auto *assignment = dynamic_cast<parser::assignment_expression *>(&value))
        {
          expression(assignment->target);
          expression(assignment->value);
        }
        else if (auto *call = dynamic_cast<parser::call_expression *>(&value))
        {
          expression(call->callee);
          for (auto &argument : call->arguments) expression(argument);
        }
        else if (auto *index = dynamic_cast<parser::index_expression *>(&value))
        {
          expression(index->target);
          expression(index->index);
        }
        else if (auto *member = dynamic_cast<parser::member_expression *>(&value)) expression(member->target);
        else if (auto *string = dynamic_cast<parser::string_expression *>(&value))
        {
          for (auto &part : string->parts)
            if (part.interpolation) expression(part.interpolation);
        }
        else if (auto *spread = dynamic_cast<parser::spread_expression *>(&value)) expression(spread->value);
        else if (auto *collection = dynamic_cast<parser::collection_expression *>(&value))
        {
          for (auto &element : collection->elements) expression(element);
        }
        else if (auto *dictionary = dynamic_cast<parser::dictionary_expression *>(&value))
        {
          for (auto &entry : dictionary->entries)
          {
            if (entry.key) expression(entry.key);
            expression(entry.value);
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
          expression(lambda->body);
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
        if (value.expression_body) expression(value.expression_body);
        locals.pop_back();
      }

      auto statement(parser::statement &value, const bool top_level, const bool member) -> void
      {
        if (auto *declaration = dynamic_cast<parser::let_declaration *>(&value))
        {
          annotation(declaration->type_name);
          if (declaration->initializer) expression(declaration->initializer);
          if (top_level)
          {
            if (const auto found = bindings.find(declaration->name); found != bindings.end())
              declaration->name = found->second;
          }
          else if (!member) locals.back().insert(declaration->name);
        }
        else if (auto *expression_value = dynamic_cast<parser::expression_statement *>(&value))
          expression(expression_value->value);
        else if (auto *assignment = dynamic_cast<parser::assignment_statement *>(&value))
        {
          expression(assignment->target);
          expression(assignment->value);
        }
        else if (auto *nested = dynamic_cast<parser::block_statement *>(&value)) block(*nested);
        else if (auto *conditional = dynamic_cast<parser::if_statement *>(&value))
        {
          expression(conditional->condition);
          block(*conditional->then_branch);
          if (conditional->else_branch) statement(*conditional->else_branch, false, false);
        }
        else if (auto *loop = dynamic_cast<parser::condition_loop_statement *>(&value))
        {
          expression(loop->condition);
          block(*loop->body);
        }
        else if (auto *loop = dynamic_cast<parser::for_statement *>(&value))
        {
          expression(loop->iterable);
          locals.emplace_back();
          locals.back().insert(loop->binding);
          block(*loop->body);
          locals.pop_back();
        }
        else if (auto *returned = dynamic_cast<parser::return_statement *>(&value))
        {
          if (returned->value) expression(returned->value);
        }
        else if (auto *yielded = dynamic_cast<parser::yield_statement *>(&value))
        {
          if (yielded->value) expression(yielded->value);
        }
        else if (auto *matched = dynamic_cast<parser::match_statement *>(&value))
        {
          expression(matched->subject);
          for (auto &branch : matched->cases)
          {
            if (branch.pattern) expression(branch.pattern);
            block(*branch.body);
          }
        }
        else if (auto *hope = dynamic_cast<parser::hope_statement *>(&value))
        {
          block(*hope->protected_body);
          for (auto &handler : hope->handlers)
          {
            expression(handler.pattern);
            block(*handler.body);
          }
          if (hope->cleanup) block(*hope->cleanup);
        }
        else if (auto *scream = dynamic_cast<parser::scream_statement *>(&value)) expression(scream->value);
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
      symbol_rewriter(const std::unordered_map<std::string, std::string> &module_bindings,
                      const std::unordered_map<std::string, std::string> &module_namespaces,
                      const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &exports)
          : bindings(module_bindings), namespaces(module_namespaces), public_names(exports)
      {
      }

      auto rewrite(parser::program &tree) -> void
      {
        locals.emplace_back();
        for (auto &entry : tree.statements) statement(*entry, true, false);
        locals.pop_back();
      }
    };

    auto link_graph(const module_graph &graph, const sagan::source::source_provider &source) -> parser::program
    {
      std::unordered_map<std::string, std::unordered_map<std::string, std::string>> linked_names;
      std::unordered_map<std::string, std::unordered_map<std::string, std::string>> public_names;
      const std::string entry_name = graph.package
                                         ? module_name_for_path(graph.source_root, graph.entry_path)
                                         : graph.entry_path.stem().string();

      for (const auto &module : graph.modules)
      {
        parser::program tree = parse_file(module.path, source);
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
        parser::program tree = parse_file(module.path, source);
        auto bindings = linked_names.at(module.name);
        std::unordered_map<std::string, std::string> namespaces;
        for (const auto &imported : module.imports)
        {
          if (imported.whole_module) namespaces[imported.binding_name] = imported.module_name;
          else bindings[imported.binding_name] = public_names.at(imported.module_name).at(imported.imported_name);
        }
        symbol_rewriter(bindings, namespaces, public_names).rewrite(tree);
        for (auto &entry : tree.statements)
        {
          if (dynamic_cast<parser::module_declaration *>(entry.get()) ||
              dynamic_cast<parser::import_declaration *>(entry.get()) ||
              dynamic_cast<parser::export_declaration *>(entry.get())) continue;
          entry->origin_path = module.path;
          combined.push_back(std::move(entry));
        }
      }
      return parser::program(std::move(combined));
    }
  }

  auto application_mode_name(const application_mode mode) -> std::string_view
  {
    return mode == application_mode::windowed ? "windowed" : "console";
  }

  auto module_graph::print(std::ostream &stream) const -> void
  {
    stream << "ModuleGraph\n";
    if (package)
      stream << "  Package(" << package->name << " " << package->version << ")\n"
             << "  Manifest(" << package->manifest_path.string() << ")\n"
             << "  ApplicationMode(" << application_mode_name(package->mode) << ")\n";
    stream << "  SourceRoot(" << source_root.string() << ")\n  Entry(" << entry_path.string() << ")\n";
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
    const sagan::source::disk_source_provider source;
    return resolve(entry_path, source);
  }

  auto resolve(const std::filesystem::path &entry_path, const sagan::source::source_provider &source)
    -> module_graph
  {
    if (entry_path.extension() != ".sagan")
      throw std::runtime_error("Module entry file must use the .sagan extension");
    const auto manifest = discover_manifest(entry_path);
    return resolver(entry_path, source, manifest).run();
  }

  auto link(const std::filesystem::path &entry_path) -> parser::program
  {
    const sagan::source::disk_source_provider source;
    return link(entry_path, source);
  }

  auto link(const std::filesystem::path &entry_path, const sagan::source::source_provider &source)
    -> parser::program
  {
    return link_graph(resolve(entry_path, source), source);
  }

  auto load_package(const std::filesystem::path &package_path) -> package_manifest
  {
    const auto absolute = std::filesystem::absolute(package_path).lexically_normal();
    const auto manifest = std::filesystem::is_directory(absolute) ? absolute / "sagan.toml" : absolute;
    if (manifest.filename() != "sagan.toml")
      throw std::runtime_error("Package path must name a directory or sagan.toml");
    if (!std::filesystem::is_regular_file(manifest))
      throw std::runtime_error("Could not find package manifest '" + manifest.string() + "'");
    return manifest_from_file(manifest);
  }

  auto discover_package(const std::filesystem::path &entry_path) -> std::optional<package_manifest>
  {
    return discover_manifest(entry_path);
  }

  auto resolve_package(const std::filesystem::path &package_path) -> module_graph
  {
    const sagan::source::disk_source_provider source;
    return resolve_package(package_path, source);
  }

  auto resolve_package(const std::filesystem::path &package_path,
                       const sagan::source::source_provider &source) -> module_graph
  {
    auto manifest = load_package(package_path);
    const auto entry_path = path_for_module(manifest.source_root, manifest.entry_module);
    if (!source.exists_path(entry_path))
      throw std::runtime_error("Package entry module does not exist: " + entry_path.string());
    return resolver(entry_path, source, std::move(manifest)).run();
  }

  auto link_package(const std::filesystem::path &package_path) -> parser::program
  {
    const sagan::source::disk_source_provider source;
    return link_package(package_path, source);
  }

  auto link_package(const std::filesystem::path &package_path,
                    const sagan::source::source_provider &source) -> parser::program
  {
    return link_graph(resolve_package(package_path, source), source);
  }
}
