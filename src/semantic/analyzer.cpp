#include "analyzer.hpp"

#include "semantic_error.hpp"

#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace semantic
{
  namespace
  {
    constexpr std::size_t no_parent = std::numeric_limits<std::size_t>::max();

    class analysis
    {
      semantic_model model;
      std::vector<std::unordered_map<std::string, std::vector<std::size_t>>> names;
      std::size_t current_scope = 0;

      auto open_scope(std::string label) -> std::size_t
      {
        const std::size_t parent = current_scope;
        const std::size_t id = model.scopes.size();
        model.scopes.push_back(scope{id, parent, std::move(label), {}});
        names.emplace_back();
        current_scope = id;
        return parent;
      }

      auto close_scope(const std::size_t parent) -> void
      {
        current_scope = parent;
      }

      auto declare(std::string name, std::string kind, const parser::span range) -> void
      {
        auto &entries = names[current_scope][name];
        if (!entries.empty())
        {
          const symbol &existing = model.scopes[current_scope].symbols[entries.front()];
          const bool overload = existing.kind == "function" && kind == "function";
          if (!overload)
          {
            throw semantic_error("Duplicate declaration of '" + name + "' in the same scope", range);
          }
        }
        entries.push_back(model.scopes[current_scope].symbols.size());
        model.scopes[current_scope].symbols.push_back(symbol{std::move(name), std::move(kind), range});
      }

      auto find(const std::string &name) const -> std::optional<symbol>
      {
        std::size_t candidate = current_scope;
        while (candidate != no_parent)
        {
          const auto found = names[candidate].find(name);
          if (found != names[candidate].end())
          {
            return model.scopes[candidate].symbols[found->second.front()];
          }
          candidate = model.scopes[candidate].parent;
        }
        return {};
      }

      auto resolve_name(const std::string &name, const parser::span range) -> void
      {
        const auto declaration = find(name);
        if (!declaration)
        {
          throw semantic_error("Undefined name '" + name + "'", range);
        }
        model.resolutions.push_back(resolution{name, range, declaration->declaration});
      }

      auto resolve_type(const std::optional<std::string> &name, const parser::span range) -> void
      {
        if (name)
        {
          const std::size_t open = name->find('<');
          const std::string base = open == std::string::npos ? *name : name->substr(0, open);
          const auto declaration = find(base);
          if (!declaration)
          {
            throw semantic_error("Undefined type '" + base + "'", range);
          }
          if (declaration->kind != "type" && declaration->kind != "builtin type" && declaration->kind != "import")
          {
            throw semantic_error("'" + base + "' does not name a type", range);
          }
          model.resolutions.push_back(resolution{base, range, declaration->declaration});
          if (open != std::string::npos)
          {
            if (!name->ends_with('>')) throw semantic_error("Malformed type annotation '" + *name + "'", range);
            resolve_type(std::optional<std::string>{name->substr(open + 1, name->size() - open - 2)}, range);
          }
        }
      }

      auto predeclare(const parser::statement &value) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          declare(declaration->name, "variable", declaration->range);
        }
        else if (const auto *function = dynamic_cast<const parser::function_declaration *>(&value))
        {
          declare(function->name, "function", function->range);
        }
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(&value))
        {
          declare(type->name, "type", type->range);
        }
        else if (const auto *imported = dynamic_cast<const parser::import_declaration *>(&value))
        {
          declare(imported->alias.value_or(imported->imported_name), "import", imported->range);
        }
      }

      auto expression(const parser::expression &value) -> void
      {
        if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          resolve_name(identifier->name, identifier->range);
        }
        else if (const auto *grouping = dynamic_cast<const parser::grouping_expression *>(&value))
        {
          expression(*grouping->value);
        }
        else if (const auto *unary = dynamic_cast<const parser::unary_expression *>(&value))
        {
          expression(*unary->operand);
        }
        else if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
        {
          expression(*binary->left);
          expression(*binary->right);
        }
        else if (const auto *conditional = dynamic_cast<const parser::conditional_expression *>(&value))
        {
          expression(*conditional->condition);
          expression(*conditional->when_true);
          expression(*conditional->when_false);
        }
        else if (const auto *assignment = dynamic_cast<const parser::assignment_expression *>(&value))
        {
          expression(*assignment->target);
          expression(*assignment->value);
        }
        else if (const auto *call = dynamic_cast<const parser::call_expression *>(&value))
        {
          expression(*call->callee);
          for (const auto &argument : call->arguments) expression(*argument);
        }
        else if (const auto *index = dynamic_cast<const parser::index_expression *>(&value))
        {
          expression(*index->target);
          expression(*index->index);
        }
        else if (const auto *member = dynamic_cast<const parser::member_expression *>(&value))
        {
          expression(*member->target);
        }
        else if (const auto *string = dynamic_cast<const parser::string_expression *>(&value))
        {
          for (const auto &part : string->parts)
          {
            if (part.interpolation) expression(*part.interpolation);
          }
        }
        else if (const auto *spread = dynamic_cast<const parser::spread_expression *>(&value))
        {
          expression(*spread->value);
        }
        else if (const auto *collection = dynamic_cast<const parser::collection_expression *>(&value))
        {
          for (const auto &element : collection->elements) expression(*element);
        }
        else if (const auto *dictionary = dynamic_cast<const parser::dictionary_expression *>(&value))
        {
          for (const auto &entry : dictionary->entries)
          {
            if (entry.key) expression(*entry.key);
            expression(*entry.value);
          }
        }
        else if (const auto *lambda = dynamic_cast<const parser::lambda_expression *>(&value))
        {
          const std::size_t parent = open_scope("lambda");
          for (const auto &parameter : lambda->parameters)
          {
            resolve_type(parameter.type_name, lambda->range);
            declare(parameter.name, "parameter", lambda->range);
          }
          resolve_type(lambda->return_type, lambda->range);
          expression(*lambda->body);
          close_scope(parent);
        }
      }

      auto block(const parser::block_statement &value, std::string label = "block") -> void
      {
        const std::size_t parent = open_scope(std::move(label));
        for (const auto &entry : value.statements)
        {
          if (dynamic_cast<const parser::let_declaration *>(entry.get()))
          {
            statement(*entry, false);
          }
          else
          {
            statement(*entry, true);
          }
        }
        close_scope(parent);
      }

      auto function(const parser::function_declaration &value) -> void
      {
        const std::size_t parent = open_scope("function " + value.name);
        for (const auto &parameter : value.parameters)
        {
          resolve_type(parameter.type_name, value.range);
          declare(parameter.name, "parameter", value.range);
        }
        resolve_type(value.return_type, value.range);
        if (value.body) block(*value.body, "function body");
        if (value.expression_body) expression(*value.expression_body);
        close_scope(parent);
      }

      auto type(const parser::type_declaration &value) -> void
      {
        for (const auto &interface_name : value.composed_interfaces)
        {
          resolve_name(interface_name, value.range);
        }
        const std::size_t parent = open_scope("type " + value.name);
        if (value.type_kind == parser::type_declaration::kind::class_type ||
            value.type_kind == parser::type_declaration::kind::interface_type)
        {
          declare("self", "self", value.range);
        }
        for (const auto &member : value.members) predeclare(*member);
        for (const auto &member : value.enum_members) declare(member.name, "enum member", member.range);
        for (const auto &member : value.members) statement(*member, true);
        close_scope(parent);
      }

      auto statement(const parser::statement &value, const bool already_declared) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          resolve_type(declaration->type_name, declaration->range);
          if (declaration->initializer) expression(*declaration->initializer);
          if (!already_declared) declare(declaration->name, "variable", declaration->range);
        }
        else if (const auto *expression_statement = dynamic_cast<const parser::expression_statement *>(&value))
        {
          expression(*expression_statement->value);
        }
        else if (const auto *assignment = dynamic_cast<const parser::assignment_statement *>(&value))
        {
          expression(*assignment->target);
          expression(*assignment->value);
        }
        else if (const auto *nested = dynamic_cast<const parser::block_statement *>(&value))
        {
          block(*nested);
        }
        else if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          expression(*conditional->condition);
          block(*conditional->then_branch, "if branch");
          if (conditional->else_branch) statement(*conditional->else_branch, true);
        }
        else if (const auto *loop = dynamic_cast<const parser::condition_loop_statement *>(&value))
        {
          expression(*loop->condition);
          block(*loop->body, "loop body");
        }
        else if (const auto *loop = dynamic_cast<const parser::for_statement *>(&value))
        {
          expression(*loop->iterable);
          const std::size_t parent = open_scope("for loop");
          declare(loop->binding, "loop binding", loop->range);
          for (const auto &entry : loop->body->statements) statement(*entry, false);
          close_scope(parent);
        }
        else if (const auto *returned = dynamic_cast<const parser::return_statement *>(&value))
        {
          if (returned->value) expression(*returned->value);
        }
        else if (const auto *yielded = dynamic_cast<const parser::yield_statement *>(&value))
        {
          if (yielded->value) expression(*yielded->value);
        }
        else if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          expression(*matched->subject);
          for (const auto &branch : matched->cases)
          {
            const auto *call = branch.pattern
                                   ? dynamic_cast<const parser::call_expression *>(branch.pattern.get())
                                   : nullptr;
            const auto *callee = call
                                     ? dynamic_cast<const parser::identifier_expression *>(call->callee.get())
                                     : nullptr;
            const auto *binding = call && call->arguments.size() == 1
                                      ? dynamic_cast<const parser::identifier_expression *>(call->arguments[0].get())
                                      : nullptr;
            const bool some_pattern = callee && callee->name == "Some" && binding;
            if (branch.pattern && !some_pattern) expression(*branch.pattern);
            const std::size_t parent = open_scope("match case");
            if (some_pattern)
            {
              resolve_name("Some", callee->range);
              declare(binding->name, "match binding", binding->range);
            }
            for (const auto &entry : branch.body->statements) statement(*entry, false);
            close_scope(parent);
          }
        }
        else if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&value))
        {
          block(*hope->protected_body, "hope body");
          for (const auto &handler : hope->handlers)
          {
            expression(*handler.pattern);
            block(*handler.body, "unless body");
          }
          if (hope->cleanup) block(*hope->cleanup, "finally body");
        }
        else if (const auto *scream = dynamic_cast<const parser::scream_statement *>(&value))
        {
          expression(*scream->value);
        }
        else if (const auto *function_declaration = dynamic_cast<const parser::function_declaration *>(&value))
        {
          function(*function_declaration);
        }
        else if (const auto *type_declaration = dynamic_cast<const parser::type_declaration *>(&value))
        {
          type(*type_declaration);
        }
        else if (const auto *exported = dynamic_cast<const parser::export_declaration *>(&value))
        {
          resolve_name(exported->exported_name, exported->range);
        }
      }

    public:
      analysis()
      {
        model.scopes.push_back(scope{0, no_parent, "program", {}});
        names.emplace_back();
        for (const std::string_view builtin : {"Bool", "Coordinate", "Float", "Float32", "Float64", "Frame",
                                               "Int", "Int8", "Int16", "Int32", "Int64", "Optional", "String",
                                               "Vector", "Void"})
        {
          declare(std::string(builtin), "builtin type", parser::span{0, 0});
        }
        declare("print", "function", parser::span{0, 0});
        declare("Some", "function", parser::span{0, 0});
        declare("None", "builtin value", parser::span{0, 0});
      }

      auto run(const parser::program &tree) -> semantic_model
      {
        for (const auto &entry : tree.statements) predeclare(*entry);
        for (const auto &entry : tree.statements) statement(*entry, true);
        return std::move(model);
      }
    };
  }

  auto semantic_model::print(std::ostream &stream) const -> void
  {
    stream << "SemanticModel\n";
    for (const auto &entry : scopes)
    {
      stream << "  Scope(" << entry.id << ", " << entry.label;
      if (entry.parent != no_parent) stream << ", parent=" << entry.parent;
      stream << ")\n";
      for (const auto &declared : entry.symbols)
      {
        stream << "    Symbol(" << declared.kind << " " << declared.name << " @ "
               << declared.declaration.begin << ".." << declared.declaration.end << ")\n";
      }
    }
    stream << "  Resolutions\n";
    for (const auto &resolved : resolutions)
    {
      stream << "    " << resolved.name << " @ " << resolved.use.begin << ".." << resolved.use.end
             << " -> " << resolved.declaration.begin << ".." << resolved.declaration.end << '\n';
    }
  }

  auto analyze(const parser::program &tree) -> semantic_model
  {
    return analysis().run(tree);
  }
}
