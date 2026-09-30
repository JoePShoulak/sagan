#include "analyzer.hpp"
#include "../parser/naming.hpp"

#include "semantic_error.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
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
      std::vector<std::string> scope_keys{"program"};
      analysis_identity identity_;
      bool tolerant_{};
      std::size_t current_scope = 0;
      std::vector<std::size_t> active_type_scopes;
      std::unordered_map<std::string, std::size_t> type_scopes;

      static auto documentation(const std::vector<parser::documentation_comment> &comments)
        -> std::vector<std::string>
      {
        std::vector<std::string> result;
        result.reserve(comments.size());
        for (const auto &comment : comments) result.push_back(comment.text);
        return result;
      }

      auto make_id(const std::string_view declared_name, const symbol_kind kind,
                   const symbol_origin origin, const std::size_t ordinal) const -> symbol_id
      {
        const std::string package = origin == symbol_origin::builtin ? "sagan" : identity_.package;
        const std::string module = origin == symbol_origin::builtin ? "core" : identity_.module;
        const std::string scope = origin == symbol_origin::builtin ? "builtins" : scope_keys[current_scope];
        const std::string key = "sagan-symbol-v1|" + package + "|" + module + "|" +
                                scope + "|" + std::string(name(kind)) + "|" +
                                std::string(declared_name) + "|" + std::to_string(ordinal);
        std::uint64_t hash = 14695981039346656037ULL;
        for (const unsigned char byte : key)
        {
          hash ^= byte;
          hash *= 1099511628211ULL;
        }
        std::ostringstream encoded;
        encoded << "sagan-symbol-v1:" << std::hex << std::setfill('0') << std::setw(16) << hash;
        return {encoded.str()};
      }

      auto open_scope(std::string label, const parser::span range) -> std::size_t
      {
        const std::size_t parent = current_scope;
        const std::size_t id = model.scopes.size();
        model.scopes.push_back(scope{id, parent, std::move(label), range, {}});
        names.emplace_back();
        scope_keys.push_back(scope_keys[parent] + "/" + std::to_string(id));
        current_scope = id;
        return parent;
      }

      auto close_scope(const std::size_t parent) -> void
      {
        current_scope = parent;
      }

      auto declare(std::string declared_name, const symbol_kind kind, const parser::span range,
                   const symbol_visibility visibility = symbol_visibility::public_access,
                   const symbol_origin origin = symbol_origin::source,
                   std::vector<std::string> documentation = {}) -> void
      {
        auto &entries = names[current_scope][declared_name];
        if (!entries.empty())
        {
          const symbol &existing = model.scopes[current_scope].symbols[entries.front()];
          const auto callable = [](const symbol_kind value)
          {
            return value == symbol_kind::function || value == symbol_kind::method ||
                   value == symbol_kind::constructor || value == symbol_kind::enum_constructor;
          };
          const bool overload = callable(existing.kind) && callable(kind);
          if (!overload)
          {
            if (tolerant_) return;
            throw semantic_error("Duplicate declaration of '" + declared_name + "' in the same scope", range);
          }
        }
        const symbol_id id = make_id(declared_name, kind, origin, entries.size());
        entries.push_back(model.scopes[current_scope].symbols.size());
        model.scopes[current_scope].symbols.push_back(symbol{id, std::move(declared_name), kind, visibility,
                                                             origin, range, current_scope,
                                                             std::move(documentation)});
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

      auto resolve_name(const std::string &name, const parser::span range,
                        const reference_kind kind = reference_kind::read) -> void
      {
        const auto declaration = find(name);
        if (!declaration)
        {
          if (tolerant_) return;
          throw semantic_error("Undefined name '" + name + "'", range);
        }
        model.resolutions.push_back(resolution{name, range, declaration->declaration, declaration->id, kind});
      }

      auto resolve_member(const parser::member_expression &member, const reference_kind kind) -> void
      {
        const auto *target = dynamic_cast<const parser::identifier_expression *>(member.target.get());
        if (!target) return;
        if (const auto receiver = find(target->name);
            receiver && receiver->kind == symbol_kind::imported_namespace)
        {
          model.unresolved_members.push_back(
              unresolved_member_reference{receiver->id, member.member_name, member.range, kind});
          return;
        }
        if (const auto declared_type = type_scopes.find(target->name); declared_type != type_scopes.end())
        {
          const auto found = names[declared_type->second].find(member.member_name);
          if (found == names[declared_type->second].end()) return;
          const auto &declared = model.scopes[declared_type->second].symbols[found->second.front()];
          model.resolutions.push_back(resolution{member.member_name, member.range, declared.declaration,
                                                 declared.id, kind});
          return;
        }
        if (target->name != "self" || active_type_scopes.empty()) return;
        const auto scope_id = active_type_scopes.back();
        const auto found = names[scope_id].find(member.member_name);
        // The type checker owns inherited/default face members and produces the
        // actual validity diagnostic. The lexical analyzer links only members
        // declared directly in this type scope.
        if (found == names[scope_id].end()) return;
        const auto &declared = model.scopes[scope_id].symbols[found->second.front()];
        model.resolutions.push_back(resolution{member.member_name, member.range, declared.declaration,
                                               declared.id, kind});
      }

      auto resolve_type(const std::optional<std::string> &name, const parser::span range,
                        const reference_kind kind = reference_kind::type) -> void
      {
        if (name)
        {
          if (name->starts_with('('))
          {
            int parentheses = 0;
            std::size_t close = std::string::npos;
            for (std::size_t index = 0; index < name->size(); ++index)
            {
              if ((*name)[index] == '(') ++parentheses;
              else if ((*name)[index] == ')' && --parentheses == 0) { close = index; break; }
            }
            if (close == std::string::npos || name->substr(close, 4) != ") =>")
              throw semantic_error("Malformed function type annotation '" + *name + "'", range);
            const std::string parameters = name->substr(1, close - 1);
            std::size_t begin = 0;
            parentheses = 0;
            int angles = 0;
            for (std::size_t index = 0; index <= parameters.size(); ++index)
            {
              if (index < parameters.size() && parameters[index] == '(') ++parentheses;
              else if (index < parameters.size() && parameters[index] == ')') --parentheses;
              else if (index < parameters.size() && parameters[index] == '<') ++angles;
              else if (index < parameters.size() && parameters[index] == '>') --angles;
              if (index == parameters.size() ||
                  (parameters[index] == ',' && parentheses == 0 && angles == 0))
              {
                std::string parameter = parameters.substr(begin, index - begin);
                while (!parameter.empty() && parameter.front() == ' ') parameter.erase(parameter.begin());
                while (!parameter.empty() && parameter.back() == ' ') parameter.pop_back();
                if (!parameter.empty()) resolve_type(std::optional<std::string>{parameter}, range, kind);
                begin = index + 1;
              }
            }
            std::string result = name->substr(close + 4);
            while (!result.empty() && result.front() == ' ') result.erase(result.begin());
            resolve_type(std::optional<std::string>{result}, range, kind);
            return;
          }
          const std::size_t open = name->find('<');
          const std::string base = open == std::string::npos ? *name : name->substr(0, open);
          std::string lookup = base;
          for (const std::string_view family : {std::string_view{"SphericalVector"},
                                                std::string_view{"SphericalPoint"},
                                                std::string_view{"Vector"}, std::string_view{"Point"}})
            if (lookup.starts_with(family) && lookup.size() > family.size() &&
                std::all_of(lookup.begin() + static_cast<std::ptrdiff_t>(family.size()), lookup.end(),
                            [](const unsigned char value) { return value >= '0' && value <= '9'; }))
              lookup = family;
          const auto declaration = find(lookup);
          if (!declaration)
          {
            if (tolerant_) return;
            throw semantic_error("Undefined type '" + base + "'", range);
          }
          if (declaration->kind != symbol_kind::type && declaration->kind != symbol_kind::builtin_type &&
              declaration->kind != symbol_kind::type_parameter &&
              declaration->kind != symbol_kind::imported_namespace)
          {
            if (tolerant_) return;
            throw semantic_error("'" + base + "' does not name a type", range);
          }
          model.resolutions.push_back(resolution{lookup, range, declaration->declaration, declaration->id, kind});
          if (open != std::string::npos)
          {
            if (!name->ends_with('>')) throw semantic_error("Malformed type annotation '" + *name + "'", range);
            const std::string arguments = name->substr(open + 1, name->size() - open - 2);
            const bool measured_scalar = lookup.starts_with("Int") || lookup.starts_with("Float");
            const bool dimensioned_type = lookup == "Vector" || lookup == "Point" ||
                                          lookup == "SphericalVector" || lookup == "SphericalPoint";
            std::size_t begin = 0;
            std::size_t argument_index = 0;
            int depth = 0;
            for (std::size_t index = 0; index <= arguments.size(); ++index)
            {
              if (index < arguments.size() && arguments[index] == '<') ++depth;
              else if (index < arguments.size() && arguments[index] == '>') --depth;
              if (index == arguments.size() || (arguments[index] == ',' && depth == 0))
              {
                std::string argument = arguments.substr(begin, index - begin);
                while (!argument.empty() && argument.front() == ' ') argument.erase(argument.begin());
                if (!measured_scalar && (!dimensioned_type || argument_index == 0))
                  resolve_type(std::optional<std::string>{argument}, range);
                begin = index + 1;
                ++argument_index;
              }
            }
          }
        }
      }

      auto resolve_explicit_generic(const std::string &name, const parser::span range,
                                    const bool resolve_base = true) -> void
      {
        const std::size_t open = name.find('<');
        if (open == std::string::npos)
        {
          resolve_name(name, range, reference_kind::call);
          return;
        }
        const std::string base = name.substr(0, open);
        if (resolve_base) resolve_name(base, range, reference_kind::call);
        const std::string arguments = name.substr(open + 1, name.size() - open - 2);
        std::vector<std::string> specialization_arguments;
        std::size_t begin = 0;
        int depth = 0;
        for (std::size_t index = 0; index <= arguments.size(); ++index)
        {
          if (index < arguments.size() && arguments[index] == '<') ++depth;
          else if (index < arguments.size() && arguments[index] == '>') --depth;
          if (index == arguments.size() || (arguments[index] == ',' && depth == 0))
          {
            std::string argument = arguments.substr(begin, index - begin);
            while (!argument.empty() && argument.front() == ' ') argument.erase(argument.begin());
            while (!argument.empty() && argument.back() == ' ') argument.pop_back();
            specialization_arguments.push_back(argument);
            resolve_type(std::optional<std::string>{argument}, range);
            begin = index + 1;
          }
        }
        if (resolve_base)
          if (const auto generic = find(base))
            model.specializations.push_back(generic_specialization{generic->id,
                                                                    std::move(specialization_arguments), range});
      }

      auto predeclare(const parser::statement &value) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          const bool constant = dynamic_cast<const parser::const_declaration *>(declaration);
          declare(declaration->name,
                  current_scope == 0 ? (constant ? symbol_kind::constant : symbol_kind::variable)
                                     : (constant ? symbol_kind::constant_field : symbol_kind::field),
                  declaration->range,
                  declaration->private_member ? symbol_visibility::private_access
                                              : symbol_visibility::public_access,
                  symbol_origin::source, documentation(declaration->documentation));
        }
        else if (const auto *function = dynamic_cast<const parser::function_declaration *>(&value))
        {
          declare(function->name, function->constructor_member ? symbol_kind::constructor
                                                                : (current_scope == 0 ? symbol_kind::function
                                                                                      : symbol_kind::method),
                  function->range,
                  function->private_member ? symbol_visibility::private_access
                                           : symbol_visibility::public_access,
                  symbol_origin::source, documentation(function->documentation));
        }
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(&value))
        {
          declare(type->name, symbol_kind::type, type->range, symbol_visibility::public_access,
                  symbol_origin::source, documentation(type->documentation));
          if (type->type_kind == parser::type_declaration::kind::enum_type)
            for (const auto &member : type->enum_members)
              if (!member.payload_types.empty()) declare(member.name, symbol_kind::enum_constructor, member.range);
        }
        else if (const auto *imported = dynamic_cast<const parser::import_declaration *>(&value))
        {
          const std::size_t separator = imported->imported_name.rfind('.');
          const std::string fallback = separator == std::string::npos
                                           ? imported->imported_name
                                           : imported->imported_name.substr(separator + 1);
          declare(imported->alias.value_or(fallback), symbol_kind::imported_namespace, imported->range,
                  symbol_visibility::public_access, symbol_origin::imported,
                  documentation(imported->documentation));
        }
        else if (const auto *measurement = dynamic_cast<const parser::measurement_declaration *>(&value))
        {
          const auto kind = measurement->declaration_kind == parser::measurement_declaration::kind::dimension
                                ? symbol_kind::dimension
                            : measurement->declaration_kind == parser::measurement_declaration::kind::quantity
                                ? symbol_kind::quantity
                                : symbol_kind::unit;
          declare(measurement->name, kind, measurement->range, symbol_visibility::public_access,
                  symbol_origin::source, documentation(measurement->documentation));
        }
      }

      auto expression(const parser::expression &value) -> void
      {
        if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          const std::size_t generic = identifier->name.find('<');
          if (generic == std::string::npos) resolve_name(identifier->name, identifier->range);
          else resolve_type(std::optional<std::string>{identifier->name}, identifier->range);
        }
        else if (const auto *grouping = dynamic_cast<const parser::grouping_expression *>(&value))
        {
          expression(*grouping->value);
        }
        else if (const auto *measured = dynamic_cast<const parser::measured_expression *>(&value))
        {
          expression(*measured->value);
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
          if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(call->callee.get()))
            resolve_explicit_generic(identifier->name, identifier->range);
          else if (const auto *member = dynamic_cast<const parser::member_expression *>(call->callee.get()))
          {
            expression(*member->target);
            resolve_member(*member, reference_kind::call);
            if (member->member_name.find('<') != std::string::npos)
              resolve_explicit_generic(member->member_name, member->range, false);
          }
          else expression(*call->callee);
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
          resolve_member(*member, reference_kind::read);
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
          const std::size_t parent = open_scope("lambda", lambda->range);
          for (const auto &parameter : lambda->parameters)
          {
            resolve_type(parameter.type_name, lambda->range);
            declare(parameter.name, symbol_kind::parameter, lambda->range);
          }
          resolve_type(lambda->return_type, lambda->range);
          expression(*lambda->body);
          close_scope(parent);
        }
      }

      auto block(const parser::block_statement &value, std::string label = "block") -> void
      {
        const std::size_t parent = open_scope(std::move(label), value.range);
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
        const std::size_t parent = open_scope("function " + value.name, value.range);
        for (const auto &parameter : value.type_parameters)
          declare(parameter, symbol_kind::type_parameter, value.range);
        for (const auto &constraint : value.type_constraints) resolve_type(constraint, value.range);
        for (const auto &parameter : value.parameters)
        {
          resolve_type(parameter.type_name, value.range);
          declare(parameter.name, symbol_kind::parameter, value.range);
        }
        resolve_type(value.return_type, value.range);
        if (value.body) block(*value.body, "function body");
        if (value.expression_body) expression(*value.expression_body);
        close_scope(parent);
      }

      auto type(const parser::type_declaration &value) -> void
      {
        const std::size_t parent = open_scope("type " + value.name, value.range);
        type_scopes.insert_or_assign(value.name, current_scope);
        active_type_scopes.push_back(current_scope);
        for (const auto &parameter : value.type_parameters)
          declare(parameter, symbol_kind::type_parameter, value.range);
        for (const auto &constraint : value.type_constraints) resolve_type(constraint, value.range);
        for (const auto &interface_name : value.composed_interfaces)
        {
          resolve_type(std::optional<std::string>{interface_name}, value.range, reference_kind::conformance);
        }
        if (value.type_kind == parser::type_declaration::kind::class_type ||
            value.type_kind == parser::type_declaration::kind::interface_type)
        {
          declare("self", symbol_kind::self_value, value.range, symbol_visibility::public_access,
                  symbol_origin::generated);
        }
        for (const auto &member : value.members) predeclare(*member);
        for (const auto &member : value.enum_members)
        {
          declare(member.name, symbol_kind::enum_case, member.range, symbol_visibility::public_access,
                  symbol_origin::source, documentation(member.documentation));
          for (const auto &payload : member.payload_types)
            resolve_type(std::optional<std::string>{payload}, member.range);
        }
        for (const auto &member : value.members) statement(*member, true);
        active_type_scopes.pop_back();
        close_scope(parent);
      }

      auto statement(const parser::statement &value, const bool already_declared) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          resolve_type(declaration->type_name, declaration->range);
          if (declaration->initializer) expression(*declaration->initializer);
          if (!already_declared) declare(declaration->name,
                                         dynamic_cast<const parser::const_declaration *>(declaration)
                                             ? symbol_kind::constant : symbol_kind::variable,
                                         declaration->range,
                                         declaration->private_member ? symbol_visibility::private_access
                                                                     : symbol_visibility::public_access,
                                         symbol_origin::source, documentation(declaration->documentation));
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
          const std::size_t parent = open_scope("for loop", loop->range);
          declare(loop->binding, symbol_kind::loop_binding, loop->range);
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
            const bool binding_pattern = callee && call &&
                std::all_of(call->arguments.begin(), call->arguments.end(), [](const auto &argument)
                {
                  return dynamic_cast<const parser::identifier_expression *>(argument.get()) != nullptr;
                });
            if (branch.pattern && !binding_pattern) expression(*branch.pattern);
            const std::size_t parent = open_scope("match case", branch.body->range);
            if (binding_pattern)
            {
              resolve_name(callee->name, callee->range);
              for (const auto &argument : call->arguments)
              {
                const auto &binding = dynamic_cast<const parser::identifier_expression &>(*argument);
                declare(binding.name, symbol_kind::match_binding, binding.range);
              }
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
        else if (dynamic_cast<const parser::measurement_declaration *>(&value))
        {
          // Validated by the unit registry in the type checker.
        }
        else if (const auto *exported = dynamic_cast<const parser::export_declaration *>(&value))
        {
          resolve_name(exported->exported_name, exported->range, reference_kind::export_reference);
        }
      }

    public:
      explicit analysis(analysis_identity identity, const bool tolerant = false)
          : identity_(std::move(identity)), tolerant_(tolerant)
      {
        model.scopes.push_back(scope{0, no_parent, "program", {}, {}});
        names.emplace_back();
        for (const std::string_view builtin : {"Bool", "Float", "Float32", "Float64", "Frame",
                                               "Int", "Int8", "Int16", "Int32", "Int64", "Optional", "RuntimeError", "String",
                                               "Point", "SphericalPoint", "SphericalVector", "Vector", "Void"})
        {
          declare(std::string(builtin), symbol_kind::builtin_type, parser::span{0, 0},
                  symbol_visibility::public_access, symbol_origin::builtin);
        }
        declare("print", symbol_kind::function, parser::span{0, 0}, symbol_visibility::public_access,
                symbol_origin::builtin);
        declare("Some", symbol_kind::function, parser::span{0, 0}, symbol_visibility::public_access,
                symbol_origin::builtin);
        declare("None", symbol_kind::builtin_value, parser::span{0, 0}, symbol_visibility::public_access,
                symbol_origin::builtin);
      }

      auto run(const parser::program &tree) -> semantic_model
      {
        model.scopes.front().range = tree.range;
        for (const auto &entry : tree.statements) predeclare(*entry);
        for (const auto &entry : tree.statements) statement(*entry, true);
        return std::move(model);
      }
    };
  }

  auto name(const symbol_kind value) -> std::string_view
  {
    switch (value)
    {
    case symbol_kind::module: return "module";
    case symbol_kind::imported_namespace: return "import";
    case symbol_kind::variable: return "variable";
    case symbol_kind::constant: return "constant";
    case symbol_kind::parameter: return "parameter";
    case symbol_kind::loop_binding: return "loop binding";
    case symbol_kind::match_binding: return "match binding";
    case symbol_kind::function: return "function";
    case symbol_kind::constructor: return "constructor";
    case symbol_kind::type: return "type";
    case symbol_kind::type_parameter: return "type parameter";
    case symbol_kind::dimension: return "dimension";
    case symbol_kind::quantity: return "quantity";
    case symbol_kind::unit: return "unit";
    case symbol_kind::field: return "field";
    case symbol_kind::constant_field: return "constant field";
    case symbol_kind::method: return "method";
    case symbol_kind::enum_case: return "enum member";
    case symbol_kind::enum_constructor: return "enum constructor";
    case symbol_kind::self_value: return "self";
    case symbol_kind::builtin_type: return "builtin type";
    case symbol_kind::builtin_value: return "builtin value";
    }
    return "unknown";
  }

  auto rename_preserves_binding_convention(const symbol_kind kind,
                                           const std::string_view proposed) -> bool
  {
    if (proposed.empty()) return false;
    if (kind == symbol_kind::constant || kind == symbol_kind::constant_field)
      return parser::is_constant_name(proposed);
    if (kind == symbol_kind::variable || kind == symbol_kind::field)
      return !parser::is_constant_name(proposed);
    return true;
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
        stream << "    Symbol(" << name(declared.kind) << " " << declared.name << " @ "
               << declared.declaration.begin << ".." << declared.declaration.end << ")\n";
      }
    }
    stream << "  Resolutions\n";
    for (const auto &resolved : resolutions)
    {
      stream << "    " << resolved.name << " @ " << resolved.use.begin << ".." << resolved.use.end
             << " -> " << resolved.declaration.begin << ".." << resolved.declaration.end << '\n';
    }
    for (const auto &specialization : specializations)
    {
      stream << "    Specialization(" << specialization.generic.value << " @ "
             << specialization.use.begin << ".." << specialization.use.end << ")\n";
    }
  }

  auto analyze(const parser::program &tree, analysis_identity identity) -> semantic_model
  {
    return analysis(std::move(identity)).run(tree);
  }

  auto analyze_partial(const parser::program &tree, analysis_identity identity) -> semantic_model
  {
    return analysis(std::move(identity), true).run(tree);
  }
}
