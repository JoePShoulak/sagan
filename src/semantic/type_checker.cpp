#include "type_checker.hpp"

#include "semantic_error.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace semantic
{
  namespace
  {
    constexpr std::string_view unknown_type = "Unknown";
    constexpr std::string_view void_type = "Void";

    struct callable_signature
    {
      std::vector<std::string> parameters;
      std::string result;
    };

    struct binding
    {
      std::string type;
      std::optional<callable_signature> callable;
      bool initialized = true;
    };

    struct dimensioned_type
    {
      std::string family;
      std::size_t dimensions;
      std::string component;
    };

    auto is_unknown(const std::string_view type) -> bool
    {
      return type == unknown_type || type.find("Unknown") != std::string_view::npos;
    }

    auto is_numeric(const std::string_view type) -> bool
    {
      return type.starts_with("Int") || type.starts_with("Float");
    }

    auto integer_width(const std::string_view type) -> int
    {
      if (type == "Int8") return 8;
      if (type == "Int16") return 16;
      if (type == "Int32") return 32;
      if (type == "Int64") return 64;
      return type == "Int" ? 0 : -1;
    }

    auto float_width(const std::string_view type) -> int
    {
      if (type == "Float32") return 32;
      if (type == "Float64") return 64;
      return type == "Float" ? 0 : -1;
    }

    auto inferred_integer_type(const std::string &spelling, const bool negative,
                               const parser::span range) -> std::string
    {
      std::string digits;
      for (const char character : spelling)
      {
        if (character != '_') digits.push_back(character);
      }
      std::uint64_t magnitude = 0;
      const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude);
      if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size())
      {
        throw semantic_error("Integer literal is outside the supported Int64 range", range);
      }
      const std::uint64_t int8_limit = negative ? 128ULL : 127ULL;
      const std::uint64_t int16_limit = negative ? 32768ULL : 32767ULL;
      const std::uint64_t int32_limit = negative ? 2147483648ULL : 2147483647ULL;
      const std::uint64_t int64_limit = negative ? (std::uint64_t{1} << 63U)
                                                 : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
      if (magnitude <= int8_limit) return "Int8";
      if (magnitude <= int16_limit) return "Int16";
      if (magnitude <= int32_limit) return "Int32";
      if (magnitude <= int64_limit) return "Int64";
      throw semantic_error("Integer literal is outside the supported Int64 range", range);
    }

    auto array_element(const std::string_view type) -> std::optional<std::string>
    {
      constexpr std::string_view prefix = "Array<";
      if (!type.starts_with(prefix) || !type.ends_with('>')) return {};
      return std::string(type.substr(prefix.size(), type.size() - prefix.size() - 1));
    }

    auto dictionary_types(const std::string_view type) -> std::optional<std::pair<std::string, std::string>>
    {
      constexpr std::string_view prefix = "Dictionary<";
      if (!type.starts_with(prefix) || !type.ends_with('>')) return {};
      const std::string_view contents = type.substr(prefix.size(), type.size() - prefix.size() - 1);
      int depth = 0;
      for (std::size_t index = 0; index < contents.size(); ++index)
      {
        if (contents[index] == '<') ++depth;
        else if (contents[index] == '>') --depth;
        else if (contents[index] == ',' && depth == 0)
        {
          std::string key(contents.substr(0, index));
          std::string mapped(contents.substr(index + 1));
          if (!mapped.empty() && mapped.front() == ' ') mapped.erase(mapped.begin());
          return std::pair{std::move(key), std::move(mapped)};
        }
      }
      return {};
    }

    auto dimensioned(const std::string_view type) -> std::optional<dimensioned_type>
    {
      std::string family;
      if (type.starts_with("Vector")) family = "Vector";
      else if (type.starts_with("Coordinate")) family = "Coordinate";
      else return {};
      const std::size_t digits_begin = family.size();
      const std::size_t angle = type.find('<', digits_begin);
      if (angle == std::string_view::npos || !type.ends_with('>') || angle == digits_begin) return {};
      std::size_t dimensions = 0;
      const auto parsed = std::from_chars(type.data() + digits_begin, type.data() + angle, dimensions);
      if (parsed.ec != std::errc{} || parsed.ptr != type.data() + angle) return {};
      return dimensioned_type{std::move(family), dimensions,
                              std::string(type.substr(angle + 1, type.size() - angle - 2))};
    }

    class type_analysis
    {
      type_model model;
      std::vector<std::unordered_map<std::string, std::vector<binding>>> scopes{1};
      std::vector<std::string> return_types;

      auto open_scope() -> void
      {
        scopes.emplace_back();
      }

      auto close_scope() -> void
      {
        scopes.pop_back();
      }

      auto add_binding(const std::string &name, binding value) -> void
      {
        scopes.back()[name].push_back(std::move(value));
      }

      auto find(const std::string &name) const -> const std::vector<binding> *
      {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
        {
          const auto found = scope->find(name);
          if (found != scope->end()) return &found->second;
        }
        return nullptr;
      }

      auto find_mutable(const std::string &name) -> std::vector<binding> *
      {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
        {
          const auto found = scope->find(name);
          if (found != scope->end()) return &found->second;
        }
        return nullptr;
      }

      using scope_state = std::vector<std::unordered_map<std::string, std::vector<binding>>>;

      auto merge_initialization(const scope_state &before,
                                const std::vector<scope_state> &paths) -> void
      {
        scopes = before;
        for (std::size_t scope_index = 0; scope_index < scopes.size(); ++scope_index)
        {
          for (auto &[name, overloads] : scopes[scope_index])
          {
            for (std::size_t binding_index = 0; binding_index < overloads.size(); ++binding_index)
            {
              bool initialized = true;
              for (const auto &path : paths)
              {
                initialized &= path[scope_index].at(name)[binding_index].initialized;
              }
              overloads[binding_index].initialized = initialized;
            }
          }
        }
      }

      auto record(const parser::expression &value, std::string type) -> std::string
      {
        model.expressions.push_back(typed_expression{value.range, type});
        return type;
      }

      auto require(const bool condition, const std::string &message, const parser::span range) const -> void
      {
        if (!condition) throw semantic_error(message, range);
      }

      auto compatible(const std::string_view expected, const std::string_view actual) const -> bool
      {
        if (is_unknown(expected) || is_unknown(actual) || expected == actual) return true;
        const int expected_width = integer_width(expected);
        const int actual_width = integer_width(actual);
        if (expected_width == 0 && actual_width >= 0) return true;
        if (expected_width > 0 && actual_width > 0) return actual_width <= expected_width;
        const int expected_float_width = float_width(expected);
        const int actual_float_width = float_width(actual);
        if (expected_float_width == 0 && actual_float_width >= 0) return true;
        if (expected_float_width > 0 && actual_float_width > 0)
        {
          return actual_float_width <= expected_float_width;
        }
        if (expected_float_width >= 0 && actual_width > 0)
        {
          const int effective_float_width = expected_float_width == 0 ? 64 : expected_float_width;
          return (effective_float_width == 32 && actual_width <= 16) ||
                 (effective_float_width == 64 && actual_width <= 32);
        }
        const auto actual_dimensioned = dimensioned(actual);
        if ((expected == "Vector" || expected == "Coordinate") && actual_dimensioned)
        {
          return actual_dimensioned->family == expected;
        }
        const auto expected_dimensioned = dimensioned(expected);
        if (expected_dimensioned && actual_dimensioned)
        {
          return expected_dimensioned->family == actual_dimensioned->family &&
                 expected_dimensioned->dimensions == actual_dimensioned->dimensions &&
                 compatible(expected_dimensioned->component, actual_dimensioned->component);
        }
        return false;
      }

      auto common_type(const std::string &left, const std::string &right,
                       const parser::span range, const std::string_view context) const -> std::string
      {
        if (is_unknown(left)) return right;
        if (is_unknown(right)) return left;
        if (left == right) return left;
        const int left_width = integer_width(left);
        const int right_width = integer_width(right);
        if (left_width >= 0 && right_width >= 0)
        {
          if (left_width == 0 || right_width == 0) return "Int";
          return left_width > right_width ? left : right;
        }
        const int left_float_width = float_width(left);
        const int right_float_width = float_width(right);
        if (left_float_width >= 0 && right_float_width >= 0)
        {
          if (left_float_width == 0 || right_float_width == 0) return "Float";
          return left_float_width > right_float_width ? left : right;
        }
        if (left_float_width >= 0 && right_width > 0 && compatible(left, right)) return left;
        if (right_float_width >= 0 && left_width > 0 && compatible(right, left)) return right;
        const auto left_dimensioned = dimensioned(left);
        const auto right_dimensioned = dimensioned(right);
        if (left_dimensioned && right_dimensioned && left_dimensioned->family == right_dimensioned->family &&
            left_dimensioned->dimensions == right_dimensioned->dimensions)
        {
          return left_dimensioned->family + std::to_string(left_dimensioned->dimensions) + "<" +
                 common_type(left_dimensioned->component, right_dimensioned->component, range,
                             "Vector components") + ">";
        }
        require(false, std::string(context) + " have incompatible types " + left + " and " + right, range);
        return std::string(unknown_type);
      }

      auto require_compatible(const std::string &expected, const std::string &actual,
                              const parser::span range, const std::string_view context) const -> void
      {
        require(compatible(expected, actual),
                std::string(context) + " requires " + expected + ", but received " + actual, range);
      }

      auto annotation(const std::optional<std::string> &name) const -> std::string
      {
        return name.value_or(std::string(unknown_type));
      }

      auto fixed_annotation(const std::optional<std::string> &name) const -> std::string
      {
        if (!name) return std::string(unknown_type);
        if (*name == "Int") return "Int64";
        if (*name == "Float") return "Float64";
        return *name;
      }

      auto signature(const parser::function_declaration &function) const -> callable_signature
      {
        callable_signature result;
        for (const auto &parameter : function.parameters) result.parameters.push_back(fixed_annotation(parameter.type_name));
        result.result = fixed_annotation(function.return_type);
        return result;
      }

      auto predeclare(const parser::statement &value) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          add_binding(declaration->name,
                      binding{annotation(declaration->type_name), {}, false});
        }
        else if (const auto *function = dynamic_cast<const parser::function_declaration *>(&value))
        {
          const auto declared_signature = signature(*function);
          add_binding(function->name, binding{"Function", declared_signature});
        }
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(&value))
        {
          add_binding(type->name, binding{"Type", {}});
        }
        else if (const auto *imported = dynamic_cast<const parser::import_declaration *>(&value))
        {
          add_binding(imported->alias.value_or(imported->imported_name), binding{std::string(unknown_type), {}});
        }
      }

      auto expression(const parser::expression &value) -> std::string
      {
        if (const auto *literal = dynamic_cast<const parser::literal_expression *>(&value))
        {
          const std::string type = literal->literal_kind == parser::literal_expression::kind::integer
                                       ? inferred_integer_type(literal->spelling, false, literal->range)
                                   : literal->literal_kind == parser::literal_expression::kind::floating_point
                                       ? "Float64"
                                       : "Bool";
          return record(value, type);
        }
        if (dynamic_cast<const parser::string_expression *>(&value))
        {
          const auto &string = static_cast<const parser::string_expression &>(value);
          for (const auto &part : string.parts)
          {
            if (part.interpolation) static_cast<void>(expression(*part.interpolation));
          }
          return record(value, "String");
        }
        if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          const auto *matches = find(identifier->name);
          require(matches && !matches->empty(), "Undefined name '" + identifier->name + "'", value.range);
          require(matches->front().initialized, "Variable '" + identifier->name + "' is used before initialization",
                  value.range);
          return record(value, matches->front().type);
        }
        if (const auto *grouping = dynamic_cast<const parser::grouping_expression *>(&value))
        {
          return record(value, expression(*grouping->value));
        }
        if (const auto *unary = dynamic_cast<const parser::unary_expression *>(&value))
        {
          if (unary->operator_text == "-" && !unary->postfix)
          {
            if (const auto *literal = dynamic_cast<const parser::literal_expression *>(unary->operand.get());
                literal && literal->literal_kind == parser::literal_expression::kind::integer)
            {
              static_cast<void>(record(*literal, inferred_integer_type(literal->spelling, true, literal->range)));
              return record(value, inferred_integer_type(literal->spelling, true, value.range));
            }
          }
          const std::string operand = expression(*unary->operand);
          if (unary->operator_text == "!")
          {
            require_compatible("Bool", operand, value.range, "Logical negation");
            return record(value, "Bool");
          }
          require(is_unknown(operand) || is_numeric(operand),
                  "Operator '" + unary->operator_text + "' requires a numeric operand, but received " + operand,
                  value.range);
          return record(value, operand);
        }
        if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
        {
          const std::string left = expression(*binary->left);
          const std::string right = expression(*binary->right);
          if (binary->operator_text == "and" || binary->operator_text == "or")
          {
            require_compatible("Bool", left, binary->left->range, "Logical operator");
            require_compatible("Bool", right, binary->right->range, "Logical operator");
            return record(value, "Bool");
          }
          if (binary->operator_text == "==" || binary->operator_text == "!=" ||
              binary->operator_text == "<" || binary->operator_text == "<=" ||
              binary->operator_text == ">" || binary->operator_text == ">=")
          {
            static_cast<void>(common_type(left, right, value.range, "Comparison operands"));
            return record(value, "Bool");
          }
          require((is_unknown(left) || is_numeric(left)) && (is_unknown(right) || is_numeric(right)),
                  "Operator '" + binary->operator_text + "' requires numeric operands, but received " +
                      left + " and " + right,
                  value.range);
          return record(value, common_type(left, right, value.range, "Operator operands"));
        }
        if (const auto *conditional = dynamic_cast<const parser::conditional_expression *>(&value))
        {
          const std::string condition = expression(*conditional->condition);
          require_compatible("Bool", condition, conditional->condition->range, "Conditional expression");
          const std::string when_true = expression(*conditional->when_true);
          const std::string when_false = expression(*conditional->when_false);
          return record(value, common_type(when_true, when_false, value.range, "Conditional branches"));
        }
        if (const auto *assignment = dynamic_cast<const parser::assignment_expression *>(&value))
        {
          const std::string target = assignment_target(*assignment->target);
          const std::string assigned = expression(*assignment->value);
          require_compatible(target, assigned, value.range, "Assignment");
          mark_initialized(*assignment->target);
          return record(value, target);
        }
        if (const auto *call = dynamic_cast<const parser::call_expression *>(&value))
        {
          std::vector<std::string> arguments;
          for (const auto &argument : call->arguments) arguments.push_back(expression(*argument));
          if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(call->callee.get()))
          {
            const auto *matches = find(identifier->name);
            require(matches, "Undefined name '" + identifier->name + "'", identifier->range);
            std::vector<const callable_signature *> viable;
            for (const auto &candidate : *matches)
            {
              if (!candidate.callable || candidate.callable->parameters.size() != arguments.size()) continue;
              bool matches_arguments = true;
              for (std::size_t index = 0; index < arguments.size(); ++index)
              {
                matches_arguments &= compatible(candidate.callable->parameters[index], arguments[index]);
              }
              if (matches_arguments) viable.push_back(&*candidate.callable);
            }
            require(!viable.empty(), "No matching overload for '" + identifier->name + "'", value.range);
            require(viable.size() == 1, "Ambiguous overload for '" + identifier->name + "'", value.range);
            static_cast<void>(expression(*call->callee));
            return record(value, viable.front()->result);
          }
          static_cast<void>(expression(*call->callee));
          return record(value, std::string(unknown_type));
        }
        if (const auto *index = dynamic_cast<const parser::index_expression *>(&value))
        {
          const std::string target = expression(*index->target);
          const std::string index_type = expression(*index->index);
          if (const auto element = array_element(target))
          {
            require(integer_width(index_type) >= 0 || is_unknown(index_type),
                    "Array index requires Int, but received " + index_type, index->index->range);
            return record(value, *element);
          }
          if (const auto types = dictionary_types(target))
          {
            require_compatible(types->first, index_type, index->index->range, "Dictionary key");
            return record(value, types->second);
          }
          if (const auto shaped = dimensioned(target))
          {
            require(integer_width(index_type) >= 0 || is_unknown(index_type),
                    shaped->family + " index requires Int, but received " + index_type, index->index->range);
            return record(value, shaped->component);
          }
          return record(value, std::string(unknown_type));
        }
        if (const auto *member = dynamic_cast<const parser::member_expression *>(&value))
        {
          static_cast<void>(expression(*member->target));
          return record(value, std::string(unknown_type));
        }
        if (const auto *spread = dynamic_cast<const parser::spread_expression *>(&value))
        {
          return record(value, expression(*spread->value));
        }
        if (const auto *collection = dynamic_cast<const parser::collection_expression *>(&value))
        {
          if (collection->collection_kind == parser::collection_expression::kind::array)
          {
            require(!collection->elements.empty(),
                    "Empty array requires an explicit element type, which is not implemented yet", value.range);
            std::optional<std::string> element_type;
            for (const auto &element : collection->elements)
            {
              std::string current = expression(*element);
              if (dynamic_cast<const parser::spread_expression *>(element.get()))
              {
                const auto spread_element = array_element(current);
                require(spread_element.has_value(), "Array spread requires another array, but received " + current,
                        element->range);
                current = *spread_element;
              }
              element_type = element_type ? common_type(*element_type, current, element->range, "Array elements")
                                          : current;
            }
            return record(value, "Array<" + *element_type + ">");
          }
          const std::string family = collection->collection_kind == parser::collection_expression::kind::vector
                                         ? "Vector"
                                         : "Coordinate";
          std::optional<std::string> component_type;
          std::size_t dimensions = 0;
          for (const auto &element : collection->elements)
          {
            std::string current = expression(*element);
            std::size_t contribution = 1;
            if (dynamic_cast<const parser::spread_expression *>(element.get()))
            {
              const auto spread_type = dimensioned(current);
              require(spread_type && spread_type->family == family,
                      family + " spread requires another " + family + ", but received " + current,
                      element->range);
              current = spread_type->component;
              contribution = spread_type->dimensions;
            }
            require(is_numeric(current), family + " components must be numeric, but received " + current,
                    element->range);
            component_type = component_type
                                 ? common_type(*component_type, current, element->range, family + " components")
                                 : current;
            dimensions += contribution;
          }
          return record(value, family + std::to_string(dimensions) + "<" + *component_type + ">");
        }
        if (const auto *dictionary = dynamic_cast<const parser::dictionary_expression *>(&value))
        {
          require(!dictionary->entries.empty(),
                  "Empty dictionary requires explicit key and value types, which are not implemented yet",
                  value.range);
          std::optional<std::string> key_type;
          std::optional<std::string> mapped_type;
          for (const auto &entry : dictionary->entries)
          {
            if (!entry.key)
            {
              const std::string spread = expression(*entry.value);
              const auto spread_types = dictionary_types(spread);
              require(spread_types.has_value(),
                      "Dictionary spread requires another dictionary, but received " + spread,
                      entry.value->range);
              key_type = key_type ? common_type(*key_type, spread_types->first, entry.value->range,
                                                "Dictionary keys")
                                  : spread_types->first;
              mapped_type = mapped_type ? common_type(*mapped_type, spread_types->second, entry.value->range,
                                                      "Dictionary values")
                                        : spread_types->second;
              continue;
            }
            const std::string key = expression(*entry.key);
            const std::string mapped = expression(*entry.value);
            key_type = key_type ? common_type(*key_type, key, entry.key->range, "Dictionary keys") : key;
            mapped_type = mapped_type ? common_type(*mapped_type, mapped, entry.value->range, "Dictionary values")
                                      : mapped;
          }
          return record(value, "Dictionary<" + *key_type + ", " + *mapped_type + ">");
        }
        if (const auto *lambda = dynamic_cast<const parser::lambda_expression *>(&value))
        {
          open_scope();
          for (const auto &parameter : lambda->parameters)
          {
            add_binding(parameter.name, binding{fixed_annotation(parameter.type_name), {}});
          }
          const std::string body = expression(*lambda->body);
          const std::string result = fixed_annotation(lambda->return_type);
          if (!is_unknown(result)) require_compatible(result, body, value.range, "Lambda return");
          close_scope();
          return record(value, "Function");
        }
        return record(value, std::string(unknown_type));
      }

      auto block(const parser::block_statement &value) -> void
      {
        open_scope();
        for (const auto &entry : value.statements) statement(*entry, false);
        close_scope();
      }

      auto assignment_target(const parser::expression &value) -> std::string
      {
        if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          const auto *matches = find(identifier->name);
          require(matches && !matches->empty(), "Undefined name '" + identifier->name + "'", value.range);
          return matches->front().type;
        }
        if (dynamic_cast<const parser::member_expression *>(&value) ||
            dynamic_cast<const parser::index_expression *>(&value))
        {
          return expression(value);
        }
        throw semantic_error("Assignment target is not assignable", value.range);
      }

      auto mark_initialized(const parser::expression &value) -> void
      {
        if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          auto *matches = find_mutable(identifier->name);
          if (matches && !matches->empty()) matches->front().initialized = true;
        }
      }

      auto function(const parser::function_declaration &value) -> void
      {
        open_scope();
        for (const auto &parameter : value.parameters)
        {
          const std::string type = fixed_annotation(parameter.type_name);
          add_binding(parameter.name, binding{type, {}});
          model.declarations.push_back(typed_declaration{value.range, parameter.name, type});
        }
        return_types.push_back(fixed_annotation(value.return_type));
        if (value.body) block(*value.body);
        if (value.expression_body)
        {
          const std::string body = expression(*value.expression_body);
          if (!is_unknown(return_types.back()))
          {
            require_compatible(return_types.back(), body, value.range, "Function return");
          }
        }
        const bool definitely_returns = value.body && block_returns(*value.body);
        if (value.body && value.return_type && *value.return_type != "Void" && !definitely_returns)
        {
          throw semantic_error("Function '" + value.name + "' may reach the end without returning " +
                                   fixed_annotation(value.return_type),
                               value.range);
        }
        return_types.pop_back();
        close_scope();
      }

      auto statement_returns(const parser::statement &value) const -> bool
      {
        if (dynamic_cast<const parser::return_statement *>(&value)) return true;
        if (const auto *block_value = dynamic_cast<const parser::block_statement *>(&value))
        {
          return block_returns(*block_value);
        }
        if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          return conditional->else_branch && block_returns(*conditional->then_branch) &&
                 statement_returns(*conditional->else_branch);
        }
        if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          bool has_fallback = false;
          for (const auto &branch : matched->cases)
          {
            has_fallback |= !branch.pattern;
            if (!block_returns(*branch.body)) return false;
          }
          return has_fallback;
        }
        return false;
      }

      auto block_returns(const parser::block_statement &value) const -> bool
      {
        bool returned = false;
        for (const auto &entry : value.statements)
        {
          if (returned) throw semantic_error("Unreachable statement", entry->range);
          returned = statement_returns(*entry);
        }
        return returned;
      }

      auto statement(const parser::statement &value, const bool predeclared) -> void
      {
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          const std::string declared = annotation(declaration->type_name);
          std::string inferred = declaration->initializer ? expression(*declaration->initializer)
                                                           : std::string(unknown_type);
          if (declared == "Float32" && declaration->initializer)
          {
            if (const auto *literal = dynamic_cast<const parser::literal_expression *>(declaration->initializer.get());
                literal && literal->literal_kind == parser::literal_expression::kind::floating_point)
            {
              inferred = "Float32";
            }
          }
          if (!is_unknown(declared) && declaration->initializer)
          {
            require_compatible(declared, inferred, declaration->range, "Variable initializer");
          }
          const std::string type = !declaration->initializer && declared == "Int"
                                       ? "Int64"
                                   : !declaration->initializer && declared == "Float"
                                       ? "Float64"
                                   : (declared == "Int" && integer_width(inferred) > 0) ||
                                           (declared == "Float" && float_width(inferred) > 0)
                                       ? inferred
                                   : is_unknown(declared) ? inferred
                                                          : declared;
          if (!predeclared)
          {
            add_binding(declaration->name, binding{type, {}, declaration->initializer != nullptr});
          }
          else
          {
            scopes.back()[declaration->name].front().type = type;
            scopes.back()[declaration->name].front().initialized = declaration->initializer != nullptr;
          }
          require(!is_unknown(type),
                  "Variable '" + declaration->name + "' requires a type annotation or initializer",
                  declaration->range);
          model.declarations.push_back(typed_declaration{declaration->range, declaration->name, type});
        }
        else if (const auto *expression_statement = dynamic_cast<const parser::expression_statement *>(&value))
        {
          static_cast<void>(expression(*expression_statement->value));
        }
        else if (const auto *assignment = dynamic_cast<const parser::assignment_statement *>(&value))
        {
          const std::string target = assignment->operation == "=" ? assignment_target(*assignment->target)
                                                                  : expression(*assignment->target);
          const std::string assigned = expression(*assignment->value);
          require_compatible(target, assigned, assignment->range, "Assignment");
          mark_initialized(*assignment->target);
        }
        else if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          require_compatible("Bool", expression(*conditional->condition), conditional->condition->range,
                             "If condition");
          const scope_state before = scopes;
          block(*conditional->then_branch);
          const scope_state after_then = scopes;
          scopes = before;
          if (conditional->else_branch) statement(*conditional->else_branch, false);
          const scope_state after_else = scopes;
          merge_initialization(before, {after_then, after_else});
        }
        else if (const auto *loop = dynamic_cast<const parser::condition_loop_statement *>(&value))
        {
          require_compatible("Bool", expression(*loop->condition), loop->condition->range, "Loop condition");
          const scope_state before = scopes;
          block(*loop->body);
          scopes = before;
        }
        else if (const auto *loop = dynamic_cast<const parser::for_statement *>(&value))
        {
          const std::string iterable = expression(*loop->iterable);
          std::string binding_type = array_element(iterable).value_or(std::string(unknown_type));
          if (const auto shaped = dimensioned(iterable)) binding_type = shaped->component;
          const scope_state before = scopes;
          open_scope();
          add_binding(loop->binding, binding{binding_type, {}});
          model.declarations.push_back(typed_declaration{loop->range, loop->binding, binding_type});
          block(*loop->body);
          close_scope();
          scopes = before;
        }
        else if (const auto *returned = dynamic_cast<const parser::return_statement *>(&value))
        {
          const std::string actual = returned->value ? expression(*returned->value) : std::string(void_type);
          if (!return_types.empty() && !is_unknown(return_types.back()))
          {
            require_compatible(return_types.back(), actual, returned->range, "Return");
          }
        }
        else if (const auto *yielded = dynamic_cast<const parser::yield_statement *>(&value))
        {
          if (yielded->value) static_cast<void>(expression(*yielded->value));
        }
        else if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          const std::string subject = expression(*matched->subject);
          const scope_state before = scopes;
          std::vector<scope_state> paths;
          bool has_fallback = false;
          for (const auto &branch : matched->cases)
          {
            scopes = before;
            if (branch.pattern)
            {
              const std::string pattern = expression(*branch.pattern);
              static_cast<void>(common_type(subject, pattern, branch.range, "Match subject and pattern"));
            }
            else has_fallback = true;
            block(*branch.body);
            paths.push_back(scopes);
          }
          if (!has_fallback) paths.push_back(before);
          merge_initialization(before, paths);
        }
        else if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&value))
        {
          const scope_state before = scopes;
          block(*hope->protected_body);
          for (const auto &handler : hope->handlers)
          {
            scopes = before;
            static_cast<void>(expression(*handler.pattern));
            block(*handler.body);
          }
          scopes = before;
          if (hope->cleanup) block(*hope->cleanup);
          else scopes = before;
        }
        else if (const auto *scream = dynamic_cast<const parser::scream_statement *>(&value))
        {
          static_cast<void>(expression(*scream->value));
        }
        else if (const auto *function_declaration = dynamic_cast<const parser::function_declaration *>(&value))
        {
          function(*function_declaration);
        }
        else if (const auto *nested = dynamic_cast<const parser::block_statement *>(&value))
        {
          block(*nested);
        }
        else if (const auto *type = dynamic_cast<const parser::type_declaration *>(&value))
        {
          open_scope();
          if (type->type_kind == parser::type_declaration::kind::class_type)
          {
            add_binding("self", binding{type->name, {}});
          }
          for (const auto &member : type->members) predeclare(*member);
          for (const auto &member : type->members) statement(*member, true);
          close_scope();
        }
      }

    public:
      auto run(const parser::program &tree) -> type_model
      {
        add_binding("print", binding{"Function", callable_signature{{std::string(unknown_type)}, "Void"}});
        for (const auto &entry : tree.statements) predeclare(*entry);
        for (const auto &entry : tree.statements) statement(*entry, true);
        return std::move(model);
      }
    };
  }

  auto type_model::print(std::ostream &stream) const -> void
  {
    stream << "TypeModel\n  Declarations\n";
    for (const auto &entry : declarations)
    {
      stream << "    " << entry.name << ": " << entry.type << " @ "
             << entry.range.begin << ".." << entry.range.end << '\n';
    }
    stream << "  Expressions\n";
    for (const auto &entry : expressions)
    {
      stream << "    " << entry.type << " @ " << entry.range.begin << ".." << entry.range.end << '\n';
    }
  }

  auto check_types(const parser::program &tree) -> type_model
  {
    return type_analysis().run(tree);
  }

  auto validate_entry_point(const parser::program &tree) -> void
  {
    const parser::function_declaration *entry = nullptr;
    for (const auto &statement : tree.statements)
    {
      const auto *function = dynamic_cast<const parser::function_declaration *>(statement.get());
      if (!function || function->name != "main") continue;
      if (entry) throw semantic_error("Program defines more than one 'main' entry point", function->range);
      entry = function;
    }
    if (!entry) throw semantic_error("Executable program requires a 'main' entry point", tree.range);
    if (!entry->parameters.empty())
    {
      throw semantic_error("Entry point 'main' cannot declare parameters", entry->range);
    }
    if (!entry->return_type || (*entry->return_type != "Int" && *entry->return_type != "Void"))
    {
      throw semantic_error("Entry point 'main' must return Int or Void", entry->range);
    }
  }
}
