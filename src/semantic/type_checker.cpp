#include "type_checker.hpp"

#include "semantic_error.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
      std::vector<std::string> type_parameters;
      std::vector<std::optional<std::string>> type_constraints;
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

    struct object_type
    {
      std::vector<std::string> type_parameters;
      std::vector<std::optional<std::string>> type_constraints;
      std::unordered_map<std::string, std::string> fields;
      std::unordered_set<std::string> private_fields;
      std::unordered_set<std::string> weak_fields;
      std::unordered_set<std::string> defaulted_fields;
      std::vector<callable_signature> constructors;
      std::unordered_map<std::string, std::vector<callable_signature>> methods;
      std::unordered_set<std::string> private_methods;
      std::vector<std::string> faces;
    };

    struct enum_case_type
    {
      std::string enum_name;
      std::vector<std::string> type_parameters;
      std::vector<std::string> payload_types;
      std::size_t index = 0;
    };

    struct generic_type
    {
      std::string base;
      std::vector<std::string> arguments;
    };

    auto generic_instance(const std::string_view type) -> generic_type
    {
      const std::size_t open = type.find('<');
      if (open == std::string_view::npos || !type.ends_with('>')) return {std::string(type), {}};
      generic_type result{std::string(type.substr(0, open)), {}};
      const std::string_view contents = type.substr(open + 1, type.size() - open - 2);
      std::size_t begin = 0;
      int depth = 0;
      for (std::size_t index = 0; index <= contents.size(); ++index)
      {
        if (index < contents.size() && contents[index] == '<') ++depth;
        else if (index < contents.size() && contents[index] == '>') --depth;
        if (index == contents.size() || (contents[index] == ',' && depth == 0))
        {
          std::string argument(contents.substr(begin, index - begin));
          while (!argument.empty() && argument.front() == ' ') argument.erase(argument.begin());
          while (!argument.empty() && argument.back() == ' ') argument.pop_back();
          result.arguments.push_back(std::move(argument));
          begin = index + 1;
        }
      }
      return result;
    }

    auto substitute_type(const std::string &type, const std::vector<std::string> &parameters,
                         const std::vector<std::string> &arguments) -> std::string
    {
      for (std::size_t index = 0; index < parameters.size() && index < arguments.size(); ++index)
        if (type == parameters[index]) return arguments[index];
      const auto generic = generic_instance(type);
      if (!generic.arguments.empty())
      {
        std::string result = generic.base + '<';
        for (std::size_t index = 0; index < generic.arguments.size(); ++index)
        {
          if (index != 0) result += ", ";
          result += substitute_type(generic.arguments[index], parameters, arguments);
        }
        return result + '>';
      }
      return type;
    }

    auto infer_type_arguments(const std::string &pattern, const std::string &actual,
                              const std::vector<std::string> &parameters,
                              std::vector<std::string> &arguments) -> bool
    {
      for (std::size_t index = 0; index < parameters.size(); ++index)
      {
        if (pattern != parameters[index]) continue;
        if (arguments[index] == unknown_type || arguments[index].find("Unknown") != std::string::npos)
          arguments[index] = actual;
        return arguments[index] == actual;
      }
      const auto expected = generic_instance(pattern);
      const auto received = generic_instance(actual);
      if (expected.arguments.empty()) return true;
      if (expected.base != received.base || expected.arguments.size() != received.arguments.size()) return false;
      for (std::size_t index = 0; index < expected.arguments.size(); ++index)
        if (!infer_type_arguments(expected.arguments[index], received.arguments[index], parameters, arguments))
          return false;
      return true;
    }

    auto enum_numeric_value(const std::string &spelling, const parser::span range) -> std::int64_t
    {
      const bool negative = spelling.starts_with('-');
      std::string digits = negative ? spelling.substr(1) : spelling;
      digits.erase(std::remove(digits.begin(), digits.end(), '_'), digits.end());
      std::uint64_t magnitude = 0;
      const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude);
      const std::uint64_t negative_limit = std::uint64_t{1} << 63U;
      if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
          (!negative && magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) ||
          (negative && magnitude > negative_limit))
        throw semantic_error("Enum numeric value is outside the supported Int64 range", range);
      if (!negative) return static_cast<std::int64_t>(magnitude);
      if (magnitude == negative_limit) return std::numeric_limits<std::int64_t>::min();
      return -static_cast<std::int64_t>(magnitude);
    }

    struct interface_type
    {
      std::vector<std::string> type_parameters;
      std::vector<std::optional<std::string>> type_constraints;
      std::unordered_map<std::string, std::vector<callable_signature>> methods;
      std::unordered_map<std::string, std::vector<callable_signature>> defaults;
      std::vector<std::string> faces;
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

    auto optional_element(const std::string_view type) -> std::optional<std::string>
    {
      constexpr std::string_view prefix = "Optional<";
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
      if (type.starts_with("SphericalVector")) family = "SphericalVector";
      else if (type.starts_with("SphericalPoint")) family = "SphericalPoint";
      else if (type.starts_with("Vector")) family = "Vector";
      else if (type.starts_with("Point")) family = "Point";
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
      std::unordered_map<const parser::expression *, callable_signature> callables;
      std::unordered_map<std::string, object_type> objects;
      std::unordered_map<std::string, interface_type> interfaces;
      std::unordered_map<std::string, parser::span> interface_ranges;
      std::unordered_map<std::string, std::unordered_set<std::string>> enums;
      std::unordered_map<std::string, enum_case_type> enum_cases;
      std::unordered_map<std::string, std::vector<std::string>> generic_enums;
      std::optional<std::string> expected_expression;
      std::optional<std::string> active_class;

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
        if (const auto expected_value = optional_element(expected))
        {
          if (actual == "None") return true;
          const auto actual_value = optional_element(actual);
          return actual_value && compatible(*expected_value, *actual_value);
        }
        const auto expected_instance = generic_instance(expected);
        if (interfaces.contains(expected_instance.base))
        {
          const auto actual_instance = generic_instance(actual);
          const auto object = objects.find(actual_instance.base);
          if (object != objects.end())
          {
            std::vector<std::string> pending;
            for (const auto &face : object->second.faces)
              pending.push_back(substitute_type(face, object->second.type_parameters, actual_instance.arguments));
            std::unordered_set<std::string> visited;
            while (!pending.empty())
            {
              std::string face = std::move(pending.back());
              pending.pop_back();
              if (face == expected) return true;
              if (!visited.insert(face).second) continue;
              const auto face_instance = generic_instance(face);
              const auto inherited = interfaces.find(face_instance.base);
              if (inherited != interfaces.end())
                for (const auto &parent : inherited->second.faces)
                  pending.push_back(substitute_type(parent, inherited->second.type_parameters,
                                                    face_instance.arguments));
            }
          }
        }
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
        if ((expected == "Vector" || expected == "Point" || expected == "SphericalVector" ||
             expected == "SphericalPoint") && actual_dimensioned)
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

      auto arithmetic_type(const std::string &operation, const std::string &left, const std::string &right,
                           const parser::span range) const -> std::string
      {
        if (is_unknown(left) || is_unknown(right)) return std::string(unknown_type);
        const auto left_dimensioned = dimensioned(left);
        const auto right_dimensioned = dimensioned(right);
        if (!left_dimensioned && !right_dimensioned)
        {
          require(is_numeric(left) && is_numeric(right),
                  "Operator '" + operation + "' requires numeric operands, but received " + left + " and " + right,
                  range);
          return common_type(left, right, range, "Operator operands");
        }

        if (operation == "+" || operation == "-")
        {
          require(left_dimensioned && right_dimensioned &&
                      left_dimensioned->dimensions == right_dimensioned->dimensions,
                  "Operator '" + operation + "' requires dimensioned operands of equal size, but received " +
                      left + " and " + right, range);
          const std::string component = common_type(left_dimensioned->component, right_dimensioned->component,
                                                    range, "Dimensioned components");
          if (left_dimensioned->family == "Vector" && right_dimensioned->family == "Vector")
            return "Vector" + std::to_string(left_dimensioned->dimensions) + "<" + component + ">";
          if (left_dimensioned->family == "Point" && right_dimensioned->family == "Vector")
            return "Point" + std::to_string(left_dimensioned->dimensions) + "<" + component + ">";
          if (operation == "-" && left_dimensioned->family == "Point" &&
              right_dimensioned->family == "Point")
            return "Vector" + std::to_string(left_dimensioned->dimensions) + "<" + component + ">";
          require(false, "Operator '" + operation + "' is not defined for " + left + " and " + right, range);
          return std::string(unknown_type);
        }

        if (operation == "*" && left_dimensioned && left_dimensioned->family == "Vector" && is_numeric(right))
          return "Vector" + std::to_string(left_dimensioned->dimensions) + "<" +
                 common_type(left_dimensioned->component, right, range, "Vector scalar operands") + ">";
        if (operation == "*" && right_dimensioned && right_dimensioned->family == "Vector" && is_numeric(left))
          return "Vector" + std::to_string(right_dimensioned->dimensions) + "<" +
                 common_type(left, right_dimensioned->component, range, "Vector scalar operands") + ">";
        if (operation == "/" && left_dimensioned && left_dimensioned->family == "Vector" && is_numeric(right))
          return "Vector" + std::to_string(left_dimensioned->dimensions) + "<" +
                 common_type(left_dimensioned->component, right, range, "Vector scalar operands") + ">";

        require(false, "Operator '" + operation + "' is not defined for " + left + " and " + right, range);
        return std::string(unknown_type);
      }

      auto require_compatible(const std::string &expected, const std::string &actual,
                              const parser::span range, const std::string_view context) const -> void
      {
        require(compatible(expected, actual),
                std::string(context) + " requires " + expected + ", but received " + actual, range);
      }

      auto require_constraints(const std::vector<std::string> &parameters,
                               const std::vector<std::optional<std::string>> &constraints,
                               const std::vector<std::string> &arguments,
                               const parser::span range) const -> void
      {
        for (std::size_t index = 0; index < constraints.size(); ++index)
        {
          if (!constraints[index]) continue;
          const std::string constraint = substitute_type(*constraints[index], parameters, arguments);
          require(interfaces.contains(generic_instance(constraint).base),
                  "Generic constraint '" + constraint + "' is not a face", range);
          require(compatible(constraint, arguments[index]),
                  "Type argument " + arguments[index] + " does not satisfy face constraint " + constraint,
                  range);
        }
      }

      auto annotation(const std::optional<std::string> &name) const -> std::string
      {
        if (!name) return std::string(unknown_type);
        if (const auto contained = optional_element(*name))
        {
          std::string inner = *contained;
          if (inner == "Int") inner = "Int64";
          else if (inner == "Float") inner = "Float64";
          return "Optional<" + inner + ">";
        }
        const auto generic = generic_instance(*name);
        if (!generic.arguments.empty()) return fixed_annotation(name);
        return *name;
      }

      auto fixed_annotation(const std::optional<std::string> &name) const -> std::string
      {
        if (!name) return std::string(unknown_type);
        if (*name == "Int") return "Int64";
        if (*name == "Float") return "Float64";
        if (const auto contained = optional_element(*name))
          return "Optional<" + fixed_annotation(std::optional<std::string>{*contained}) + ">";
        const auto generic = generic_instance(*name);
        if (!generic.arguments.empty())
        {
          std::string result = generic.base + '<';
          for (std::size_t index = 0; index < generic.arguments.size(); ++index)
          {
            if (index != 0) result += ", ";
            result += fixed_annotation(std::optional<std::string>{generic.arguments[index]});
          }
          return result + '>';
        }
        return *name;
      }

      auto signature(const parser::function_declaration &function) const -> callable_signature
      {
        callable_signature result;
        for (const auto &parameter : function.parameters) result.parameters.push_back(fixed_annotation(parameter.type_name));
        result.result = fixed_annotation(function.return_type);
        result.type_parameters = function.type_parameters;
        result.type_constraints = function.type_constraints;
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
          const std::size_t separator = imported->imported_name.rfind('.');
          const std::string fallback = separator == std::string::npos
                                           ? imported->imported_name
                                           : imported->imported_name.substr(separator + 1);
          add_binding(imported->alias.value_or(fallback), binding{std::string(unknown_type), {}});
        }
      }

      auto collect_object_type(const parser::type_declaration &type) -> void
      {
        if (type.type_kind != parser::type_declaration::kind::class_type) return;
        object_type object;
        object.type_parameters = type.type_parameters;
        object.type_constraints = type.type_constraints;
        object.faces = type.composed_interfaces;
        for (const auto &member : type.members)
        {
          if (const auto *field = dynamic_cast<const parser::let_declaration *>(member.get()))
          {
            object.fields.emplace(field->name, fixed_annotation(field->type_name));
            if (field->private_member) object.private_fields.insert(field->name);
            if (field->weak_member) object.weak_fields.insert(field->name);
            if (field->initializer || field->weak_member) object.defaulted_fields.insert(field->name);
          }
          else if (const auto *method = dynamic_cast<const parser::function_declaration *>(member.get()))
          {
            if (method->constructor_member)
            {
              object.constructors.push_back(signature(*method));
              continue;
            }
            object.methods[method->name].push_back(signature(*method));
            if (method->private_member) object.private_methods.insert(method->name);
          }
        }
        objects.emplace(type.name, std::move(object));
      }

      struct ownership_edge
      {
        std::string owner;
        std::string field;
        std::string target;
        parser::span range;
      };

      auto reference_targets(const std::string &type, std::vector<std::string> &targets) const -> void
      {
        const auto instance = generic_instance(type);
        if (objects.contains(instance.base) || interfaces.contains(instance.base))
          targets.push_back(instance.base);
        for (const auto &argument : instance.arguments) reference_targets(argument, targets);
      }

      auto validate_ownership(const parser::program &tree) const -> void
      {
        std::unordered_map<std::string, std::vector<ownership_edge>> graph;
        std::vector<std::string> class_order;
        for (const auto &entry : tree.statements)
        {
          const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
          if (!type || type->type_kind != parser::type_declaration::kind::class_type) continue;
          class_order.push_back(type->name);
          const auto &object = objects.at(type->name);
          for (const auto &member : type->members)
          {
            const auto *field = dynamic_cast<const parser::let_declaration *>(member.get());
            if (!field || field->weak_member) continue;
            std::vector<std::string> targets;
            reference_targets(object.fields.at(field->name), targets);
            for (const auto &target : targets)
            {
              require(!interfaces.contains(target),
                      "Strong field '" + type->name + "." + field->name +
                          "' cannot use dynamic face type '" + target + "'; declare it with weak let",
                      field->range);
              if (objects.contains(target))
                graph[type->name].push_back(ownership_edge{type->name, field->name, target, field->range});
            }
          }
        }

        std::unordered_map<std::string, int> state;
        std::vector<std::string> class_stack;
        std::vector<ownership_edge> edge_stack;
        const auto visit = [&](const auto &self, const std::string &name) -> void
        {
          state[name] = 1;
          class_stack.push_back(name);
          if (const auto found = graph.find(name); found != graph.end())
            for (const auto &edge : found->second)
            {
              edge_stack.push_back(edge);
              if (state[edge.target] == 0) self(self, edge.target);
              else if (state[edge.target] == 1)
              {
                const auto begin = std::find(class_stack.begin(), class_stack.end(), edge.target);
                const std::size_t first = static_cast<std::size_t>(std::distance(class_stack.begin(), begin));
                std::string path;
                for (std::size_t index = first; index < edge_stack.size(); ++index)
                {
                  if (!path.empty()) path += " -> ";
                  path += edge_stack[index].owner + "." + edge_stack[index].field;
                }
                path += " -> " + edge.target;
                throw semantic_error("Strong ownership cycle requires an explicit weak field edge: " + path,
                                     edge.range);
              }
              edge_stack.pop_back();
            }
          class_stack.pop_back();
          state[name] = 2;
        };
        for (const auto &name : class_order)
          if (state[name] == 0) visit(visit, name);
      }

      auto collect_interface_type(const parser::type_declaration &type) -> void
      {
        if (type.type_kind != parser::type_declaration::kind::interface_type) return;
        interface_type interface;
        interface.type_parameters = type.type_parameters;
        interface.type_constraints = type.type_constraints;
        interface.faces = type.composed_interfaces;
        for (const auto &member : type.members)
        {
          const auto *method = dynamic_cast<const parser::function_declaration *>(member.get());
          if (!method) continue;
          interface.methods[method->name].push_back(signature(*method));
          if (method->body || method->expression_body)
            interface.defaults[method->name].push_back(signature(*method));
        }
        interfaces.emplace(type.name, std::move(interface));
        interface_ranges.emplace(type.name, type.range);
      }

      auto add_signature(std::vector<callable_signature> &signatures,
                         const callable_signature &candidate) const -> void
      {
        if (std::none_of(signatures.begin(), signatures.end(), [&](const callable_signature &existing)
            {
              return same_signature(existing, candidate);
            }))
          signatures.push_back(candidate);
      }

      auto resolve_interface(const std::string &name,
                             std::unordered_map<std::string, int> &states) -> void
      {
        if (states[name] == 2) return;
        require(states[name] != 1, "Cyclic face composition involving '" + name + "'",
                interface_ranges.at(name));
        states[name] = 1;
        const interface_type direct = interfaces.at(name);
        interface_type resolved;
        resolved.type_parameters = direct.type_parameters;
        resolved.type_constraints = direct.type_constraints;
        resolved.faces = direct.faces;
        for (const auto &parent_name : direct.faces)
        {
          const auto parent_instance = generic_instance(parent_name);
          require(interfaces.contains(parent_instance.base),
                  "Face '" + name + "' composes unknown face '" + parent_name + "'",
                  interface_ranges.at(name));
          resolve_interface(parent_instance.base, states);
          const auto &parent = interfaces.at(parent_instance.base);
          require(parent_instance.arguments.size() == parent.type_parameters.size(),
                  "Face '" + parent_instance.base + "' expects " +
                      std::to_string(parent.type_parameters.size()) + " type argument(s)",
                  interface_ranges.at(name));
          for (const auto &[method_name, signatures] : parent.methods)
            for (auto candidate : signatures)
            {
              for (auto &parameter : candidate.parameters)
                parameter = substitute_type(parameter, parent.type_parameters, parent_instance.arguments);
              candidate.result = substitute_type(candidate.result, parent.type_parameters, parent_instance.arguments);
              add_signature(resolved.methods[method_name], candidate);
            }
          for (const auto &[method_name, defaults] : parent.defaults)
            for (auto candidate : defaults)
            {
              for (auto &parameter : candidate.parameters)
                parameter = substitute_type(parameter, parent.type_parameters, parent_instance.arguments);
              candidate.result = substitute_type(candidate.result, parent.type_parameters, parent_instance.arguments);
              resolved.defaults[method_name].push_back(std::move(candidate));
            }
        }
        for (const auto &[method_name, signatures] : direct.methods)
        {
          for (const auto &candidate : signatures)
          {
            add_signature(resolved.methods[method_name], candidate);
            auto &defaults = resolved.defaults[method_name];
            std::erase_if(defaults, [&](const callable_signature &existing)
            {
              return same_signature(existing, candidate);
            });
            const auto own_defaults = direct.defaults.find(method_name);
            if (own_defaults != direct.defaults.end())
              for (const auto &own_default : own_defaults->second)
                if (same_signature(own_default, candidate)) defaults.push_back(own_default);
          }
        }
        interfaces[name] = std::move(resolved);
        states[name] = 2;
      }

      auto collect_enum_type(const parser::type_declaration &type) -> void
      {
        if (type.type_kind != parser::type_declaration::kind::enum_type) return;
        generic_enums[type.name] = type.type_parameters;
        auto &members = enums[type.name];
        std::unordered_map<std::int64_t, std::string> numeric_values;
        std::int64_t previous_value = 0;
        bool has_previous_value = false;
        for (std::size_t index = 0; index < type.enum_members.size(); ++index)
        {
          const auto &member = type.enum_members[index];
          std::int64_t numeric_value = 0;
          if (member.numeric_value) numeric_value = enum_numeric_value(*member.numeric_value, member.range);
          else if (!has_previous_value) numeric_value = 0;
          else
          {
            require(previous_value != std::numeric_limits<std::int64_t>::max(),
                    "Implicit enum numeric value after '" + type.enum_members[index - 1].name +
                        "' would overflow Int64",
                    member.range);
            numeric_value = previous_value + 1;
          }
          if (const auto duplicate = numeric_values.find(numeric_value); duplicate != numeric_values.end())
            throw semantic_error("Enum cases '" + duplicate->second + "' and '" + member.name +
                                     "' cannot share numeric value " + std::to_string(numeric_value),
                                 member.range);
          numeric_values.emplace(numeric_value, member.name);
          previous_value = numeric_value;
          has_previous_value = true;
          members.insert(member.name);
          if (!member.payload_types.empty())
          {
            std::vector<std::string> payload_types;
            for (const auto &payload : member.payload_types)
              payload_types.push_back(fixed_annotation(std::optional<std::string>{payload}));
            require(!enum_cases.contains(member.name),
                    "Payload enum constructor '" + member.name + "' is already declared", member.range);
            enum_cases.emplace(member.name, enum_case_type{type.name, type.type_parameters, payload_types, index});
            if (type.type_parameters.empty())
              add_binding(member.name, binding{"Function", callable_signature{payload_types, type.name, {}, {}}});
            else add_binding(member.name, binding{"GenericFunction", {}});
          }
        }
      }

      auto same_signature(const callable_signature &left, const callable_signature &right) const -> bool
      {
        return left.parameters == right.parameters && left.result == right.result &&
               left.type_parameters == right.type_parameters && left.type_constraints == right.type_constraints;
      }

      auto validate_composition(const std::string &class_name, object_type &object,
                                const parser::span range) -> void
      {
        std::unordered_map<std::string, std::vector<callable_signature>> inherited_defaults;
        for (const auto &face_name : object.faces)
        {
          const auto face_instance = generic_instance(face_name);
          const auto face = interfaces.find(face_instance.base);
          require(face != interfaces.end(),
                  "Class '" + class_name + "' composes unknown face '" + face_name + "'", range);
          require(face_instance.arguments.size() == face->second.type_parameters.size(),
                  "Face '" + face_instance.base + "' expects " +
                      std::to_string(face->second.type_parameters.size()) + " type argument(s)", range);
          for (const auto &[method_name, required_signatures] : face->second.methods)
          {
            const auto provided = object.methods.find(method_name);
            for (auto required : required_signatures)
            {
              for (auto &parameter : required.parameters)
                parameter = substitute_type(parameter, face->second.type_parameters, face_instance.arguments);
              required.result = substitute_type(required.result, face->second.type_parameters,
                                                face_instance.arguments);
              const bool provided_match = provided != object.methods.end() &&
                  std::any_of(provided->second.begin(), provided->second.end(),
                              [&](const callable_signature &candidate)
              {
                return same_signature(required, candidate);
              });
              const auto defaults = face->second.defaults.find(method_name);
              const std::size_t default_count = defaults == face->second.defaults.end() ? 0U :
                  static_cast<std::size_t>(std::count_if(defaults->second.begin(), defaults->second.end(),
                                                         [&](const callable_signature &candidate)
              {
                return same_signature(required, candidate);
              }));
              const bool has_default = default_count != 0;
              if (provided_match)
              {
                require(!object.private_methods.contains(method_name),
                        "Class '" + class_name + "' cannot satisfy face '" + face_name +
                            "' with private method '" + method_name + "'",
                        range);
              }
              require(provided_match || has_default,
                      provided == object.methods.end()
                          ? "Class '" + class_name + "' does not implement required method '" + method_name +
                                "' from face '" + face_name + "'"
                          : "Class '" + class_name + "' has an incompatible signature for method '" +
                                method_name + "' required by face '" + face_name + "'",
                      range);
              if (!provided_match)
                for (std::size_t index = 0; index < default_count; ++index)
                  inherited_defaults[method_name].push_back(required);
            }
          }
        }
        for (const auto &[method_name, defaults] : inherited_defaults)
        {
          for (std::size_t index = 0; index < defaults.size(); ++index)
          {
            const std::size_t matches = static_cast<std::size_t>(std::count_if(
                defaults.begin(), defaults.end(), [&](const callable_signature &candidate)
            {
              return same_signature(defaults[index], candidate);
            }));
            require(matches == 1,
                    "Class '" + class_name + "' inherits conflicting defaults for method '" + method_name +
                        "'; provide an explicit override",
                    range);
            auto &methods = object.methods[method_name];
            if (std::none_of(methods.begin(), methods.end(), [&](const callable_signature &candidate)
                {
                  return same_signature(defaults[index], candidate);
                }))
              methods.push_back(defaults[index]);
          }
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
          if (identifier->name == "None") return record(value, "None");
          const auto *matches = find(identifier->name);
          require(matches && !matches->empty(), "Undefined name '" + identifier->name + "'", value.range);
          require(matches->front().initialized, "Variable '" + identifier->name + "' is used before initialization",
                  value.range);
          if (matches->front().callable) callables[&value] = *matches->front().callable;
          return record(value, matches->front().type);
        }
        if (const auto *grouping = dynamic_cast<const parser::grouping_expression *>(&value))
        {
          const std::string type = expression(*grouping->value);
          if (const auto found = callables.find(grouping->value.get()); found != callables.end())
            callables[&value] = found->second;
          return record(value, type);
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
          if (unary->operator_text == "++" || unary->operator_text == "--")
          {
            static_cast<void>(assignment_target(*unary->operand));
            require(is_unknown(operand) || is_numeric(operand),
                    "Operator '" + unary->operator_text + "' requires a numeric operand, but received " + operand,
                    value.range);
            return record(value, operand);
          }
          if (unary->operator_text == "!")
          {
            require_compatible("Bool", operand, value.range, "Logical negation");
            return record(value, "Bool");
          }
          const auto shaped = dimensioned(operand);
          require(is_unknown(operand) || is_numeric(operand) ||
                      (shaped && shaped->family == "Vector" &&
                       (unary->operator_text == "+" || unary->operator_text == "-")),
                  "Operator '" + unary->operator_text + "' requires a numeric operand, but received " + operand,
                  value.range);
          return record(value, operand);
        }
        if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
        {
          const std::string left = expression(*binary->left);
          const std::string right = expression(*binary->right);
          if (binary->operator_text == "??")
          {
            const auto contained = optional_element(left);
            require(contained.has_value(), "Left operand of ?? must be Optional, but received " + left,
                    binary->left->range);
            if (const auto fallback = optional_element(right))
            {
              static_cast<void>(common_type(*contained, *fallback, value.range, "Optional fallback values"));
              return record(value, left);
            }
            require_compatible(*contained, right, binary->right->range, "Optional fallback");
            return record(value, *contained);
          }
          if (binary->operator_text == "and" || binary->operator_text == "or")
          {
            require_compatible("Bool", left, binary->left->range, "Logical operator");
            require_compatible("Bool", right, binary->right->range, "Logical operator");
            return record(value, "Bool");
          }
          if (binary->operator_text == "==" || binary->operator_text == "!=")
          {
            static_cast<void>(common_type(left, right, value.range, "Comparison operands"));
            return record(value, "Bool");
          }
          if (binary->operator_text == "<" || binary->operator_text == "<=" ||
              binary->operator_text == ">" || binary->operator_text == ">=")
          {
            require(!enums.contains(left) && !enums.contains(right),
                    "Ordered comparison is not defined for enum values", value.range);
            require(!dimensioned(left) && !dimensioned(right),
                    "Ordered comparison is not defined for " + left + " and " + right, value.range);
            static_cast<void>(common_type(left, right, value.range, "Comparison operands"));
            return record(value, "Bool");
          }
          return record(value, arithmetic_type(binary->operator_text, left, right, value.range));
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
          if (const auto *member = dynamic_cast<const parser::member_expression *>(call->callee.get()))
          {
            if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get()))
            {
              const auto qualified = generic_instance(target->name);
              if (const auto enum_case = enum_cases.find(member->member_name);
                  enum_case != enum_cases.end() && enum_case->second.enum_name == qualified.base)
              {
                const auto &case_type = enum_case->second;
                require(!member->safe, "Safe member access is not defined for enum cases", value.range);
                require(qualified.arguments.size() == case_type.type_parameters.size(),
                        "Generic enum '" + qualified.base + "' expects " +
                            std::to_string(case_type.type_parameters.size()) + " type argument(s), but received " +
                            std::to_string(qualified.arguments.size()), target->range);
                std::vector<std::string> type_arguments;
                for (const auto &argument : qualified.arguments)
                  type_arguments.push_back(fixed_annotation(std::optional<std::string>{argument}));
                require(arguments.size() == case_type.payload_types.size(),
                        "Enum case '" + member->member_name + "' expects " +
                            std::to_string(case_type.payload_types.size()) + " payload value(s), but received " +
                            std::to_string(arguments.size()), value.range);
                for (std::size_t index = 0; index < arguments.size(); ++index)
                  require_compatible(substitute_type(case_type.payload_types[index], case_type.type_parameters,
                                                     type_arguments),
                                     arguments[index], call->arguments[index]->range, "Enum case payload");
                static_cast<void>(record(*member->target, "Type"));
                static_cast<void>(record(*call->callee, "Function"));
                std::string result = qualified.base + '<';
                for (std::size_t index = 0; index < type_arguments.size(); ++index)
                {
                  if (index != 0) result += ", ";
                  result += type_arguments[index];
                }
                return record(value, result + '>');
              }
            }
          }
          if (const auto *identifier = dynamic_cast<const parser::identifier_expression *>(call->callee.get()))
          {
            if (identifier->name == "Some")
            {
              require(arguments.size() == 1, "Some expects exactly one value", value.range);
              static_cast<void>(record(*call->callee, "Function"));
              return record(value, "Optional<" + arguments.front() + ">");
            }
            if (const auto enum_case = enum_cases.find(identifier->name); enum_case != enum_cases.end())
            {
              const auto &case_type = enum_case->second;
              std::vector<std::string> type_arguments(case_type.type_parameters.size(), std::string(unknown_type));
              if (expected_expression)
              {
                const auto contextual = generic_instance(*expected_expression);
                if (contextual.base == case_type.enum_name && contextual.arguments.size() == type_arguments.size())
                  type_arguments = contextual.arguments;
              }
              require(arguments.size() == case_type.payload_types.size(),
                      "Enum case '" + identifier->name + "' expects " +
                          std::to_string(case_type.payload_types.size()) +
                          " payload value(s), but received " + std::to_string(arguments.size()),
                      value.range);
              for (std::size_t index = 0; index < arguments.size(); ++index)
              {
                for (std::size_t parameter = 0; parameter < case_type.type_parameters.size(); ++parameter)
                  if (case_type.payload_types[index] == case_type.type_parameters[parameter] &&
                      is_unknown(type_arguments[parameter]))
                    type_arguments[parameter] = arguments[index];
                require_compatible(substitute_type(case_type.payload_types[index], case_type.type_parameters,
                                                   type_arguments),
                                   arguments[index], call->arguments[index]->range, "Enum case payload");
              }
              require(std::none_of(type_arguments.begin(), type_arguments.end(), [](const auto &type)
                      { return is_unknown(type); }),
                      "Cannot infer every generic argument for enum case '" + identifier->name +
                          "'; provide an expected " + case_type.enum_name + " type",
                      value.range);
              static_cast<void>(record(*call->callee, "Function"));
              std::string result = case_type.enum_name;
              if (!type_arguments.empty())
              {
                result += '<';
                for (std::size_t index = 0; index < type_arguments.size(); ++index)
                {
                  if (index != 0) result += ", ";
                  result += type_arguments[index];
                }
                result += '>';
              }
              return record(value, result);
            }
            const auto constructed = generic_instance(identifier->name);
            if (objects.contains(constructed.base))
            {
              const auto &object = objects.at(constructed.base);
              std::vector<std::string> type_arguments(object.type_parameters.size(), std::string(unknown_type));
              if (!constructed.arguments.empty())
              {
                require(constructed.arguments.size() == object.type_parameters.size(),
                        "Generic class '" + constructed.base + "' expects " +
                            std::to_string(object.type_parameters.size()) + " type argument(s), but received " +
                            std::to_string(constructed.arguments.size()), identifier->range);
                type_arguments.clear();
                for (const auto &argument : constructed.arguments)
                  type_arguments.push_back(fixed_annotation(std::optional<std::string>{argument}));
              }
              else if (expected_expression)
              {
                const auto contextual = generic_instance(*expected_expression);
                if (contextual.base == constructed.base && contextual.arguments.size() == type_arguments.size())
                  type_arguments = contextual.arguments;
              }
              if (object.constructors.empty())
              {
                require(arguments.empty(), "Default construction of '" + constructed.base +
                                               "' does not accept arguments", value.range);
                require(object.defaulted_fields.size() == object.fields.size(),
                        "Class '" + constructed.base +
                            "' requires a constructor because not every field has a default",
                        value.range);
              }
              else
              {
                std::vector<std::pair<callable_signature, std::vector<std::string>>> viable;
                for (const auto &candidate : object.constructors)
                {
                  if (candidate.parameters.size() != arguments.size()) continue;
                  auto inferred = type_arguments;
                  bool matches = true;
                  if (std::any_of(inferred.begin(), inferred.end(), [](const auto &type)
                      { return is_unknown(type); }))
                    for (std::size_t index = 0; index < arguments.size(); ++index)
                      matches &= infer_type_arguments(candidate.parameters[index], arguments[index],
                                                      object.type_parameters, inferred);
                  matches &= std::none_of(inferred.begin(), inferred.end(), [](const auto &type)
                  {
                    return is_unknown(type);
                  });
                  callable_signature instantiated = candidate;
                  for (auto &parameter : instantiated.parameters)
                    parameter = substitute_type(parameter, object.type_parameters, inferred);
                  for (std::size_t index = 0; index < arguments.size(); ++index)
                    matches &= compatible(instantiated.parameters[index], arguments[index]);
                  if (matches) viable.emplace_back(std::move(instantiated), std::move(inferred));
                }
                require(!viable.empty(), "No matching constructor for '" + constructed.base + "'", value.range);
                require(viable.size() == 1, "Ambiguous constructor for '" + constructed.base + "'", value.range);
                type_arguments = std::move(viable.front().second);
              }
              require(std::none_of(type_arguments.begin(), type_arguments.end(), [](const auto &type)
                      { return is_unknown(type); }),
                      "Cannot infer every generic argument for class '" + constructed.base +
                          "'; provide an expected type",
                      value.range);
              require_constraints(object.type_parameters, object.type_constraints, type_arguments, value.range);
              static_cast<void>(record(*call->callee, "Type"));
              std::string result = constructed.base;
              if (!type_arguments.empty())
              {
                result += '<';
                for (std::size_t index = 0; index < type_arguments.size(); ++index)
                {
                  if (index != 0) result += ", ";
                  result += type_arguments[index];
                }
                result += '>';
              }
              return record(value, result);
            }
            const auto requested = generic_instance(identifier->name);
            const auto *matches = find(requested.base);
            require(matches, "Undefined name '" + requested.base + "'", identifier->range);
            std::vector<callable_signature> viable;
            for (const auto &candidate : *matches)
            {
              if (!candidate.callable || candidate.callable->parameters.size() != arguments.size()) continue;
              callable_signature instantiated = *candidate.callable;
              if (!instantiated.type_parameters.empty())
              {
                std::vector<std::string> inferred;
                bool inferred_all = true;
                if (!requested.arguments.empty())
                {
                  if (requested.arguments.size() != instantiated.type_parameters.size()) continue;
                  for (const auto &argument : requested.arguments)
                    inferred.push_back(fixed_annotation(std::optional<std::string>{argument}));
                }
                else
                {
                  inferred.assign(instantiated.type_parameters.size(), std::string(unknown_type));
                  for (std::size_t index = 0; index < arguments.size(); ++index)
                    inferred_all &= infer_type_arguments(instantiated.parameters[index], arguments[index],
                                                         instantiated.type_parameters, inferred);
                }
                inferred_all &= std::none_of(inferred.begin(), inferred.end(), [](const auto &type)
                {
                  return is_unknown(type);
                });
                if (!inferred_all) continue;
                require_constraints(instantiated.type_parameters, instantiated.type_constraints, inferred,
                                    value.range);
                for (auto &parameter : instantiated.parameters)
                  parameter = substitute_type(parameter, instantiated.type_parameters, inferred);
                instantiated.result = substitute_type(instantiated.result, instantiated.type_parameters, inferred);
                instantiated.type_parameters.clear();
              }
              else if (!requested.arguments.empty()) continue;
              bool matches_arguments = true;
              for (std::size_t index = 0; index < arguments.size(); ++index)
              {
                matches_arguments &= compatible(instantiated.parameters[index], arguments[index]);
              }
              if (matches_arguments) viable.push_back(std::move(instantiated));
            }
            require(!viable.empty(), "No matching overload for '" + requested.base + "'", value.range);
            require(viable.size() == 1, "Ambiguous overload for '" + requested.base + "'", value.range);
            static_cast<void>(record(*call->callee, "Function"));
            return record(value, viable.front().result);
          }
          static_cast<void>(expression(*call->callee));
          const auto callable = callables.find(call->callee.get());
          require(callable != callables.end(), "Called expression is not callable", call->callee->range);
          callable_signature instantiated = callable->second;
          require(instantiated.parameters.size() == arguments.size(),
                  "Callable expects " + std::to_string(instantiated.parameters.size()) +
                      " arguments, but received " + std::to_string(arguments.size()),
                  value.range);
          if (!instantiated.type_parameters.empty())
          {
            std::vector<std::string> inferred;
            bool inferred_all = true;
            const auto *member = dynamic_cast<const parser::member_expression *>(call->callee.get());
            const auto requested = member ? generic_instance(member->member_name) : generic_type{};
            if (!requested.arguments.empty())
            {
              require(requested.arguments.size() == instantiated.type_parameters.size(),
                      "Generic method '" + requested.base + "' expects " +
                          std::to_string(instantiated.type_parameters.size()) + " type argument(s)", value.range);
              for (const auto &argument : requested.arguments)
                inferred.push_back(fixed_annotation(std::optional<std::string>{argument}));
            }
            else
            {
              inferred.assign(instantiated.type_parameters.size(), std::string(unknown_type));
              for (std::size_t index = 0; index < arguments.size(); ++index)
                inferred_all &= infer_type_arguments(instantiated.parameters[index], arguments[index],
                                                     instantiated.type_parameters, inferred);
            }
            inferred_all &= std::none_of(inferred.begin(), inferred.end(), [](const auto &type)
            {
              return is_unknown(type);
            });
            require(inferred_all, "Cannot infer every generic argument for called method", value.range);
            require_constraints(instantiated.type_parameters, instantiated.type_constraints, inferred, value.range);
            for (auto &parameter : instantiated.parameters)
              parameter = substitute_type(parameter, instantiated.type_parameters, inferred);
            instantiated.result = substitute_type(instantiated.result, instantiated.type_parameters, inferred);
          }
          for (std::size_t index = 0; index < arguments.size(); ++index)
            require_compatible(instantiated.parameters[index], arguments[index], call->arguments[index]->range,
                               "Callable argument");
          return record(value, instantiated.result);
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
          if (const auto *type_name = dynamic_cast<const parser::identifier_expression *>(member->target.get()))
          {
            const auto qualified = generic_instance(type_name->name);
            if (const auto enumeration = enums.find(qualified.base); enumeration != enums.end())
            {
              static_cast<void>(record(*member->target, "Type"));
              require(!member->safe, "Safe member access is not defined for enum type '" + qualified.base + "'",
                      value.range);
              require(enumeration->second.contains(member->member_name),
                      "Enum '" + qualified.base + "' has no member '" + member->member_name + "'", value.range);
              return record(value, fixed_annotation(std::optional<std::string>{type_name->name}));
            }
          }
          const std::string target = expression(*member->target);
          const auto optional_target = optional_element(target);
          const std::string accessed_target = optional_target.value_or(target);
          if (const auto shaped = dimensioned(target))
          {
            require(!member->safe, "Safe member access is not defined for " + shaped->family + " values",
                    value.range);
            const bool spherical = shaped->family == "SphericalPoint" || shaped->family == "SphericalVector";
            const std::array<std::string_view, 3> spherical_names =
                shaped->family == "SphericalPoint"
                    ? std::array<std::string_view, 3>{"radius", "inclination", "azimuth"}
                    : std::array<std::string_view, 3>{"magnitude", "inclination", "azimuth"};
            const std::string_view component_names = "xyzw";
            const auto spherical_component = std::find(spherical_names.begin(), spherical_names.end(),
                                                       member->member_name);
            const std::size_t component = spherical
                                              ? static_cast<std::size_t>(spherical_component - spherical_names.begin())
                                              : component_names.find(member->member_name);
            require((spherical && spherical_component != spherical_names.end()) ||
                        (!spherical && member->member_name.size() == 1 && component != std::string_view::npos),
                    shaped->family + " has no member '" + member->member_name + "'", value.range);
            require(component < shaped->dimensions,
                    shaped->family + std::to_string(shaped->dimensions) + " has no member '" +
                        member->member_name + "'",
                    value.range);
            return record(value, shaped->component);
          }
          require(!member->safe || optional_target.has_value(),
                  "Safe member access requires Optional, but received " + target, value.range);
          const auto instantiated_target = generic_instance(accessed_target);
          if (const auto object = objects.find(instantiated_target.base); object != objects.end())
          {
            if (const auto field = object->second.fields.find(member->member_name);
                field != object->second.fields.end())
            {
              require(!object->second.private_fields.contains(member->member_name) ||
                          (active_class && *active_class == instantiated_target.base),
                      "Private field '" + member->member_name + "' of class '" + accessed_target +
                          "' is not accessible here",
                      value.range);
              const std::string field_type = substitute_type(field->second, object->second.type_parameters,
                                                             instantiated_target.arguments);
              const bool optional_result = member->safe || object->second.weak_fields.contains(member->member_name);
              return record(value, optional_result ? "Optional<" + field_type + ">" : field_type);
            }
            const auto requested_member = generic_instance(member->member_name);
            if (const auto methods = object->second.methods.find(requested_member.base);
                methods != object->second.methods.end())
            {
              require(!object->second.private_methods.contains(member->member_name) ||
                          (active_class && *active_class == instantiated_target.base),
                      "Private method '" + member->member_name + "' of class '" + accessed_target +
                          "' is not accessible here",
                      value.range);
              require(methods->second.size() == 1,
                      "Method reference '" + member->member_name + "' is overloaded and requires a call",
                      value.range);
              auto signature = methods->second.front();
              for (auto &parameter : signature.parameters)
                parameter = substitute_type(parameter, object->second.type_parameters, instantiated_target.arguments);
              signature.result = substitute_type(signature.result, object->second.type_parameters,
                                                 instantiated_target.arguments);
              if (member->safe) signature.result = "Optional<" + signature.result + ">";
              callables[&value] = std::move(signature);
              return record(value, "Function");
            }
            throw semantic_error("Type '" + accessed_target + "' has no member '" + member->member_name + "'", value.range);
          }
          const auto instantiated_interface = generic_instance(accessed_target);
          if (const auto interface = interfaces.find(instantiated_interface.base); interface != interfaces.end())
          {
            const auto requested_member = generic_instance(member->member_name);
            const auto methods = interface->second.methods.find(requested_member.base);
            require(methods != interface->second.methods.end(),
                    "Face '" + target + "' has no method '" + member->member_name + "'", value.range);
            require(methods->second.size() == 1,
                    "Face method reference '" + member->member_name + "' is overloaded and requires a call",
                    value.range);
            auto signature = methods->second.front();
            for (auto &parameter : signature.parameters)
              parameter = substitute_type(parameter, interface->second.type_parameters,
                                          instantiated_interface.arguments);
            signature.result = substitute_type(signature.result, interface->second.type_parameters,
                                               instantiated_interface.arguments);
            if (member->safe) signature.result = "Optional<" + signature.result + ">";
            callables[&value] = std::move(signature);
            return record(value, "Function");
          }
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
                                     : collection->collection_kind == parser::collection_expression::kind::point
                                         ? "Point"
                                     : collection->collection_kind == parser::collection_expression::kind::spherical_vector
                                         ? "SphericalVector"
                                         : "SphericalPoint";
          std::optional<std::string> component_type;
          std::size_t dimensions = 0;
          for (const auto &element : collection->elements)
          {
            std::string current = expression(*element);
            std::size_t contribution = 1;
            if (dynamic_cast<const parser::spread_expression *>(element.get()))
            {
              require(family == "Vector" || family == "Point",
                      family + " literals do not support spread elements", element->range);
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
          callable_signature signature;
          open_scope();
          for (const auto &parameter : lambda->parameters)
          {
            const std::string parameter_type = fixed_annotation(parameter.type_name);
            signature.parameters.push_back(parameter_type);
            add_binding(parameter.name, binding{parameter_type, {}});
          }
          const std::string body = expression(*lambda->body);
          signature.result = fixed_annotation(lambda->return_type);
          if (!is_unknown(signature.result)) require_compatible(signature.result, body, value.range, "Lambda return");
          else signature.result = body;
          close_scope();
          callables[&value] = std::move(signature);
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
        if (const auto *member = dynamic_cast<const parser::member_expression *>(&value))
        {
          if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get());
              target && enums.contains(target->name))
            throw semantic_error("Enum members are not assignable", value.range);
          const std::string target_type = expression(*member->target);
          const auto instantiated_target = generic_instance(target_type);
          if (const auto object = objects.find(instantiated_target.base); object != objects.end())
          {
            const auto field = object->second.fields.find(member->member_name);
            if (field != object->second.fields.end() && object->second.weak_fields.contains(member->member_name))
            {
              require(!member->safe, "Safe-access results are not assignable", value.range);
              require(!object->second.private_fields.contains(member->member_name) ||
                          (active_class && *active_class == instantiated_target.base),
                      "Private field '" + member->member_name + "' of class '" + target_type +
                          "' is not accessible here",
                      value.range);
              return substitute_type(field->second, object->second.type_parameters, instantiated_target.arguments);
            }
          }
          return expression(value);
        }
        if (dynamic_cast<const parser::index_expression *>(&value))
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
        require(value.name != "main" || value.type_parameters.empty(),
                "Entry function 'main' cannot be generic", value.range);
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

      auto constructor_assigned_fields(const parser::statement &value,
                                       std::unordered_set<std::string> assigned,
                                       const object_type &object) const
          -> std::unordered_set<std::string>
      {
        require(dynamic_cast<const parser::return_statement *>(&value) == nullptr,
                "Constructor cannot return", value.range);
        if (const auto *assignment = dynamic_cast<const parser::assignment_statement *>(&value))
        {
          if (assignment->operation == "=")
          {
            const auto *member = dynamic_cast<const parser::member_expression *>(assignment->target.get());
            const auto *target = member
                                     ? dynamic_cast<const parser::identifier_expression *>(member->target.get())
                                     : nullptr;
            if (member && target && target->name == "self" && object.fields.contains(member->member_name))
              assigned.insert(member->member_name);
          }
          return assigned;
        }
        if (const auto *block_value = dynamic_cast<const parser::block_statement *>(&value))
        {
          for (const auto &entry : block_value->statements)
            assigned = constructor_assigned_fields(*entry, std::move(assigned), object);
          return assigned;
        }
        if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          auto when_true = constructor_assigned_fields(*conditional->then_branch, assigned, object);
          auto when_false = assigned;
          if (conditional->else_branch)
            when_false = constructor_assigned_fields(*conditional->else_branch, assigned, object);
          std::erase_if(when_true, [&](const std::string &name) { return !when_false.contains(name); });
          return when_true;
        }
        if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          bool fallback = false;
          std::optional<std::unordered_set<std::string>> intersection;
          for (const auto &branch : matched->cases)
          {
            fallback |= !branch.pattern;
            auto branch_fields = constructor_assigned_fields(*branch.body, assigned, object);
            if (!intersection) intersection = std::move(branch_fields);
            else std::erase_if(*intersection, [&](const std::string &name) { return !branch_fields.contains(name); });
          }
          return fallback && intersection ? *intersection : assigned;
        }
        return assigned;
      }

      auto validate_constructor(const parser::function_declaration &value,
                                const object_type &object) const -> void
      {
        require(value.body != nullptr, "Constructor requires a block body", value.range);
        auto assigned = constructor_assigned_fields(*value.body, object.defaulted_fields, object);
        for (const auto &[field, unused] : object.fields)
        {
          static_cast<void>(unused);
          require(assigned.contains(field),
                  "Constructor may leave field '" + field + "' uninitialized", value.range);
        }
      }

      auto validate_cleanup_control(const parser::statement &value, const int local_loop_depth = 0) const -> void
      {
        require(dynamic_cast<const parser::return_statement *>(&value) == nullptr,
                "Finally cleanup cannot return", value.range);
        require(dynamic_cast<const parser::scream_statement *>(&value) == nullptr,
                "Finally cleanup cannot scream", value.range);
        if (dynamic_cast<const parser::loop_control_statement *>(&value))
          require(local_loop_depth > 0, "Finally cleanup cannot control an enclosing loop", value.range);
        if (const auto *block_value = dynamic_cast<const parser::block_statement *>(&value))
          for (const auto &entry : block_value->statements)
            validate_cleanup_control(*entry, local_loop_depth);
        else if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          validate_cleanup_control(*conditional->then_branch, local_loop_depth);
          if (conditional->else_branch) validate_cleanup_control(*conditional->else_branch, local_loop_depth);
        }
        else if (const auto *loop = dynamic_cast<const parser::condition_loop_statement *>(&value))
          validate_cleanup_control(*loop->body, local_loop_depth + 1);
        else if (const auto *loop = dynamic_cast<const parser::for_statement *>(&value))
          validate_cleanup_control(*loop->body, local_loop_depth + 1);
        else if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
          for (const auto &branch : matched->cases)
            validate_cleanup_control(*branch.body, local_loop_depth);
        else if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&value))
        {
          validate_cleanup_control(*hope->protected_body, local_loop_depth);
          for (const auto &handler : hope->handlers)
            validate_cleanup_control(*handler.body, local_loop_depth);
          if (hope->cleanup) validate_cleanup_control(*hope->cleanup, local_loop_depth);
        }
      }

      auto statement_returns(const parser::statement &value) const -> bool
      {
        if (dynamic_cast<const parser::return_statement *>(&value)) return true;
        if (dynamic_cast<const parser::scream_statement *>(&value)) return true;
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
          std::optional<std::string> covered_enum;
          std::unordered_set<std::string> covered_cases;
          for (const auto &branch : matched->cases)
          {
            has_fallback |= !branch.pattern;
            if (!block_returns(*branch.body)) return false;
            if (!branch.pattern) continue;
            if (const auto *call = dynamic_cast<const parser::call_expression *>(branch.pattern.get()))
            {
              if (const auto *callee = dynamic_cast<const parser::identifier_expression *>(call->callee.get()))
                if (const auto found = enum_cases.find(callee->name); found != enum_cases.end())
                {
                  if (!covered_enum) covered_enum = found->second.enum_name;
                  if (*covered_enum == found->second.enum_name) covered_cases.insert(callee->name);
                }
            }
            else if (const auto *member = dynamic_cast<const parser::member_expression *>(branch.pattern.get()))
            {
              if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get());
                  target && enums.contains(target->name))
              {
                if (!covered_enum) covered_enum = target->name;
                if (*covered_enum == target->name) covered_cases.insert(member->member_name);
              }
            }
          }
          return has_fallback || (covered_enum && covered_cases.size() == enums.at(*covered_enum).size());
        }
        if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&value))
        {
          if (!block_returns(*hope->protected_body)) return false;
          return std::all_of(hope->handlers.begin(), hope->handlers.end(), [&](const auto &handler)
          {
            return block_returns(*handler.body);
          });
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
          const auto previous_expected = expected_expression;
          if (!is_unknown(declared)) expected_expression = fixed_annotation(declaration->type_name);
          std::string inferred = declaration->initializer ? expression(*declaration->initializer)
                                                           : std::string(unknown_type);
          expected_expression = previous_expected;
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
          std::optional<callable_signature> callable;
          if (declaration->initializer)
          {
            if (const auto found = callables.find(declaration->initializer.get()); found != callables.end())
              callable = found->second;
          }
          if (!predeclared)
          {
            add_binding(declaration->name, binding{type, callable, declaration->initializer != nullptr});
          }
          else
          {
            scopes.back()[declaration->name].front().type = type;
            scopes.back()[declaration->name].front().callable = std::move(callable);
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
          if (assignment->operation != "=")
          {
            const std::string result = arithmetic_type(assignment->operation.substr(0, 1), target, assigned,
                                                       assignment->range);
            require_compatible(target, result, assignment->range, "Compound assignment");
          }
          else require_compatible(target, assigned, assignment->range, "Assignment");
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
          const auto previous_expected = expected_expression;
          if (!return_types.empty()) expected_expression = return_types.back();
          const std::string actual = returned->value ? expression(*returned->value) : std::string(void_type);
          expected_expression = previous_expected;
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
          bool has_some = false;
          bool has_none = false;
          std::unordered_set<std::string> covered_enum_cases;
          for (const auto &branch : matched->cases)
          {
            scopes = before;
            if (branch.pattern)
            {
              const auto *call = dynamic_cast<const parser::call_expression *>(branch.pattern.get());
              const auto *callee = call
                                       ? dynamic_cast<const parser::identifier_expression *>(call->callee.get())
                                       : nullptr;
              const auto *payload = call && call->arguments.size() == 1
                                        ? dynamic_cast<const parser::identifier_expression *>(call->arguments[0].get())
                                        : nullptr;
              const auto *name = dynamic_cast<const parser::identifier_expression *>(branch.pattern.get());
              const auto *member_pattern = dynamic_cast<const parser::member_expression *>(branch.pattern.get());
              if (callee && callee->name == "Some" && payload)
              {
                const auto contained = optional_element(subject);
                require(contained.has_value(), "Some pattern requires an Optional subject, but received " + subject,
                        branch.range);
                open_scope();
                add_binding(payload->name, binding{*contained, {}});
                model.declarations.push_back(typed_declaration{payload->range, payload->name, *contained});
                block(*branch.body);
                close_scope();
                has_some = true;
                paths.push_back(scopes);
                continue;
              }
              if (callee)
              {
                if (const auto enum_case = enum_cases.find(callee->name); enum_case != enum_cases.end())
                {
                  const auto matched_generic = generic_instance(subject);
                  require(enum_case->second.enum_name == matched_generic.base,
                          "Enum case '" + callee->name + "' belongs to " + enum_case->second.enum_name +
                              ", but the match subject is " + subject,
                          branch.range);
                  require(call->arguments.size() == enum_case->second.payload_types.size(),
                          "Enum case pattern '" + callee->name + "' expects " +
                              std::to_string(enum_case->second.payload_types.size()) + " binding(s)",
                          branch.range);
                  require(covered_enum_cases.insert(callee->name).second,
                          "Duplicate match case '" + callee->name + "'", branch.range);
                  open_scope();
                  for (std::size_t index = 0; index < call->arguments.size(); ++index)
                  {
                    const auto *binding_name = dynamic_cast<const parser::identifier_expression *>(call->arguments[index].get());
                    require(binding_name != nullptr, "Enum case payload patterns must be binding names", branch.range);
                    const std::string payload_type = substitute_type(enum_case->second.payload_types[index],
                                                                     enum_case->second.type_parameters,
                                                                     matched_generic.arguments);
                    add_binding(binding_name->name, binding{payload_type, {}});
                    model.declarations.push_back(
                        typed_declaration{binding_name->range, binding_name->name, payload_type});
                  }
                  block(*branch.body);
                  close_scope();
                  paths.push_back(scopes);
                  continue;
                }
              }
              if (name && name->name == "None")
              {
                require(optional_element(subject).has_value(),
                        "None pattern requires an Optional subject, but received " + subject, branch.range);
                static_cast<void>(expression(*branch.pattern));
                has_none = true;
              }
              else
              {
                const std::string pattern = expression(*branch.pattern);
                static_cast<void>(common_type(subject, pattern, branch.range, "Match subject and pattern"));
                if (member_pattern)
                {
                  const auto *enum_name = dynamic_cast<const parser::identifier_expression *>(member_pattern->target.get());
                  if (enum_name && enums.contains(enum_name->name))
                  {
                    require(enum_name->name == subject,
                            "Enum case '" + member_pattern->member_name + "' belongs to " + enum_name->name +
                                ", but the match subject is " + subject,
                            branch.range);
                    require(covered_enum_cases.insert(member_pattern->member_name).second,
                            "Duplicate match case '" + member_pattern->member_name + "'", branch.range);
                  }
                }
              }
            }
            else has_fallback = true;
            block(*branch.body);
            paths.push_back(scopes);
          }
          has_fallback |= has_some && has_none;
          if (const auto enum_type = enums.find(generic_instance(subject).base); enum_type != enums.end())
            has_fallback |= covered_enum_cases.size() == enum_type->second.size();
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
          if (hope->cleanup)
          {
            validate_cleanup_control(*hope->cleanup);
            block(*hope->cleanup);
          }
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
          const auto previous_class = active_class;
          if (type->type_kind == parser::type_declaration::kind::class_type) active_class = type->name;
          open_scope();
          if (type->type_kind == parser::type_declaration::kind::class_type ||
              type->type_kind == parser::type_declaration::kind::interface_type)
          {
            std::string self_type = type->name;
            if (!type->type_parameters.empty())
            {
              self_type += '<';
              for (std::size_t index = 0; index < type->type_parameters.size(); ++index)
              {
                if (index != 0) self_type += ", ";
                self_type += type->type_parameters[index];
              }
              self_type += '>';
            }
            add_binding("self", binding{std::move(self_type), {}});
          }
          for (const auto &member : type->members) predeclare(*member);
          for (const auto &member : type->members)
          {
            if (const auto *function_member = dynamic_cast<const parser::function_declaration *>(member.get());
                function_member && function_member->constructor_member)
              validate_constructor(*function_member, objects.at(type->name));
            statement(*member, true);
          }
          close_scope();
          active_class = previous_class;
        }
      }

    public:
      auto run(const parser::program &tree) -> type_model
      {
        add_binding("print", binding{"Function", callable_signature{{std::string(unknown_type)}, "Void", {}, {}}});
        add_binding("None", binding{"None", {}});
        add_binding("RuntimeError", binding{"Type", {}});
        enums["RuntimeError"] = {"integer_overflow", "division_by_zero", "modulo_by_zero",
                                  "undefined_exponentiation", "negative_integer_exponent",
                                  "index_out_of_bounds", "missing_key"};
        for (const auto &entry : tree.statements) predeclare(*entry);
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get())) collect_interface_type(*type);
        std::unordered_map<std::string, int> interface_states;
        for (const auto &[name, unused] : interfaces)
        {
          static_cast<void>(unused);
          resolve_interface(name, interface_states);
        }
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get())) collect_enum_type(*type);
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get())) collect_object_type(*type);
        validate_ownership(tree);
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
              type && type->type_kind == parser::type_declaration::kind::class_type)
          {
            const auto &object = objects.at(type->name);
            for (const auto &member : type->members)
              if (const auto *field = dynamic_cast<const parser::let_declaration *>(member.get());
                  field && field->weak_member)
              {
                require(field->type_name.has_value(), "Weak field '" + field->name + "' requires a type annotation",
                        field->range);
                require(!field->initializer, "Weak field '" + field->name + "' starts empty and cannot declare an initializer",
                        field->range);
                const std::string field_type = object.fields.at(field->name);
                const std::string field_base = generic_instance(field_type).base;
                require(objects.contains(field_base) || interfaces.contains(field_base),
                        "Weak field '" + field->name + "' requires a class or face type, but received " + field_type,
                        field->range);
              }
          }
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
              type && type->type_kind == parser::type_declaration::kind::class_type)
            validate_composition(type->name, objects.at(type->name), type->range);
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
