#include "cpp_generator.hpp"

#include "../semantic/semantic_error.hpp"
#include "../semantic/units.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace codegen
{
  namespace
  {
    class cpp_generator
    {
      const semantic::type_model &types;
      std::ostringstream output;
      int depth = 0;
      std::size_t temporary_index = 0;
      std::size_t array_index = 0;
      std::size_t dictionary_index = 0;
      std::size_t dimensioned_index = 0;
      std::unordered_set<std::string> user_types;
      std::unordered_set<std::string> enum_types;
      std::unordered_map<std::string, std::pair<std::string, std::size_t>> enum_cases;
      std::unordered_map<std::string, const parser::type_declaration *> enum_declarations;
      std::unordered_set<std::string> payload_enums;
      std::unordered_set<std::string> class_types;
      std::unordered_map<std::string, const parser::type_declaration *> face_types;
      std::unordered_map<std::string, std::unordered_set<std::string>> weak_fields;
      std::unordered_set<std::string> emitted_faces;
      std::unordered_set<std::string> active_type_parameters;
      std::unordered_map<std::string, const parser::function_declaration *> functions;
      semantic::units::registry unit_registry;
      std::vector<std::string> active_return_types;
      std::vector<std::unordered_set<std::string>> boxed_scopes;
      std::vector<std::unordered_set<std::string>> unboxed_scopes;
      bool in_method = false;
      bool map_enabled = false;
      std::optional<std::filesystem::path> default_source;
      std::optional<std::string> selected_test;
      std::optional<std::filesystem::path> active_source;
      std::string active_function;
      std::vector<source_map_entry> mappings;

      struct map_scope
      {
        cpp_generator &generator;
        std::size_t begin;
        parser::span source;
        bool breakpoint;
        std::optional<std::filesystem::path> source_path;
        std::string function;
        std::string previous_function;

        map_scope(cpp_generator &owner, const parser::span range, const bool can_break,
                  std::string function_name = {})
            : generator(owner), begin(static_cast<std::size_t>(owner.output.tellp())),
              source(range), breakpoint(can_break), source_path(owner.active_source),
              function(function_name.empty() ? owner.active_function : std::move(function_name)),
              previous_function(owner.active_function)
        {
          generator.active_function = function;
        }

        ~map_scope()
        {
          const auto end = static_cast<std::size_t>(generator.output.tellp());
          if (generator.map_enabled && end > begin && source.begin >= 0 && source.end >= source.begin)
            generator.mappings.push_back({begin, end, source, source_path, function, breakpoint});
          generator.active_function = previous_function;
        }
      };

      auto source_of(const parser::ast_node &node) -> void
      {
        active_source = node.origin_path ? node.origin_path : default_source;
      }

      auto active_source_utf8() const -> std::string
      {
        if (!active_source) return {};
        const auto encoded = active_source->generic_u8string();
        return {reinterpret_cast<const char *>(encoded.data()), encoded.size()};
      }

      auto open_boxed_scope() -> void { boxed_scopes.emplace_back(); unboxed_scopes.emplace_back(); }
      auto close_boxed_scope() -> void { boxed_scopes.pop_back(); unboxed_scopes.pop_back(); }
      auto box(const std::string &name) -> void { boxed_scopes.back().insert(name); }
      auto unbox(const std::string &name) -> void { unboxed_scopes.back().insert(name); }
      auto boxed(const std::string &name) const -> bool
      {
        for (std::size_t index = boxed_scopes.size(); index > 0; --index)
        {
          if (boxed_scopes[index - 1].contains(name)) return true;
          if (unboxed_scopes[index - 1].contains(name)) return false;
        }
        return false;
      }

      auto function_components(const std::string_view name) const
          -> std::optional<std::pair<std::vector<std::string>, std::string>>
      {
        if (!name.starts_with('(')) return {};
        int depth = 0;
        std::size_t close = std::string_view::npos;
        for (std::size_t index = 0; index < name.size(); ++index)
        {
          if (name[index] == '(') ++depth;
          else if (name[index] == ')' && --depth == 0) { close = index; break; }
        }
        if (close == std::string_view::npos || name.substr(close, 4) != ") =>") return {};
        const std::string_view text = name.substr(1, close - 1);
        std::vector<std::string> parameters;
        std::size_t begin = 0;
        depth = 0;
        int angle_depth = 0;
        for (std::size_t index = 0; index <= text.size(); ++index)
        {
          if (index < text.size() && text[index] == '(') ++depth;
          else if (index < text.size() && text[index] == ')') --depth;
          else if (index < text.size() && text[index] == '<') ++angle_depth;
          else if (index < text.size() && text[index] == '>') --angle_depth;
          if (index == text.size() || (text[index] == ',' && depth == 0 && angle_depth == 0))
          {
            std::string parameter(text.substr(begin, index - begin));
            while (!parameter.empty() && parameter.front() == ' ') parameter.erase(parameter.begin());
            while (!parameter.empty() && parameter.back() == ' ') parameter.pop_back();
            if (!parameter.empty()) parameters.push_back(std::move(parameter));
            begin = index + 1;
          }
        }
        std::string result(name.substr(close + 4));
        while (!result.empty() && result.front() == ' ') result.erase(result.begin());
        return std::pair{std::move(parameters), std::move(result)};
      }

      auto indentation() const -> std::string
      {
        return std::string(static_cast<std::size_t>(depth) * 2, ' ');
      }

      auto fail(const std::string &message, const parser::span range) const -> void
      {
        throw semantic::semantic_error("C++ backend: " + message, range, active_source);
      }

      auto identifier(const std::string &name) const -> std::string
      {
        if (name == "self" && in_method) return "(*this)";
        return generated_identifier(name);
      }

      auto type_name(const std::string &name, const parser::span range,
                     const bool entry = false) const -> std::string
      {
        if (entry && name == "Int") return "int";
        if (entry && name == "Void") return "int";
        if (name == "Void") return "void";
        if (name == "Bool") return "bool";
        if (name == "String") return "std::string";
        if (name == "RuntimeError") return "sagan_runtime_error";
        if (name == "Float" || name == "Float64") return "double";
        if (name == "Float32") return "float";
        if (name == "Int" || name == "Int64") return "std::int64_t";
        if (name == "Int32") return "std::int32_t";
        if (name == "Int16") return "std::int16_t";
        if (name == "Int8") return "std::int8_t";
        if (const auto function = function_components(name))
        {
          std::string result = "std::function<" + type_name(function->second, range) + "(";
          for (std::size_t index = 0; index < function->first.size(); ++index)
          {
            if (index != 0) result += ", ";
            result += type_name(function->first[index], range);
          }
          return result + ")>";
        }
        if (const auto measured = semantic::units::parse_measured_type(name, unit_registry, range))
          return type_name(measured->numeric, range, entry);
        if (active_type_parameters.contains(name)) return identifier(name);
        if (std::string_view{name}.starts_with("Optional<") && name.ends_with('>'))
          return "std::optional<" + type_name(name.substr(9, name.size() - 10), range) + ">";
        const std::size_t generic_open = name.find('<');
        const std::string generic_base = generic_open == std::string::npos ? name : name.substr(0, generic_open);
        if (generic_base == "Array")
        {
          const auto arguments = generic_arguments(name);
          if (arguments.size() == 1) return "std::vector<" + type_name(arguments.front(), range) + ">";
        }
        if (generic_base == "Dictionary")
        {
          const auto arguments = generic_arguments(name);
          if (arguments.size() == 2)
            return "std::unordered_map<" + type_name(arguments[0], range) + ", " +
                   type_name(arguments[1], range) + ">";
        }
        if (class_types.contains(generic_base) || face_types.contains(generic_base))
        {
          std::string concrete = identifier(generic_base);
          const auto arguments = generic_arguments(name);
          if (!arguments.empty())
          {
            concrete += '<';
            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
              if (index != 0) concrete += ", ";
              concrete += type_name(arguments[index], range);
            }
            concrete += '>';
          }
          return "std::shared_ptr<" + concrete + ">";
        }
        if (user_types.contains(name)) return identifier(name);
        if (const std::size_t open = name.find('<'); open != std::string::npos && name.ends_with('>') &&
            user_types.contains(name.substr(0, open)))
          return identifier(name.substr(0, open));
        for (const std::string_view family : {std::string_view{"SphericalVector"},
                                              std::string_view{"SphericalPoint"},
                                              std::string_view{"Vector"}, std::string_view{"Point"}})
        {
          if (!std::string_view{name}.starts_with(family)) continue;
          const std::size_t open = name.find('<', family.size());
          if (open == std::string::npos || !name.ends_with('>')) break;
          const std::string dimension_text = name.substr(family.size(), open - family.size());
          if (dimension_text.empty() ||
              !std::all_of(dimension_text.begin(), dimension_text.end(), [](const unsigned char value)
              {
                return std::isdigit(value) != 0;
              })) break;
          std::string component = name.substr(open + 1, name.size() - open - 2);
          const auto arguments = generic_arguments(name);
          if (arguments.size() == 2) component = arguments.front();
          const std::string runtime_family = family == "Vector"          ? "sagan_vector"
                                             : family == "Point"         ? "sagan_point"
                                             : family == "SphericalVector" ? "sagan_spherical_vector"
                                                                            : "sagan_spherical_point";
          return runtime_family + "<" + type_name(component, range) + ", " + dimension_text + ">";
        }
        fail("type '" + name + "' is not available in the initial native subset", range);
        return {};
      }

      auto type(const std::optional<std::string> &name, const parser::span range,
                const bool entry = false) const -> std::string
      {
        if (!name) fail("native functions require explicit parameter and return types", range);
        return type_name(*name, range, entry);
      }

      auto expression_type(const parser::expression &value) const -> std::string
      {
        for (auto entry = types.expressions.rbegin(); entry != types.expressions.rend(); ++entry)
        {
          if (entry->range.begin == value.range.begin && entry->range.end == value.range.end) return entry->type;
        }
        fail("missing checked type for expression", value.range);
        return {};
      }

      auto dimensioned_component_type(const std::string &value, const parser::span range) const -> std::string
      {
        const auto open = value.find('<');
        if (open == std::string::npos || !value.ends_with('>'))
          fail("expected a dimensioned value type", range);
        return value.substr(open + 1, value.size() - open - 2);
      }

      auto ratio(const semantic::units::rational value) const -> std::string
      {
        std::string result = "(" + std::to_string(value.numerator) + ".0 / " +
                             std::to_string(value.denominator) + ".0)";
        if (value.decimal_exponent != 0)
          result = "(" + result + " * std::pow(10.0, " + std::to_string(value.decimal_exponent) + "))";
        if (value.pi_exponent != 0)
          result = "(" + result + " * std::pow(std::acos(-1.0), " + std::to_string(value.pi_exponent) + "))";
        return result;
      }

      auto convert_value(std::string value, const std::string &source_type,
                         const std::string &target_type, const parser::span range) const -> std::string
      {
        const auto source = semantic::units::parse_measured_type(source_type, unit_registry, range);
        const auto target = semantic::units::parse_measured_type(target_type, unit_registry, range);
        if (!source || !target)
        {
          const auto component = [](const std::string &type) -> std::optional<std::string>
          {
            if (!std::string_view{type}.starts_with("Vector") && !std::string_view{type}.starts_with("Point") &&
                !std::string_view{type}.starts_with("Spherical")) return {};
            const auto open = type.find('<');
            if (open == std::string::npos || !type.ends_with('>')) return {};
            return type.substr(open + 1, type.size() - open - 2);
          };
          const auto source_component = component(source_type);
          const auto target_component = component(target_type);
          if (!source_component || !target_component) return value;
          const auto source_unit = semantic::units::parse_measured_type(*source_component, unit_registry, range);
          const auto target_unit = semantic::units::parse_measured_type(*target_component, unit_registry, range);
          if (!source_unit || !target_unit || source_unit->unit.name == target_unit->unit.name) return value;
          const auto factor = semantic::units::divide(source_unit->unit.scale, target_unit->unit.scale);
          const auto source_open = source_type.find('<');
          const std::string converted_type = source_type.substr(0, source_open + 1) + *target_component + '>';
          return "sagan_multiply<" + type_name(converted_type, range) + ">(" + value + ", " + ratio(factor) + ")";
        }
        if (source->unit.name == target->unit.name) return value;
        const bool affine = source->unit.kind == semantic::units::category::affine_point;
        std::string canonical = "((" + value + ") * " + ratio(source->unit.scale);
        if (affine) canonical += " + " + ratio(source->unit.offset);
        canonical += ')';
        std::string converted = "((" + canonical;
        if (affine) converted += " - " + ratio(target->unit.offset);
        converted += ") / " + ratio(target->unit.scale) + ')';
        return converted;
      }

      auto converted_expression(const parser::expression &value, const std::string &target) -> std::string
      {
        return convert_value(expression(value), expression_type(value), target, value.range);
      }

      auto call_parameters(const parser::call_expression &value) const -> const std::vector<std::string> *
      {
        for (auto entry = types.calls.rbegin(); entry != types.calls.rend(); ++entry)
          if (entry->range.begin == value.range.begin && entry->range.end == value.range.end)
            return &entry->parameter_types;
        return nullptr;
      }

      auto binding_type(const parser::identifier_expression &value) const -> std::string
      {
        for (auto entry = types.declarations.rbegin(); entry != types.declarations.rend(); ++entry)
          if (entry->range.begin == value.range.begin && entry->range.end == value.range.end)
            return entry->type;
        return expression_type(value);
      }

      auto declaration_type(const parser::span range, const std::string &name) const -> std::string
      {
        for (auto entry = types.declarations.rbegin(); entry != types.declarations.rend(); ++entry)
          if (entry->range.begin == range.begin && entry->range.end == range.end && entry->name == name)
            return entry->type;
        fail("missing checked type for declaration '" + name + "'", range);
        return {};
      }

      auto declaration_type(const parser::let_declaration &value) const -> std::string
      {
        return declaration_type(value.range, value.name);
      }

      auto weak_member(const parser::member_expression &value) const -> bool
      {
        const std::string target = expression_type(*value.target);
        const auto fields = weak_fields.find(target.substr(0, target.find('<')));
        return fields != weak_fields.end() && fields->second.contains(value.member_name);
      }

      auto raw_member(const parser::member_expression &value) -> std::string
      {
        const std::string target_type = expression_type(*value.target);
        const std::string target_base = target_type.substr(0, target_type.find('<'));
        const auto *self = dynamic_cast<const parser::identifier_expression *>(value.target.get());
        const bool reference = (class_types.contains(target_base) || face_types.contains(target_base)) &&
                               !(self && self->name == "self");
        return expression(*value.target) + (reference ? "->" : ".") +
               generic_identifier(value.member_name, value.range);
      }

      auto enum_factory(const std::string &enum_name, const std::string &case_name) const -> std::string
      {
        return identifier(enum_name + "__" + case_name);
      }

      auto enum_numeric_suffix(const parser::type_declaration::enum_member &member) const -> std::string
      {
        if (!member.numeric_value) return {};
        std::string value = *member.numeric_value;
        value.erase(std::remove(value.begin(), value.end(), '_'), value.end());
        return " = " + value;
      }

      auto generic_arguments(const std::string &type) const -> std::vector<std::string>
      {
        const std::size_t open = type.find('<');
        if (open == std::string::npos || !type.ends_with('>')) return {};
        const std::string contents = type.substr(open + 1, type.size() - open - 2);
        std::vector<std::string> result;
        std::size_t begin = 0;
        int depth = 0;
        for (std::size_t index = 0; index <= contents.size(); ++index)
        {
          if (index < contents.size() && contents[index] == '<') ++depth;
          else if (index < contents.size() && contents[index] == '>') --depth;
          if (index == contents.size() || (contents[index] == ',' && depth == 0))
          {
            std::string argument = contents.substr(begin, index - begin);
            while (!argument.empty() && argument.front() == ' ') argument.erase(argument.begin());
            result.push_back(std::move(argument));
            begin = index + 1;
          }
        }
        return result;
      }

      auto generic_identifier(const std::string &name, const parser::span range) const -> std::string
      {
        const std::size_t open = name.find('<');
        std::string result = identifier(open == std::string::npos ? name : name.substr(0, open));
        const auto arguments = generic_arguments(name);
        if (arguments.empty()) return result;
        result += '<';
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
          if (index != 0) result += ", ";
          result += type_name(arguments[index], range);
        }
        return result + '>';
      }

      auto concrete_user_type(const std::string &type, const parser::span range) const -> std::string
      {
        const std::size_t open = type.find('<');
        const std::string base = open == std::string::npos ? type : type.substr(0, open);
        std::string result = identifier(base);
        const auto arguments = generic_arguments(type);
        if (!arguments.empty())
        {
          result += '<';
          for (std::size_t index = 0; index < arguments.size(); ++index)
          {
            if (index != 0) result += ", ";
            result += type_name(arguments[index], range);
          }
          result += '>';
        }
        return result;
      }

      auto assignable(const parser::expression &value) -> std::string
      {
        if (const auto *member = dynamic_cast<const parser::member_expression *>(&value);
            member && weak_member(*member))
          return raw_member(*member);
        return expression(value);
      }

      auto dictionary_components(const std::string_view checked,
                                 const parser::span range) const -> std::pair<std::string, std::string>
      {
        constexpr std::string_view prefix = "Dictionary<";
        if (!checked.starts_with(prefix) || !checked.ends_with('>'))
          fail("dictionary expression has no checked key and value types", range);
        const std::string_view contents = checked.substr(prefix.size(), checked.size() - prefix.size() - 1);
        int depth_value = 0;
        for (std::size_t index_value = 0; index_value < contents.size(); ++index_value)
        {
          if (contents[index_value] == '<') ++depth_value;
          else if (contents[index_value] == '>') --depth_value;
          else if (contents[index_value] == ',' && depth_value == 0)
          {
            std::string key(contents.substr(0, index_value));
            std::string mapped(contents.substr(index_value + 1));
            if (!mapped.empty() && mapped.front() == ' ') mapped.erase(mapped.begin());
            return {std::move(key), std::move(mapped)};
          }
        }
        fail("dictionary expression has malformed checked type '" + std::string(checked) + "'", range);
        return {};
      }

      auto escaped_string(const std::string &value) const -> std::string
      {
        std::ostringstream escaped;
        escaped << '"';
        for (const unsigned char character : value)
        {
          switch (character)
          {
            case '\\': escaped << "\\\\"; break;
            case '"': escaped << "\\\""; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            case '\0': escaped << "\\0"; break;
            default: escaped << static_cast<char>(character); break;
          }
        }
        escaped << '"';
        return escaped.str();
      }

      auto displayed_expression(const parser::expression &value) -> std::string
      {
        const std::string type = expression_type(value);
        std::optional<semantic::units::measured_type> measured =
            semantic::units::parse_measured_type(type, unit_registry, value.range);
        if (!measured && (type.starts_with("Vector") || type.starts_with("Point") ||
                          type.starts_with("SphericalVector") || type.starts_with("SphericalPoint")))
        {
          const auto arguments = generic_arguments(type);
          if (arguments.size() == 1)
            measured = semantic::units::parse_measured_type(arguments.front(), unit_registry, value.range);
          else if (arguments.size() == 2)
            measured = semantic::units::parse_measured_type(arguments.front() + '<' + arguments.back() + '>',
                                                            unit_registry, value.range);
        }
        std::string result = expression(value);
        if (measured)
          result = "sagan_display_unit(" + result + ", " + escaped_string(measured->unit.name) + ")";
        return result;
      }

      auto operation(const std::string &value, const parser::span range) const -> std::string
      {
        if (value == "and") return "&&";
        if (value == "or") return "||";
        if (value == "is") return "==";
        if (value == "is not") return "!=";
        if (value == "==" || value == "!=" || value == "<" || value == "<=" || value == ">" ||
            value == ">=") return value;
        fail("operator '" + value + "' is not available in the initial native subset", range);
        return {};
      }

      auto expression(const parser::expression &value) -> std::string
      {
        if (const auto *name = dynamic_cast<const parser::identifier_expression *>(&value))
        {
          if (name->name == "None") return "std::nullopt";
          const std::string generated = generic_identifier(name->name, value.range);
          return boxed(name->name) ? "sagan_box_value(" + generated + ")" : generated;
        }
        if (const auto *literal = dynamic_cast<const parser::literal_expression *>(&value))
        {
          if (literal->spelling == "nan") return "std::numeric_limits<double>::quiet_NaN()";
          if (literal->spelling == "inf") return "std::numeric_limits<double>::infinity()";
          std::string spelling = literal->spelling;
          spelling.erase(std::remove(spelling.begin(), spelling.end(), '_'), spelling.end());
          return spelling;
        }
        if (const auto *measured = dynamic_cast<const parser::measured_expression *>(&value))
        {
          const std::string inner = expression(*measured->value);
          return measured->conversion
                     ? convert_value(inner, expression_type(*measured->value), expression_type(value), value.range)
                     : inner;
        }
        if (const auto *string = dynamic_cast<const parser::string_expression *>(&value))
        {
          std::string result = "std::string{}";
          for (const auto &part : string->parts)
          {
            result += part.interpolation
                          ? " + sagan_stringify(" + displayed_expression(*part.interpolation) + ")"
                          : " + std::string{" + escaped_string(part.text) + "}";
          }
          return "(" + result + ")";
        }
        if (const auto *group = dynamic_cast<const parser::grouping_expression *>(&value))
          return "(" + expression(*group->value) + ")";
        if (const auto *unary = dynamic_cast<const parser::unary_expression *>(&value))
        {
          if (unary->operator_text == "++" || unary->operator_text == "--")
          {
            return "sagan_" + std::string(unary->operator_text == "++" ? "increment" : "decrement") + "<" +
                   type_name(expression_type(value), value.range) + ">(" + expression(*unary->operand) + ", " +
                   (unary->postfix ? "true" : "false") + ")";
          }
          if (unary->postfix) fail("unsupported postfix operator", value.range);
          const std::string op = unary->operator_text == "not" ? "!" : unary->operator_text;
          if (op != "!" && op != "+" && op != "-") fail("unsupported unary operator", value.range);
          if (op == "-")
          {
            if (const auto *literal = dynamic_cast<const parser::literal_expression *>(unary->operand.get()))
              return "static_cast<" + type_name(expression_type(*unary->operand), unary->operand->range) + ">(-" +
                     expression(*literal) + ")";
            return "sagan_negate<" + type_name(expression_type(*unary->operand), unary->operand->range) + ">(" +
                   expression(*unary->operand) + ")";
          }
          if (op == "+" && std::string_view{expression_type(value)}.starts_with("Vector"))
            return expression(*unary->operand);
          return "(" + op + expression(*unary->operand) + ")";
        }
        if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
        {
          if (binary->operator_text == "??")
          {
            const std::string temporary = "sagan_optional_" + std::to_string(temporary_index++);
            const std::string left_type = expression_type(*binary->left);
            const std::string element = left_type.substr(9, left_type.size() - 10);
            const bool optional_fallback = std::string_view{expression_type(*binary->right)}.starts_with("Optional<");
            return "([&]() { auto " + temporary + " = " + expression(*binary->left) + "; return " + temporary +
                   " ? " + (optional_fallback ? "std::optional<" + type_name(element, value.range) + ">{*" : "*") +
                   temporary + (optional_fallback ? "}" : "") + " : " + expression(*binary->right) + "; }())";
          }
          if (binary->operator_text == "^")
            return "sagan_power<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*binary->left) + ", " + expression(*binary->right) + ")";
          const std::string checked_type = type_name(expression_type(value), value.range);
          if (binary->operator_text == "+")
            return "sagan_add<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   converted_expression(*binary->right, expression_type(*binary->left)) + ")";
          if (binary->operator_text == "-")
            return "sagan_subtract<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   converted_expression(*binary->right, expression_type(*binary->left)) + ")";
          if (binary->operator_text == "*")
            return "sagan_multiply<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "/")
          {
            const std::string left_type = expression_type(*binary->left);
            const std::string division_type = std::string_view{left_type}.starts_with("Vector")
                                                  ? type_name(left_type, binary->left->range)
                                                  : checked_type;
            return "sagan_divide<" + division_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          }
          if (binary->operator_text == "%")
            return "sagan_modulo<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "==" || binary->operator_text == "!=" ||
              binary->operator_text == "<" || binary->operator_text == "<=" ||
              binary->operator_text == ">" || binary->operator_text == ">=")
            return "(" + expression(*binary->left) + " " + operation(binary->operator_text, value.range) + " " +
                   converted_expression(*binary->right, expression_type(*binary->left)) + ")";
          return "(" + expression(*binary->left) + " " + operation(binary->operator_text, value.range) + " " +
                 expression(*binary->right) + ")";
        }
        if (const auto *conditional = dynamic_cast<const parser::conditional_expression *>(&value))
          return "(" + expression(*conditional->condition) + " ? " +
                 converted_expression(*conditional->when_true, expression_type(value)) + " : " +
                 converted_expression(*conditional->when_false, expression_type(value)) + ")";
        if (const auto *assignment = dynamic_cast<const parser::assignment_expression *>(&value))
          return "(" + assignable(*assignment->target) + " = " +
                 converted_expression(*assignment->value, expression_type(value)) + ")";
        if (const auto *call = dynamic_cast<const parser::call_expression *>(&value))
        {
          const auto *called_name = dynamic_cast<const parser::identifier_expression *>(call->callee.get());
          if (called_name && called_name->name == "print" && call->arguments.size() == 1)
            return "sagan_print(" + displayed_expression(*call->arguments.front()) + ")";
          if (called_name && called_name->name == "sqrt")
            return "sagan_math_sqrt(" + expression(*call->arguments.front()) + ")";
          if (called_name && called_name->name == "squared_length")
            return "sagan_math_squared_length<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*call->arguments.front()) + ")";
          if (called_name && called_name->name == "length")
            return "sagan_math_length<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*call->arguments.front()) + ")";
          if (called_name && called_name->name == "dot")
            return "sagan_math_dot<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*call->arguments[0]) + ", " +
                   converted_expression(*call->arguments[1], expression_type(*call->arguments[0])) + ")";
          if (called_name && called_name->name == "normalized")
            return "sagan_math_normalized<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*call->arguments.front()) + ")";
          if (called_name && called_name->name == "display_coordinates")
            return "sagan_math_display_coordinates<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*call->arguments[0]) + ", " +
                   converted_expression(*call->arguments[1], expression_type(*call->arguments[0])) + ", " +
                   converted_expression(*call->arguments[2],
                                        dimensioned_component_type(expression_type(*call->arguments[0]), value.range)) +
                   ")";
          if (called_name && called_name->name == "Some")
            return "std::optional<" + type_name(expression_type(*call->arguments.front()), value.range) + ">{" +
                   expression(*call->arguments.front()) + "}";
          if (called_name && enum_cases.contains(called_name->name))
          {
            const auto &enum_case = enum_cases.at(called_name->name);
            const auto declaration = enum_declarations.at(enum_case.first);
            if (!declaration->type_parameters.empty())
            {
              const auto instantiated = generic_arguments(expression_type(value));
              const auto &case_declaration = declaration->enum_members[enum_case.second];
              std::string result = identifier(enum_case.first) + "{" + identifier(enum_case.first) + "::Tag::" +
                                   identifier(called_name->name) + ", {";
              for (std::size_t index = 0; index < call->arguments.size(); ++index)
              {
                if (index != 0) result += ", ";
                std::string payload_type = case_declaration.payload_types[index];
                for (std::size_t parameter = 0; parameter < declaration->type_parameters.size(); ++parameter)
                  if (payload_type == declaration->type_parameters[parameter] && parameter < instantiated.size())
                    payload_type = instantiated[parameter];
                result += type_name(payload_type, call->arguments[index]->range) + "{" +
                          expression(*call->arguments[index]) + "}";
              }
              return result + "}}";
            }
            std::string result = enum_factory(enum_cases.at(called_name->name).first, called_name->name) + "(";
            for (std::size_t index = 0; index < call->arguments.size(); ++index)
            {
              if (index != 0) result += ", ";
              const auto *parameters = call_parameters(*call);
              result += parameters && index < parameters->size()
                          ? converted_expression(*call->arguments[index], parameters->at(index))
                          : expression(*call->arguments[index]);
            }
            return result + ")";
          }
          if (const auto *member = dynamic_cast<const parser::member_expression *>(call->callee.get()))
          {
            if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get()))
            {
              if (target->name == "Int" && member->member_name == "round")
                return "sagan_round_int(" + expression(*call->arguments.front()) + ")";
              const std::size_t open = target->name.find('<');
              const std::string base = open == std::string::npos ? target->name : target->name.substr(0, open);
              if (const auto found = enum_cases.find(member->member_name);
                  found != enum_cases.end() && found->second.first == base)
              {
                const auto declaration = enum_declarations.at(base);
                const auto instantiated = generic_arguments(expression_type(value));
                const auto &case_declaration = declaration->enum_members[found->second.second];
                std::string result = identifier(base) + "{" + identifier(base) + "::Tag::" +
                                     identifier(member->member_name) + ", {";
                for (std::size_t index = 0; index < call->arguments.size(); ++index)
                {
                  if (index != 0) result += ", ";
                  std::string payload_type = case_declaration.payload_types[index];
                  for (std::size_t parameter = 0; parameter < declaration->type_parameters.size(); ++parameter)
                    if (payload_type == declaration->type_parameters[parameter] && parameter < instantiated.size())
                      payload_type = instantiated[parameter];
                  result += type_name(payload_type, call->arguments[index]->range) + "{" +
                            expression(*call->arguments[index]) + "}";
                }
                return result + "}}";
              }
            }
          }
          if (const auto *member = dynamic_cast<const parser::member_expression *>(call->callee.get());
              member && member->safe)
          {
            const std::string temporary = "sagan_optional_" + std::to_string(temporary_index++);
            const std::string result_type = expression_type(value);
            const std::string element = result_type.substr(9, result_type.size() - 10);
            std::string result = "([&]() { auto " + temporary + " = " + expression(*member->target) +
                                 "; if (!" + temporary + ") return std::optional<" +
                                 type_name(element, value.range) + ">{std::nullopt}; return std::optional<" +
                                 type_name(element, value.range) + ">{(*" + temporary + ")->" +
                                 generic_identifier(member->member_name, member->range) + "(";
            for (std::size_t index = 0; index < call->arguments.size(); ++index)
            {
              if (index != 0) result += ", ";
              result += expression(*call->arguments[index]);
            }
            return result + ")}; }())";
          }
          std::string result = expression(*call->callee) + "(";
          if (called_name && class_types.contains(called_name->name.substr(0, called_name->name.find('<'))))
          {
            std::string runtime_type = type_name(expression_type(value), value.range);
            constexpr std::string_view shared_prefix = "std::shared_ptr<";
            runtime_type = runtime_type.substr(shared_prefix.size(), runtime_type.size() - shared_prefix.size() - 1);
            result = "std::make_shared<" + runtime_type + ">(";
          }
          for (std::size_t index = 0; index < call->arguments.size(); ++index)
          {
            if (index != 0) result += ", ";
            const auto *parameters = call_parameters(*call);
            result += parameters && index < parameters->size()
                        ? converted_expression(*call->arguments[index], parameters->at(index))
                        : expression(*call->arguments[index]);
          }
          return result + ")";
        }
        if (const auto *index = dynamic_cast<const parser::index_expression *>(&value))
        {
          const std::string target_type = expression_type(*index->target);
          const std::string helper = std::string_view{target_type}.starts_with("Dictionary<")
                                         ? "sagan_dictionary_at" : "sagan_index";
          return helper + "(" + expression(*index->target) + ", " + expression(*index->index) + ")";
        }
        if (const auto *member = dynamic_cast<const parser::member_expression *>(&value))
        {
          if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get());
              target && target->name == "RuntimeError")
            return "sagan_runtime_error::" + member->member_name;
          if (member->safe)
          {
            const std::string temporary = "sagan_optional_" + std::to_string(temporary_index++);
            const std::string result_type = expression_type(value);
            const std::string element = result_type.substr(9, result_type.size() - 10);
            return "([&]() { auto " + temporary + " = " + expression(*member->target) + "; return " +
                   temporary + " ? std::optional<" + type_name(element, value.range) + ">{(*" + temporary +
                   ")->" + identifier(member->member_name) + "} : std::nullopt; }())";
          }
          if (const auto *target = dynamic_cast<const parser::identifier_expression *>(member->target.get());
              target && enum_types.contains(target->name.substr(0, target->name.find('<'))))
          {
            const std::string base = target->name.substr(0, target->name.find('<'));
            if (payload_enums.contains(base)) return enum_factory(base, member->member_name) + "()";
            return identifier(base) + "::" + identifier(member->member_name);
          }
          const std::string target_type = expression_type(*member->target);
          if (member->member_name == "times" &&
              (target_type == "Int" || target_type == "Int8" || target_type == "Int16" ||
               target_type == "Int32" || target_type == "Int64"))
            return "sagan_times(" + expression(*member->target) + ")";
          if (std::string_view{target_type}.starts_with("Vector") ||
              std::string_view{target_type}.starts_with("Point") ||
              std::string_view{target_type}.starts_with("Spherical"))
          {
            std::size_t component = std::string_view::npos;
            if (std::string_view{target_type}.starts_with("Spherical"))
            {
              const std::array<std::string_view, 3> names =
                  std::string_view{target_type}.starts_with("SphericalPoint")
                      ? std::array<std::string_view, 3>{"radius", "inclination", "azimuth"}
                      : std::array<std::string_view, 3>{"magnitude", "inclination", "azimuth"};
              const auto found = std::find(names.begin(), names.end(), member->member_name);
              if (found != names.end()) component = static_cast<std::size_t>(found - names.begin());
            }
            else
            {
              const std::string_view names = "xyzw";
              if (member->member_name.size() == 1) component = names.find(member->member_name);
            }
            if (component == std::string_view::npos) fail("dimensioned member access is invalid", value.range);
            return expression(*member->target) + ".at(" + std::to_string(component) + ")";
          }
          if (weak_member(*member)) return "sagan_lock_weak(" + raw_member(*member) + ")";
          return raw_member(*member);
        }
        if (const auto *collection = dynamic_cast<const parser::collection_expression *>(&value))
        {
          const std::string checked = expression_type(value);
          if (collection->collection_kind != parser::collection_expression::kind::array)
          {
            const std::string runtime_type = type_name(checked, value.range);
            const bool has_spread = std::any_of(collection->elements.begin(), collection->elements.end(),
                                                [](const auto &entry)
            {
              return dynamic_cast<const parser::spread_expression *>(entry.get()) != nullptr;
            });
            if (has_spread)
            {
              const std::size_t current_index = dimensioned_index++;
              const std::string temporary = "sagan_dimensioned_" + std::to_string(current_index);
              const std::string cursor = temporary + "_cursor";
              std::string result = "([&]() { " + runtime_type + " " + temporary + "{}; std::size_t " + cursor +
                                   " = 0; ";
              for (std::size_t index_value = 0; index_value < collection->elements.size(); ++index_value)
              {
                if (const auto *spread =
                        dynamic_cast<const parser::spread_expression *>(collection->elements[index_value].get()))
                {
                  const std::string spread_temporary = "sagan_dimensioned_spread_" +
                                                       std::to_string(current_index) + "_" +
                                                       std::to_string(index_value);
                  result += "const auto &" + spread_temporary + " = " + expression(*spread->value) + "; ";
                  result += "for (const auto &sagan_component : " + spread_temporary + ") " + temporary +
                            ".components[" + cursor + "++] = sagan_component; ";
                }
                else
                  result += temporary + ".components[" + cursor + "++] = " +
                            expression(*collection->elements[index_value]) + "; ";
              }
              return result + "return " + temporary + "; }())";
            }
            std::string result = runtime_type + "{{";
            for (std::size_t index_value = 0; index_value < collection->elements.size(); ++index_value)
            {
              if (index_value != 0) result += ", ";
              result += expression(*collection->elements[index_value]);
            }
            return result + "}}";
          }
          constexpr std::string_view prefix = "Array<";
          if (!checked.starts_with(prefix) || !checked.ends_with('>'))
            fail("array expression has no checked element type", value.range);
          const std::string element = checked.substr(prefix.size(), checked.size() - prefix.size() - 1);
          const bool has_spread = std::any_of(collection->elements.begin(), collection->elements.end(),
                                              [](const auto &entry)
          {
            return dynamic_cast<const parser::spread_expression *>(entry.get()) != nullptr;
          });
          if (has_spread)
          {
            const std::size_t current_array_index = array_index++;
            const std::string temporary = "sagan_array_" + std::to_string(current_array_index);
            std::string result = "([&]() { std::vector<" + type_name(element, value.range) + "> " + temporary + "; ";
            for (std::size_t index_value = 0; index_value < collection->elements.size(); ++index_value)
            {
              if (const auto *spread =
                      dynamic_cast<const parser::spread_expression *>(collection->elements[index_value].get()))
              {
                const std::string spread_temporary =
                    "sagan_spread_" + std::to_string(current_array_index) + "_" + std::to_string(index_value);
                result += "const auto &" + spread_temporary + " = " + expression(*spread->value) + "; ";
                const std::string spread_type = expression_type(*spread->value);
                const std::string source_element = spread_type.substr(prefix.size(),
                    spread_type.size() - prefix.size() - 1);
                result += "for (const auto &sagan_element : " + spread_temporary + ") " + temporary +
                          ".push_back(" + convert_value("sagan_element", source_element, element, spread->range) + "); ";
              }
              else result += temporary + ".push_back(" +
                             converted_expression(*collection->elements[index_value], element) + "); ";
            }
            return result + "return " + temporary + "; }())";
          }
          std::string result = "std::vector<" + type_name(element, value.range) + ">{";
          for (std::size_t index_value = 0; index_value < collection->elements.size(); ++index_value)
          {
            if (index_value != 0) result += ", ";
            result += converted_expression(*collection->elements[index_value], element);
          }
          return result + "}";
        }
        if (const auto *dictionary = dynamic_cast<const parser::dictionary_expression *>(&value))
        {
          const auto [key, mapped] = dictionary_components(expression_type(value), value.range);
          const std::size_t current_dictionary_index = dictionary_index++;
          const std::string temporary = "sagan_dictionary_" + std::to_string(current_dictionary_index);
          std::string result = "([&]() { std::unordered_map<" + type_name(key, value.range) + ", " +
                               type_name(mapped, value.range) + "> " + temporary + "; ";
          for (std::size_t index_value = 0; index_value < dictionary->entries.size(); ++index_value)
          {
            const auto &entry = dictionary->entries[index_value];
            if (!entry.key)
            {
              const auto *spread = dynamic_cast<const parser::spread_expression *>(entry.value.get());
              if (!spread) fail("dictionary spread entry is malformed", entry.value->range);
              const std::string spread_temporary = "sagan_dictionary_spread_" +
                                                   std::to_string(current_dictionary_index) + "_" +
                                                   std::to_string(index_value);
              result += "const auto &" + spread_temporary + " = " + expression(*spread->value) + "; ";
              const auto [source_key, source_mapped] =
                  dictionary_components(expression_type(*spread->value), spread->range);
              result += "for (const auto &[sagan_key, sagan_value] : " + spread_temporary + ") " + temporary +
                        ".insert_or_assign(" + convert_value("sagan_key", source_key, key, spread->range) + ", " +
                        convert_value("sagan_value", source_mapped, mapped, spread->range) + "); ";
            }
            else
            {
              result += temporary + ".insert_or_assign(" + converted_expression(*entry.key, key) + ", " +
                        converted_expression(*entry.value, mapped) + "); ";
            }
          }
          return result + "return " + temporary + "; }())";
        }
        if (const auto *lambda = dynamic_cast<const parser::lambda_expression *>(&value))
        {
          std::string result = "[=](";
          open_boxed_scope();
          for (std::size_t index = 0; index < lambda->parameters.size(); ++index)
          {
            if (index != 0) result += ", ";
            const auto &parameter = lambda->parameters[index];
            result += type(parameter.type_name, value.range) + " " + identifier(parameter.name) + "_value";
            box(parameter.name);
          }
          const std::string result_type = lambda->return_type.value_or(expression_type(*lambda->body));
          result += ") -> " + type_name(result_type, value.range) + " { ";
          for (const auto &parameter : lambda->parameters)
            result += "auto " + identifier(parameter.name) + " = std::make_shared<" +
                      type(parameter.type_name, value.range) + ">(" + identifier(parameter.name) + "_value); ";
          result += "return " + converted_expression(*lambda->body, result_type) + "; }";
          close_boxed_scope();
          return result;
        }
        fail("expression is not available in the initial native subset", value.range);
        return {};
      }

      auto block(const parser::block_statement &value) -> void
      {
        output << "{\n";
        ++depth;
        open_boxed_scope();
        for (const auto &entry : value.statements) statement(*entry);
        close_boxed_scope();
        --depth;
        output << indentation() << '}';
      }

      auto statement(const parser::statement &value) -> void
      {
        map_scope mapped(*this, value.range, true);
        if (map_enabled)
          output << indentation() << "sagan_source_scope sagan_site_" << temporary_index++
                 << '(' << escaped_string(active_source_utf8())
                 << ", " << value.range.begin << ", " << value.range.end << ");\n";
        output << indentation();
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          const std::string checked_type = declaration_type(*declaration);
          std::string initializer;
          if (declaration->initializer)
          {
            if (declaration->type_name)
              initializer = converted_expression(*declaration->initializer, checked_type);
            else initializer = expression(*declaration->initializer);
          }
          if (dynamic_cast<const parser::const_declaration *>(declaration))
          {
            unbox(declaration->name);
            output << "const " << type_name(checked_type, declaration->range) << ' '
                   << identifier(declaration->name) << " = " << initializer << ";\n";
          }
          else
          {
            box(declaration->name);
            output << "auto " << identifier(declaration->name) << " = std::make_shared<"
                   << type_name(checked_type, declaration->range) << ">(";
            if (!initializer.empty()) output << initializer;
            output << ");\n";
          }
        }
        else if (const auto *declaration = dynamic_cast<const parser::parallel_let_declaration *>(&value))
        {
          std::vector<std::string> snapshots;
          for (std::size_t index = 0; index < declaration->bindings.size(); ++index)
          {
            const auto &entry = declaration->bindings[index];
            const std::string snapshot = "sagan_parallel_let_" + std::to_string(temporary_index++);
            if (index != 0) output << indentation();
            output << "auto " << snapshot << " = "
                   << converted_expression(*entry.initializer, declaration_type(entry.name_range, entry.name))
                   << ";\n";
            snapshots.push_back(snapshot);
          }
          for (std::size_t index = 0; index < declaration->bindings.size(); ++index)
          {
            const auto &entry = declaration->bindings[index];
            const std::string checked_type = declaration_type(entry.name_range, entry.name);
            box(entry.name);
            output << indentation() << "auto " << identifier(entry.name) << " = std::make_shared<"
                   << type_name(checked_type, entry.name_range) << ">(" << snapshots[index] << ");\n";
          }
        }
        else if (const auto *expression_value = dynamic_cast<const parser::expression_statement *>(&value))
          output << expression(*expression_value->value) << ";\n";
        else if (const auto *assignment = dynamic_cast<const parser::assignment_statement *>(&value))
        {
          if (assignment->operation == "^=")
            output << "sagan_power_assign<" << type_name(expression_type(*assignment->target), assignment->range)
                   << ">(" << expression(*assignment->target) << ", " << expression(*assignment->value) << ");\n";
          else if (assignment->operation != "=")
          {
            const std::string operation_name = assignment->operation == "+=" ? "add" :
                                               assignment->operation == "-=" ? "subtract" :
                                               assignment->operation == "*=" ? "multiply" :
                                               assignment->operation == "/=" ? "divide" : "modulo";
            output << "sagan_" << operation_name << "_assign<"
                   << type_name(expression_type(*assignment->target), assignment->range) << '>' << '('
                   << expression(*assignment->target) << ", "
                   << ((assignment->operation == "+=" || assignment->operation == "-=")
                         ? converted_expression(*assignment->value, expression_type(*assignment->target))
                         : expression(*assignment->value)) << ");\n";
          }
          else
            output << assignable(*assignment->target) << ' ' << assignment->operation << ' '
                   << converted_expression(*assignment->value, expression_type(*assignment->target)) << ";\n";
        }
        else if (const auto *parallel = dynamic_cast<const parser::parallel_assignment_statement *>(&value))
        {
          std::vector<std::string> snapshots;
          for (std::size_t index = 0; index < parallel->values.size(); ++index)
          {
            std::string snapshot = "sagan_parallel_" + std::to_string(temporary_index++);
            output << "auto " << snapshot << " = "
                   << converted_expression(*parallel->values[index], expression_type(*parallel->targets[index]))
                   << ";\n" << indentation();
            snapshots.push_back(std::move(snapshot));
          }
          for (std::size_t index = 0; index < parallel->targets.size(); ++index)
          {
            output << assignable(*parallel->targets[index]) << " = " << snapshots[index] << ";\n";
            if (index + 1 < parallel->targets.size()) output << indentation();
          }
        }
        else if (const auto *conditional = dynamic_cast<const parser::if_statement *>(&value))
        {
          output << "if (" << expression(*conditional->condition) << ") ";
          block(*conditional->then_branch);
          if (conditional->else_branch)
          {
            output << " else ";
            if (const auto *else_block = dynamic_cast<const parser::block_statement *>(conditional->else_branch.get()))
              block(*else_block);
            else
            {
              output << (map_enabled ? "{\n" : "\n");
              ++depth;
              statement(*conditional->else_branch);
              --depth;
              if (map_enabled) output << indentation() << '}';
            }
          }
          output << "\n";
        }
        else if (const auto *loop = dynamic_cast<const parser::condition_loop_statement *>(&value))
        {
          output << "while (";
          if (loop->loop_kind == parser::condition_loop_statement::kind::until_loop) output << '!';
          output << expression(*loop->condition) << ") ";
          block(*loop->body);
          output << "\n";
        }
        else if (const auto *loop = dynamic_cast<const parser::for_statement *>(&value))
        {
          output << "for (auto " << identifier(loop->binding) << "_value : " << expression(*loop->iterable)
                 << ") {\n";
          ++depth;
          open_boxed_scope();
          box(loop->binding);
          output << indentation() << "auto " << identifier(loop->binding)
                 << " = std::make_shared<decltype(" << identifier(loop->binding) << "_value)>("
                 << identifier(loop->binding) << "_value);\n";
          for (const auto &entry : loop->body->statements) statement(*entry);
          close_boxed_scope();
          --depth;
          output << indentation() << "}\n";
        }
        else if (const auto *control = dynamic_cast<const parser::loop_control_statement *>(&value))
          output << (control->control_kind == parser::loop_control_statement::kind::break_loop ? "break;\n" :
                                                                                              "continue;\n");
        else if (const auto *returned = dynamic_cast<const parser::return_statement *>(&value))
        {
          output << "return";
          if (returned->value)
            output << ' ' << (active_return_types.empty()
                                 ? expression(*returned->value)
                                 : converted_expression(*returned->value, active_return_types.back()));
          output << ";\n";
        }
        else if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          const std::string temporary = "sagan_match_" + std::to_string(temporary_index++);
          const std::string matched_type = expression_type(*matched->subject);
          std::unordered_set<std::string> covered_cases;
          bool explicit_fallback = false;
          for (const auto &branch : matched->cases)
          {
            explicit_fallback |= !branch.pattern;
            if (!branch.pattern) continue;
            if (const auto *call = dynamic_cast<const parser::call_expression *>(branch.pattern.get()))
              if (const auto *callee = dynamic_cast<const parser::identifier_expression *>(call->callee.get()))
                if (enum_cases.contains(callee->name)) covered_cases.insert(callee->name);
            if (const auto *member = dynamic_cast<const parser::member_expression *>(branch.pattern.get()))
              covered_cases.insert(member->member_name);
          }
          const std::size_t matched_open = matched_type.find('<');
          const std::string matched_base = matched_open == std::string::npos
                                               ? matched_type : matched_type.substr(0, matched_open);
          const auto matched_enum = enum_declarations.find(matched_base);
          const bool exhaustive_enum = matched_enum != enum_declarations.end() &&
              covered_cases.size() == matched_enum->second->enum_members.size();
          output << "const auto " << temporary << " = " << expression(*matched->subject) << ";\n";
          bool emitted_condition = false;
          for (const auto &branch : matched->cases)
          {
            output << indentation();
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
              const bool some_pattern = callee && callee->name == "Some" && payload;
              const bool none_pattern = name && name->name == "None";
              const auto enum_case = callee ? enum_cases.find(callee->name) : enum_cases.end();
              const bool enum_pattern = enum_case != enum_cases.end();
              output << (emitted_condition ? "else if" : "if") << " (";
              if (some_pattern) output << temporary << ".has_value()";
              else if (none_pattern) output << '!' << temporary << ".has_value()";
              else if (enum_pattern)
                output << temporary << ".tag == " << identifier(enum_case->second.first) << "::Tag::"
                       << identifier(callee->name);
              else output << temporary << " == " << converted_expression(*branch.pattern, matched_type);
              output << ") ";
              emitted_condition = true;
              if (some_pattern)
              {
                output << "{\n";
                ++depth;
                open_boxed_scope();
                box(payload->name);
                output << indentation() << "auto " << identifier(payload->name) << " = std::make_shared<"
                       << type_name(binding_type(*payload), payload->range) << ">(*" << temporary << ");\n";
                for (const auto &entry : branch.body->statements) statement(*entry);
                close_boxed_scope();
                --depth;
                output << indentation() << "}";
                output << "\n";
                continue;
              }
              if (enum_pattern)
              {
                output << "{\n";
                ++depth;
                open_boxed_scope();
                const std::size_t generic_open = matched_type.find('<');
                const std::string matched_base = generic_open == std::string::npos
                                                     ? matched_type : matched_type.substr(0, generic_open);
                const bool generic_payload = enum_declarations.contains(matched_base) &&
                    !enum_declarations.at(matched_base)->type_parameters.empty();
                for (std::size_t index = 0; index < call->arguments.size(); ++index)
                {
                  const auto &binding = dynamic_cast<const parser::identifier_expression &>(*call->arguments[index]);
                  box(binding.name);
                  output << indentation() << "auto " << identifier(binding.name) << " = std::make_shared<"
                         << type_name(binding_type(binding), binding.range) << ">(";
                  if (generic_payload)
                    output << "std::any_cast<" << type_name(binding_type(binding), binding.range) << ">(" << temporary
                           << ".payload.at(" << index << "))";
                  else if (call->arguments.size() == 1)
                    output << "std::get<" << enum_case->second.second << ">(" << temporary << ".payload)";
                  else
                    output << "std::get<" << index << ">(std::get<" << enum_case->second.second << ">(" << temporary
                           << ".payload))";
                  output << ");\n";
                }
                for (const auto &entry : branch.body->statements) statement(*entry);
                close_boxed_scope();
                --depth;
                output << indentation() << "}\n";
                continue;
              }
            }
            else output << (emitted_condition ? "else " : "if (true) ");
            block(*branch.body);
            output << "\n";
          }
          if (exhaustive_enum && !explicit_fallback)
            output << indentation() << "throw std::logic_error(\"unreachable exhaustive match\");\n";
        }
        else if (const auto *hope = dynamic_cast<const parser::hope_statement *>(&value))
        {
          const std::size_t exception_index = temporary_index++;
          const std::string caught = "sagan_caught_" + std::to_string(exception_index);
          output << "{\n";
          ++depth;
          if (hope->cleanup)
          {
            output << indentation() << "auto sagan_finally_" << exception_index
                   << " = sagan_make_finally([&]() ";
            block(*hope->cleanup);
            output << ");\n";
          }
          output << indentation();
          if (!hope->handlers.empty()) output << "try ";
          block(*hope->protected_body);
          if (!hope->handlers.empty())
          {
            output << " catch (const sagan_exception &" << caught << ")\n" << indentation() << "{\n";
            ++depth;
            for (std::size_t index = 0; index < hope->handlers.size(); ++index)
            {
              const auto &handler = hope->handlers[index];
              output << indentation() << (index == 0 ? "if" : "else if")
                     << " (sagan_exception_matches<"
                     << type_name(expression_type(*handler.pattern), handler.pattern->range) << ">(" << caught
                     << ", " << expression(*handler.pattern) << ")) ";
              block(*handler.body);
              output << "\n";
            }
            output << indentation() << "else throw;\n";
            --depth;
            output << indentation() << '}';
          }
          output << "\n";
          --depth;
          output << indentation() << "}\n";
        }
        else if (const auto *scream = dynamic_cast<const parser::scream_statement *>(&value))
        {
          output << "sagan_scream<" << type_name(expression_type(*scream->value), scream->value->range)
                 << ">(" << expression(*scream->value) << ");\n";
        }
        else fail("statement is not available in the initial native subset", value.range);
      }

      auto function(const parser::function_declaration &value) -> void
      {
        map_scope mapped(*this, value.range, false, identifier(value.name));
        const auto previous_type_parameters = active_type_parameters;
        active_type_parameters.insert(value.type_parameters.begin(), value.type_parameters.end());
        if (!value.type_parameters.empty())
        {
          output << "template <";
          for (std::size_t index = 0; index < value.type_parameters.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "typename " << identifier(value.type_parameters[index]);
          }
          output << ">\n";
        }
        output << type(value.return_type, value.range) << ' ' << identifier(value.name) << '(';
        for (std::size_t index = 0; index < value.parameters.size(); ++index)
        {
          if (index != 0) output << ", ";
          output << type(value.parameters[index].type_name, value.range) << ' '
                 << identifier(value.parameters[index].name) << "_value";
        }
        output << ") {\n";
        ++depth;
        if (map_enabled)
          output << indentation() << "sagan_call_scope sagan_call_" << temporary_index++
                 << '(' << escaped_string(value.name) << ", " << escaped_string(active_source_utf8())
                 << ", " << value.range.begin << ", " << value.range.end << ");\n";
        open_boxed_scope();
        for (const auto &parameter : value.parameters)
        {
          box(parameter.name);
          output << indentation() << "auto " << identifier(parameter.name) << " = std::make_shared<"
                 << type(parameter.type_name, value.range) << ">(" << identifier(parameter.name) << "_value);\n";
        }
        active_return_types.push_back(value.return_type.value_or("Void"));
        if (value.expression_body)
        {
          map_scope expression_map(*this, value.expression_body->range, true);
          if (map_enabled)
            output << indentation() << "sagan_source_scope sagan_site_" << temporary_index++
                   << '(' << escaped_string(active_source_utf8())
                   << ", " << value.expression_body->range.begin << ", "
                   << value.expression_body->range.end << ");\n";
          output << indentation() << "return "
                 << converted_expression(*value.expression_body, active_return_types.back()) << ";\n";
          close_boxed_scope();
          --depth;
          output << "}\n\n";
          active_return_types.pop_back();
          active_type_parameters = previous_type_parameters;
          return;
        }
        if (!value.body) fail("function has no executable body", value.range);
        for (const auto &entry_statement : value.body->statements) statement(*entry_statement);
        close_boxed_scope();
        --depth;
        output << "}\n\n";
        active_return_types.pop_back();
        active_type_parameters = previous_type_parameters;
      }

      auto method(const parser::function_declaration &value, const std::string_view class_name = {},
                  const bool virtual_method = false) -> void
      {
        map_scope mapped(*this, value.range, false,
                         std::string(class_name) + "::" + identifier(value.name));
        const auto previous_type_parameters = active_type_parameters;
        active_type_parameters.insert(value.type_parameters.begin(), value.type_parameters.end());
        if (!value.type_parameters.empty())
        {
          output << indentation() << "template <";
          for (std::size_t index = 0; index < value.type_parameters.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "typename " << identifier(value.type_parameters[index]);
          }
          output << ">\n";
        }
        output << indentation();
        if (virtual_method) output << "virtual ";
        if (value.constructor_member)
          output << identifier(std::string(class_name));
        else
          output << type(value.return_type, value.range) << ' ' << identifier(value.name);
        output << '(';
        for (std::size_t index = 0; index < value.parameters.size(); ++index)
        {
          if (index != 0) output << ", ";
          output << type(value.parameters[index].type_name, value.range) << ' '
                 << identifier(value.parameters[index].name) << "_value";
        }
        output << ") ";
        const bool previous_method = in_method;
        in_method = true;
        if (!value.expression_body && !value.body)
        {
          if (virtual_method)
          {
            output << "= 0;\n";
            in_method = previous_method;
            active_type_parameters = previous_type_parameters;
            return;
          }
          fail("method has no executable body", value.range);
        }
        active_return_types.push_back(value.return_type.value_or("Void"));
        output << "{\n";
        ++depth;
        if (map_enabled)
          output << indentation() << "sagan_call_scope sagan_call_" << temporary_index++
                 << '(' << escaped_string(std::string(class_name) + "::" + value.name)
                 << ", " << escaped_string(active_source_utf8())
                 << ", " << value.range.begin << ", " << value.range.end << ");\n";
        open_boxed_scope();
        for (const auto &parameter : value.parameters)
        {
          box(parameter.name);
          output << indentation() << "auto " << identifier(parameter.name) << " = std::make_shared<"
                 << type(parameter.type_name, value.range) << ">(" << identifier(parameter.name) << "_value);\n";
        }
        if (value.expression_body)
        {
          map_scope expression_map(*this, value.expression_body->range, true);
          if (map_enabled)
            output << indentation() << "sagan_source_scope sagan_site_" << temporary_index++
                   << '(' << escaped_string(active_source_utf8())
                   << ", " << value.expression_body->range.begin << ", "
                   << value.expression_body->range.end << ");\n";
          output << indentation() << "return "
                 << converted_expression(*value.expression_body, active_return_types.back()) << ";\n";
        }
        else
        {
          for (const auto &entry_statement : value.body->statements) statement(*entry_statement);
        }
        close_boxed_scope();
        --depth;
        output << indentation() << '}';
        active_return_types.pop_back();
        in_method = previous_method;
        active_type_parameters = previous_type_parameters;
        output << "\n";
      }

      auto method_signature(const parser::function_declaration &value) const -> std::string
      {
        std::string result = value.name + "(";
        for (const auto &parameter : value.parameters)
          result += parameter.type_name.value_or("Unknown") + ";";
        return result + "):" + value.return_type.value_or("Unknown");
      }

      auto face_defaults(const std::string &name,
                         std::unordered_set<std::string> &visiting) const
          -> std::vector<const parser::function_declaration *>
      {
        if (!visiting.insert(name).second) return {};
        const auto found = face_types.find(name.substr(0, name.find('<')));
        if (found == face_types.end()) return {};
        std::vector<const parser::function_declaration *> result;
        for (const auto &parent : found->second->composed_interfaces)
        {
          auto inherited = face_defaults(parent, visiting);
          result.insert(result.end(), inherited.begin(), inherited.end());
        }
        for (const auto &entry : found->second->members)
        {
          const auto *method_value = dynamic_cast<const parser::function_declaration *>(entry.get());
          if (!method_value) continue;
          const std::string signature_value = method_signature(*method_value);
          std::erase_if(result, [&](const parser::function_declaration *existing)
          {
            return method_signature(*existing) == signature_value;
          });
          if (method_value->body || method_value->expression_body) result.push_back(method_value);
        }
        visiting.erase(name);
        return result;
      }

      auto object(const parser::type_declaration &value) -> void
      {
        if (value.type_kind != parser::type_declaration::kind::class_type)
          fail("only class declarations are available in the initial native object subset", value.range);
        const auto previous_type_parameters = active_type_parameters;
        active_type_parameters.insert(value.type_parameters.begin(), value.type_parameters.end());
        if (!value.type_parameters.empty())
        {
          output << "template <";
          for (std::size_t index = 0; index < value.type_parameters.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "typename " << identifier(value.type_parameters[index]);
          }
          output << ">\n";
        }
        output << "struct " << identifier(value.name);
        if (!value.composed_interfaces.empty())
        {
          output << " : ";
          for (std::size_t index = 0; index < value.composed_interfaces.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "public virtual " << concrete_user_type(value.composed_interfaces[index], value.range);
          }
        }
        output << "\n{\npublic:\n";
        ++depth;
        bool private_access = false;
        std::unordered_set<std::string> emitted_methods;
        for (const auto &entry : value.members)
        {
          if (const auto *field = dynamic_cast<const parser::let_declaration *>(entry.get()))
          {
            if (private_access != field->private_member)
            {
              private_access = field->private_member;
              output << (private_access ? "private:\n" : "public:\n");
            }
            output << indentation();
            if (dynamic_cast<const parser::const_declaration *>(field)) output << "const ";
            if (field->weak_member)
              output << "std::weak_ptr<" << identifier(*field->type_name) << "> ";
            else output << type(field->type_name, field->range) << ' ';
            output << identifier(field->name);
            if (field->initializer)
              output << " = " << converted_expression(*field->initializer, declaration_type(*field));
            else output << "{}";
            output << ";\n";
          }
          else if (const auto *member_method = dynamic_cast<const parser::function_declaration *>(entry.get()))
          {
            if (member_method->constructor_member)
            {
              if (private_access)
              {
                output << "public:\n";
                private_access = false;
              }
              method(*member_method, value.name);
              continue;
            }
            emitted_methods.insert(method_signature(*member_method));
            if (private_access != member_method->private_member)
            {
              private_access = member_method->private_member;
              output << (private_access ? "private:\n" : "public:\n");
            }
            method(*member_method);
          }
          else fail("class member is not available in the initial native object subset", entry->range);
        }
        if (private_access) output << "public:\n";
        for (const auto &face_name : value.composed_interfaces)
        {
          const auto face = face_types.find(face_name.substr(0, face_name.find('<')));
          if (face != face_types.end() && !face->second->type_parameters.empty()) continue;
          std::unordered_set<std::string> visiting;
          for (const auto *default_method : face_defaults(face_name, visiting))
          {
            if (emitted_methods.insert(method_signature(*default_method)).second) method(*default_method);
          }
        }
        --depth;
        output << "};\n\n";
        active_type_parameters = previous_type_parameters;
      }

      auto interface(const parser::type_declaration &value) -> void
      {
        if (!emitted_faces.insert(value.name).second) return;
        for (const auto &parent : value.composed_interfaces)
        {
          const auto found = face_types.find(parent.substr(0, parent.find('<')));
          if (found != face_types.end()) interface(*found->second);
        }
        const auto previous_type_parameters = active_type_parameters;
        active_type_parameters.insert(value.type_parameters.begin(), value.type_parameters.end());
        if (!value.type_parameters.empty())
        {
          output << "template <";
          for (std::size_t index = 0; index < value.type_parameters.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "typename " << identifier(value.type_parameters[index]);
          }
          output << ">\n";
        }
        output << "struct " << identifier(value.name);
        if (!value.composed_interfaces.empty())
        {
          output << " : ";
          for (std::size_t index = 0; index < value.composed_interfaces.size(); ++index)
          {
            if (index != 0) output << ", ";
            output << "public virtual " << concrete_user_type(value.composed_interfaces[index], value.range);
          }
        }
        output << "\n{\n  virtual ~" << identifier(value.name) << "() = default;\n";
        ++depth;
        for (const auto &entry : value.members)
        {
          const auto *face_method = dynamic_cast<const parser::function_declaration *>(entry.get());
          if (!face_method) fail("face member is not a method", entry->range);
          method(*face_method, {}, true);
        }
        --depth;
        output << "};\n\n";
        active_type_parameters = previous_type_parameters;
      }

      auto enumeration(const parser::type_declaration &value) -> void
      {
        if (!value.type_parameters.empty())
        {
          output << "struct " << identifier(value.name) << "\n{\n  enum class Tag : std::int64_t\n  {\n";
          for (std::size_t index = 0; index < value.enum_members.size(); ++index)
            output << "    " << identifier(value.enum_members[index].name)
                   << enum_numeric_suffix(value.enum_members[index])
                   << (index + 1 == value.enum_members.size() ? "\n" : ",\n");
          output << "  };\n  Tag tag;\n  std::vector<std::any> payload;\n};\n\n";
          output << "std::ostream &operator<<(std::ostream &stream, const " << identifier(value.name)
                 << " &value)\n{\n  switch (value.tag)\n  {\n";
          for (const auto &member : value.enum_members)
            output << "    case " << identifier(value.name) << "::Tag::" << identifier(member.name)
                   << ": return stream << " << escaped_string(member.name) << ";\n";
          output << "  }\n  return stream;\n}\n\n";
          return;
        }
        const bool payload_enum = std::any_of(value.enum_members.begin(), value.enum_members.end(), [](const auto &member)
        {
          return !member.payload_types.empty();
        });
        if (payload_enum)
        {
          output << "struct " << identifier(value.name) << "\n{\n  enum class Tag : std::int64_t\n  {\n";
          for (std::size_t index = 0; index < value.enum_members.size(); ++index)
            output << "    " << identifier(value.enum_members[index].name)
                   << enum_numeric_suffix(value.enum_members[index])
                   << (index + 1 == value.enum_members.size() ? "\n" : ",\n");
          output << "  };\n  using Payload = std::variant<";
          for (std::size_t index = 0; index < value.enum_members.size(); ++index)
          {
            if (index != 0) output << ", ";
            const auto &payload = value.enum_members[index].payload_types;
            if (payload.empty()) output << "std::monostate";
            else if (payload.size() == 1) output << type_name(payload.front(), value.enum_members[index].range);
            else
            {
              output << "std::tuple<";
              for (std::size_t payload_index = 0; payload_index < payload.size(); ++payload_index)
              {
                if (payload_index != 0) output << ", ";
                output << type_name(payload[payload_index], value.enum_members[index].range);
              }
              output << ">";
            }
          }
          output << ">;\n  Tag tag;\n  Payload payload;\n  friend bool operator==(const " << identifier(value.name)
                 << " &, const " << identifier(value.name) << " &) = default;\n};\n\n";
          for (std::size_t index = 0; index < value.enum_members.size(); ++index)
          {
            const auto &member = value.enum_members[index];
            output << identifier(value.name) << ' ' << enum_factory(value.name, member.name) << '(';
            for (std::size_t payload_index = 0; payload_index < member.payload_types.size(); ++payload_index)
            {
              if (payload_index != 0) output << ", ";
              output << type_name(member.payload_types[payload_index], member.range) << " sagan_payload_"
                     << payload_index;
            }
            output << ")\n{\n  return {" << identifier(value.name) << "::Tag::" << identifier(member.name)
                   << ", " << identifier(value.name) << "::Payload{std::in_place_index<" << index << ">";
            for (std::size_t payload_index = 0; payload_index < member.payload_types.size(); ++payload_index)
              output << ", sagan_payload_" << payload_index;
            output << "}};\n}\n\n";
          }
          output << "std::ostream &operator<<(std::ostream &stream, const " << identifier(value.name)
                 << " &value)\n{\n  switch (value.tag)\n  {\n";
          for (const auto &member : value.enum_members)
            output << "    case " << identifier(value.name) << "::Tag::" << identifier(member.name)
                   << ": return stream << " << escaped_string(member.name) << ";\n";
          output << "  }\n  return stream;\n}\n\n";
          return;
        }
        output << "enum class " << identifier(value.name) << " : std::int64_t\n{\n";
        ++depth;
        for (std::size_t index = 0; index < value.enum_members.size(); ++index)
        {
          output << indentation() << identifier(value.enum_members[index].name);
          output << enum_numeric_suffix(value.enum_members[index]);
          output << (index + 1 == value.enum_members.size() ? "\n" : ",\n");
        }
        --depth;
        output << "};\n\n";
        output << "std::ostream &operator<<(std::ostream &stream, const " << identifier(value.name)
               << " value)\n{\n  switch (value)\n  {\n";
        for (const auto &member : value.enum_members)
          output << "    case " << identifier(value.name) << "::" << identifier(member.name)
                 << ": return stream << " << escaped_string(member.name) << ";\n";
        output << "  }\n  return stream;\n}\n\n";
      }

    public:
      explicit cpp_generator(const semantic::type_model &checked_types,
                             const bool collect_map = false,
                             std::optional<std::filesystem::path> source_path = {},
                             std::optional<std::string> test_name = {})
          : types(checked_types), map_enabled(collect_map), default_source(std::move(source_path)),
            selected_test(std::move(test_name)) {}

      auto take_mappings() -> std::vector<source_map_entry>
      {
        std::stable_sort(mappings.begin(), mappings.end(), [](const auto &left, const auto &right)
        {
          if (left.generated_begin != right.generated_begin)
            return left.generated_begin < right.generated_begin;
          return left.generated_end < right.generated_end;
        });
        return std::move(mappings);
      }

      auto generate(const parser::program &tree) -> std::string
      {
        unit_registry.add_program(tree);
        for (const auto &entry : tree.statements)
          if (const auto *function = dynamic_cast<const parser::function_declaration *>(entry.get()))
            functions.insert_or_assign(function->name, function);
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get()))
          {
            user_types.insert(type->name);
            if (type->type_kind == parser::type_declaration::kind::enum_type)
            {
              enum_types.insert(type->name);
              enum_declarations.emplace(type->name, type);
              for (std::size_t index = 0; index < type->enum_members.size(); ++index)
              {
                const auto &member = type->enum_members[index];
                enum_cases.emplace(member.name, std::pair{type->name, index});
                if (!member.payload_types.empty()) payload_enums.insert(type->name);
              }
            }
            if (type->type_kind == parser::type_declaration::kind::class_type)
            {
              class_types.insert(type->name);
            }
            if (type->type_kind == parser::type_declaration::kind::interface_type)
              face_types.emplace(type->name, type);
            if (type->type_kind == parser::type_declaration::kind::class_type)
              for (const auto &member : type->members)
                if (const auto *field = dynamic_cast<const parser::let_declaration *>(member.get());
                    field && field->weak_member)
                  weak_fields[type->name].insert(field->name);
          }
        output << "// Generated by Sagan.\n#include <cstdlib>\n";
        output << "#include <any>\n#include <array>\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n#include <functional>\n#include <iostream>\n#include <limits>\n#include <memory>\n#include <optional>\n#include <sstream>\n#include <stdexcept>\n#include <string>\n#include <tuple>\n#include <typeindex>\n#include <type_traits>\n#include <unordered_map>\n#include <utility>\n#include <variant>\n#include <vector>\n"
                  "#ifdef _WIN32\n"
                  "#include <windows.h>\n"
                  "#endif\n\n"
                  "void sagan_initialize_runtime()\n"
                  "{\n"
                  "#ifdef _WIN32\n"
                  "  SetConsoleCP(CP_UTF8);\n"
                  "  SetConsoleOutputCP(CP_UTF8);\n"
                  "#endif\n"
                  "}\n\n"
                  "enum class sagan_runtime_error : std::int64_t\n"
                  "{\n"
                  "  integer_overflow,\n"
                  "  division_by_zero,\n"
                  "  modulo_by_zero,\n"
                  "  undefined_exponentiation,\n"
                  "  negative_integer_exponent,\n"
                  "  index_out_of_bounds,\n"
                  "  invalid_range,\n"
                  "  invalid_conversion,\n"
                  "  missing_key,\n"
                  "  uninitialized_binding,\n"
                  "  math_domain,\n"
                  "  non_finite,\n"
                  "  zero_length\n"
                  "};\n\n";
        if (map_enabled)
          output << "struct sagan_source_site { const char *path; int begin; int end; };\n"
                    "thread_local sagan_source_site sagan_active_site{\"\", -1, -1};\n"
                    "struct sagan_source_scope\n"
                    "{\n"
                    "  sagan_source_site previous;\n"
                    "  sagan_source_scope(const char *path, int begin, int end)\n"
                    "      : previous(sagan_active_site) { sagan_active_site = {path, begin, end}; }\n"
                    "  ~sagan_source_scope() { sagan_active_site = previous; }\n"
                    "};\n\n"
                    "struct sagan_call_frame { const char *name; const char *path; int begin; int end; };\n"
                    "thread_local std::vector<sagan_call_frame> sagan_call_stack;\n"
                    "struct sagan_call_scope\n"
                    "{\n"
                    "  sagan_call_scope(const char *name, const char *path, int begin, int end)\n"
                    "  { sagan_call_stack.push_back({name, path, begin, end}); }\n"
                    "  ~sagan_call_scope() { sagan_call_stack.pop_back(); }\n"
                    "};\n\n";
        output << "struct sagan_exception final : std::exception\n"
                  "{\n"
                  "  std::any value;\n"
                  "  std::type_index type;\n"
                  "  std::string message;\n";
        if (map_enabled) output << "  sagan_source_site site;\n  std::vector<sagan_call_frame> calls;\n";
        output << "  sagan_exception(std::any thrown, const std::type_index thrown_type, std::string text)\n"
                  "      : value(std::move(thrown)), type(thrown_type), message(std::move(text))";
        if (map_enabled) output << ", site(sagan_active_site), calls(sagan_call_stack)";
        output << " {}\n"
                  "  const char *what() const noexcept override { return message.c_str(); }\n"
                  "};\n\n"
                  "const char *sagan_exception_code(const sagan_exception &error)\n"
                  "{\n"
                  "  if (error.type != std::type_index(typeid(sagan_runtime_error))) return \"SAG-RUN-0100\";\n"
                  "  switch (std::any_cast<sagan_runtime_error>(error.value))\n"
                  "  {\n"
                  "  case sagan_runtime_error::integer_overflow: return \"SAG-RUN-0101\";\n"
                  "  case sagan_runtime_error::division_by_zero: return \"SAG-RUN-0102\";\n"
                  "  case sagan_runtime_error::modulo_by_zero: return \"SAG-RUN-0103\";\n"
                  "  case sagan_runtime_error::undefined_exponentiation: return \"SAG-RUN-0104\";\n"
                  "  case sagan_runtime_error::negative_integer_exponent: return \"SAG-RUN-0105\";\n"
                  "  case sagan_runtime_error::index_out_of_bounds: return \"SAG-RUN-0106\";\n"
                  "  case sagan_runtime_error::invalid_range: return \"SAG-RUN-0107\";\n"
                  "  case sagan_runtime_error::invalid_conversion: return \"SAG-RUN-0108\";\n"
                  "  case sagan_runtime_error::missing_key: return \"SAG-RUN-0109\";\n"
                  "  case sagan_runtime_error::uninitialized_binding: return \"SAG-RUN-0110\";\n"
                  "  case sagan_runtime_error::math_domain: return \"SAG-RUN-0111\";\n"
                  "  case sagan_runtime_error::non_finite: return \"SAG-RUN-0112\";\n"
                  "  case sagan_runtime_error::zero_length: return \"SAG-RUN-0113\";\n"
                  "  }\n"
                  "  return \"SAG-RUN-0001\";\n"
                  "}\n\n"
                  "template <typename T>\n"
                  "[[noreturn]] void sagan_scream(T value)\n"
                  "{\n"
                  "  const std::string message = [&]() -> std::string\n"
                  "  {\n"
                  "    if constexpr (std::is_same_v<T, std::string>) return value;\n"
                  "    return \"Uncaught Sagan exception\";\n"
                  "  }();\n"
                  "  throw sagan_exception{std::move(value), std::type_index(typeid(T)), message};\n"
                  "}\n\n"
                  "[[noreturn]] void sagan_runtime_failure(const sagan_runtime_error error, std::string message)\n"
                  "{ throw sagan_exception{error, std::type_index(typeid(sagan_runtime_error)), std::move(message)}; }\n\n"
                  "template <typename T>\n"
                  "T &sagan_box_value(const std::shared_ptr<T> &value)\n"
                  "{\n"
                  "  if (!value) sagan_runtime_failure(sagan_runtime_error::uninitialized_binding, \"Sagan binding was used before initialization\");\n"
                  "  return *value;\n"
                  "}\n\n"
                  "template <typename Float>\n"
                  "std::int64_t sagan_round_int(const Float value)\n"
                  "{\n"
                  "  static_assert(std::is_floating_point_v<Float>);\n"
                  "  if (!std::isfinite(value))\n"
                  "    sagan_runtime_failure(sagan_runtime_error::invalid_conversion, \"Int.round requires a finite Float\");\n"
                  "  const long double rounded = std::round(static_cast<long double>(value));\n"
                  "  const long double lower = static_cast<long double>(std::numeric_limits<std::int64_t>::min());\n"
                  "  if (rounded < lower || rounded >= -lower)\n"
                  "    sagan_runtime_failure(sagan_runtime_error::invalid_conversion, \"Int.round result is outside Int64\");\n"
                  "  return static_cast<std::int64_t>(rounded);\n"
                  "}\n\n"
                  "std::vector<std::int64_t> sagan_times(const std::int64_t count)\n"
                  "{\n"
                  "  if (count < 0) sagan_runtime_failure(sagan_runtime_error::invalid_range, \"times requires a non-negative integer\");\n"
                  "  std::vector<std::int64_t> values;\n"
                  "  for (std::int64_t index = 0; index < count; ++index) values.push_back(index);\n"
                  "  return values;\n"
                  "}\n\n"
                  "template <typename T>\n"
                  "bool sagan_exception_matches(const sagan_exception &error, const T &pattern)\n"
                  "{\n"
                  "  return error.type == std::type_index(typeid(T)) && std::any_cast<const T &>(error.value) == pattern;\n"
                  "}\n\n"
                  "template <typename Callable>\n"
                  "class sagan_finally_guard final\n"
                  "{\n"
                  "  Callable action;\n"
                  "public:\n"
                  "  explicit sagan_finally_guard(Callable cleanup) : action(std::move(cleanup)) {}\n"
                  "  sagan_finally_guard(const sagan_finally_guard &) = delete;\n"
                  "  sagan_finally_guard &operator=(const sagan_finally_guard &) = delete;\n"
                  "  ~sagan_finally_guard() noexcept(noexcept(action())) { action(); }\n"
                  "};\n\n"
                  "template <typename Callable>\n"
                  "auto sagan_make_finally(Callable action)\n"
                  "{ return sagan_finally_guard<Callable>(std::move(action)); }\n\n"
                  "template <typename T>\n"
                  "std::optional<std::shared_ptr<T>> sagan_lock_weak(const std::weak_ptr<T> &value)\n"
                  "{\n"
                  "  auto locked = value.lock();\n"
                  "  if (!locked) return std::nullopt;\n"
                  "  return locked;\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "struct sagan_vector\n"
                  "{\n"
                  "  std::array<T, Size> components;\n"
                  "  T &at(const std::size_t index) { return components.at(index); }\n"
                  "  const T &at(const std::size_t index) const { return components.at(index); }\n"
                  "  auto begin() { return components.begin(); }\n"
                  "  auto end() { return components.end(); }\n"
                  "  auto begin() const { return components.begin(); }\n"
                  "  auto end() const { return components.end(); }\n"
                  "  constexpr std::size_t size() const { return Size; }\n"
                  "  bool operator==(const sagan_vector &) const = default;\n"
                  "};\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "struct sagan_point\n"
                  "{\n"
                  "  std::array<T, Size> components;\n"
                  "  T &at(const std::size_t index) { return components.at(index); }\n"
                  "  const T &at(const std::size_t index) const { return components.at(index); }\n"
                  "  auto begin() { return components.begin(); }\n"
                  "  auto end() { return components.end(); }\n"
                  "  auto begin() const { return components.begin(); }\n"
                  "  auto end() const { return components.end(); }\n"
                  "  constexpr std::size_t size() const { return Size; }\n"
                  "  bool operator==(const sagan_point &) const = default;\n"
                  "};\n\n"
                  "template <typename Float>\n"
                  "Float sagan_math_sqrt(const Float value)\n"
                  "{\n"
                  "  if (!std::isfinite(value))\n"
                  "    sagan_runtime_failure(sagan_runtime_error::non_finite, \"sqrt requires a finite value\");\n"
                  "  if (value < Float{0})\n"
                  "    sagan_runtime_failure(sagan_runtime_error::math_domain, \"sqrt requires a non-negative value\");\n"
                  "  return std::sqrt(value);\n"
                  "}\n\n"
                  "template <typename Result, typename Vector>\n"
                  "Result sagan_math_squared_length(const Vector &value)\n"
                  "{\n"
                  "  Result result{};\n"
                  "  for (const auto component : value.components)\n"
                  "  {\n"
                  "    if (!std::isfinite(component))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"squared_length requires finite components\");\n"
                  "    result += static_cast<Result>(component) * static_cast<Result>(component);\n"
                  "    if (!std::isfinite(result))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"squared_length produced a non-finite result\");\n"
                  "  }\n"
                  "  return result;\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_math_dot(const Left &left, const Right &right)\n"
                  "{\n"
                  "  Result result{};\n"
                  "  for (std::size_t index = 0; index < left.size(); ++index)\n"
                  "  {\n"
                  "    if (!std::isfinite(left.components[index]) || !std::isfinite(right.components[index]))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"dot requires finite components\");\n"
                  "    result += static_cast<Result>(left.components[index]) * static_cast<Result>(right.components[index]);\n"
                  "    if (!std::isfinite(result))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"dot produced a non-finite result\");\n"
                  "  }\n"
                  "  return result;\n"
                  "}\n\n"
                  "template <typename Result, typename Vector>\n"
                  "Result sagan_math_length(const Vector &value)\n"
                  "{\n"
                  "  return sagan_math_sqrt(static_cast<Result>(sagan_math_squared_length<Result>(value)));\n"
                  "}\n\n"
                  "template <typename Result, typename Vector>\n"
                  "Result sagan_math_normalized(const Vector &value)\n"
                  "{\n"
                  "  Result result{};\n"
                  "  using Float = typename decltype(result.components)::value_type;\n"
                  "  const Float magnitude = sagan_math_length<Float>(value);\n"
                  "  if (magnitude == Float{0})\n"
                  "    sagan_runtime_failure(sagan_runtime_error::zero_length, \"normalized cannot normalize a zero-length Vector\");\n"
                  "  for (std::size_t index = 0; index < result.size(); ++index)\n"
                  "  {\n"
                  "    result.components[index] = static_cast<Float>(value.components[index]) / magnitude;\n"
                  "    if (!std::isfinite(result.components[index]))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"normalized produced a non-finite component\");\n"
                  "  }\n"
                  "  return result;\n"
                  "}\n\n"
                  "template <typename Result, typename Point, typename Scale>\n"
                  "Result sagan_math_display_coordinates(const Point &point, const Point &origin, const Scale scale)\n"
                  "{\n"
                  "  if (!std::isfinite(scale))\n"
                  "    sagan_runtime_failure(sagan_runtime_error::non_finite, \"display_coordinates requires a finite scale\");\n"
                  "  if (scale == Scale{0})\n"
                  "    sagan_runtime_failure(sagan_runtime_error::division_by_zero, \"display_coordinates scale cannot be zero\");\n"
                  "  Result result{};\n"
                  "  for (std::size_t index = 0; index < result.size(); ++index)\n"
                  "  {\n"
                  "    if (!std::isfinite(point.components[index]) || !std::isfinite(origin.components[index]))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"display_coordinates requires finite Points\");\n"
                  "    result.components[index] = static_cast<typename decltype(result.components)::value_type>(\n"
                  "        (point.components[index] - origin.components[index]) / scale);\n"
                  "    if (!std::isfinite(result.components[index]))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::non_finite, \"display_coordinates produced a non-finite component\");\n"
                  "  }\n"
                  "  return result;\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "struct sagan_spherical_vector\n"
                  "{\n"
                  "  std::array<T, Size> components;\n"
                  "  T &at(const std::size_t index) { return components.at(index); }\n"
                  "  const T &at(const std::size_t index) const { return components.at(index); }\n"
                  "  auto begin() { return components.begin(); }\n"
                  "  auto end() { return components.end(); }\n"
                  "  auto begin() const { return components.begin(); }\n"
                  "  auto end() const { return components.end(); }\n"
                  "  constexpr std::size_t size() const { return Size; }\n"
                  "  bool operator==(const sagan_spherical_vector &) const = default;\n"
                  "};\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "struct sagan_spherical_point\n"
                  "{\n"
                  "  std::array<T, Size> components;\n"
                  "  T &at(const std::size_t index) { return components.at(index); }\n"
                  "  const T &at(const std::size_t index) const { return components.at(index); }\n"
                  "  auto begin() { return components.begin(); }\n"
                  "  auto end() { return components.end(); }\n"
                  "  auto begin() const { return components.begin(); }\n"
                  "  auto end() const { return components.end(); }\n"
                  "  constexpr std::size_t size() const { return Size; }\n"
                  "  bool operator==(const sagan_spherical_point &) const = default;\n"
                  "};\n\n"
                  "template <typename Collection, typename Index>\n"
                  "decltype(auto) sagan_index(Collection &&collection, const Index index)\n"
                  "{\n"
                  "  if constexpr (std::is_signed_v<Index>)\n"
                  "    if (index < 0) sagan_runtime_failure(sagan_runtime_error::index_out_of_bounds, \"Sagan index out of bounds\");\n"
                  "  const auto converted = static_cast<std::size_t>(index);\n"
                  "  if (converted >= collection.size())\n"
                  "    sagan_runtime_failure(sagan_runtime_error::index_out_of_bounds, \"Sagan index out of bounds\");\n"
                  "  return collection.at(converted);\n"
                  "}\n\n"
                  "template <typename Dictionary, typename Key>\n"
                  "decltype(auto) sagan_dictionary_at(Dictionary &&dictionary, const Key &key)\n"
                  "{\n"
                  "  const auto found = dictionary.find(key);\n"
                  "  if (found == dictionary.end())\n"
                  "    sagan_runtime_failure(sagan_runtime_error::missing_key, \"Sagan dictionary key does not exist\");\n"
                  "  return (found->second);\n"
                  "}\n\n"
                  "template <typename T> struct sagan_vector_traits;\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_vector_traits<sagan_vector<Component, Size>>\n"
                  "{ using component = Component; static constexpr std::size_t size = Size; };\n"
                  "template <typename T> struct sagan_is_vector : std::false_type {};\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_is_vector<sagan_vector<Component, Size>> : std::true_type {};\n"
                  "template <typename T>\n"
                  "inline constexpr bool sagan_is_vector_v = sagan_is_vector<std::remove_cvref_t<T>>::value;\n\n"
                  "template <typename T> struct sagan_point_traits;\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_point_traits<sagan_point<Component, Size>>\n"
                  "{ using component = Component; static constexpr std::size_t size = Size; };\n"
                  "template <typename T> struct sagan_is_point : std::false_type {};\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_is_point<sagan_point<Component, Size>> : std::true_type {};\n"
                  "template <typename T>\n"
                  "inline constexpr bool sagan_is_point_v = sagan_is_point<std::remove_cvref_t<T>>::value;\n\n"
                  "template <typename T>\n"
                  "void sagan_stream_component(std::ostream &stream, const T value)\n"
                  "{\n"
                  "  if constexpr (std::is_same_v<T, std::int8_t>) stream << static_cast<int>(value);\n"
                  "  else stream << value;\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "std::ostream &operator<<(std::ostream &stream, const sagan_vector<T, Size> &value)\n"
                  "{\n"
                  "  stream << '<';\n"
                  "  for (std::size_t index = 0; index < Size; ++index)\n"
                  "  {\n"
                  "    if (index != 0) stream << \", \";\n"
                  "    sagan_stream_component(stream, value.components[index]);\n"
                  "  }\n"
                  "  return stream << '>';\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "std::ostream &operator<<(std::ostream &stream, const sagan_point<T, Size> &value)\n"
                  "{\n"
                  "  stream << '(';\n"
                  "  for (std::size_t index = 0; index < Size; ++index)\n"
                  "  {\n"
                  "    if (index != 0) stream << \", \";\n"
                  "    sagan_stream_component(stream, value.components[index]);\n"
                  "  }\n"
                  "  return stream << ')';\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "std::ostream &operator<<(std::ostream &stream, const sagan_spherical_vector<T, Size> &value)\n"
                  "{\n"
                  "  stream << \"s<\";\n"
                  "  for (std::size_t index = 0; index < Size; ++index)\n"
                  "  {\n"
                  "    if (index != 0) stream << \", \";\n"
                  "    sagan_stream_component(stream, value.components[index]);\n"
                  "  }\n"
                  "  return stream << '>';\n"
                  "}\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "std::ostream &operator<<(std::ostream &stream, const sagan_spherical_point<T, Size> &value)\n"
                  "{\n"
                  "  stream << \"s(\";\n"
                  "  for (std::size_t index = 0; index < Size; ++index)\n"
                  "  {\n"
                  "    if (index != 0) stream << \", \";\n"
                  "    sagan_stream_component(stream, value.components[index]);\n"
                  "  }\n"
                  "  return stream << ')';\n"
                  "}\n\n"
                  "template <typename T>\n"
                  "struct sagan_displayed_unit { T value; const char *unit; };\n\n"
                  "template <typename T>\n"
                  "sagan_displayed_unit<T> sagan_display_unit(T value, const char *unit)\n"
                  "{ return {std::move(value), unit}; }\n\n"
                  "template <typename T>\n"
                  "std::ostream &operator<<(std::ostream &stream, const sagan_displayed_unit<T> &value)\n"
                  "{ sagan_stream_component(stream, value.value); return stream << ' ' << value.unit; }\n\n"
                  "template <typename T>\n"
                  "void sagan_print(const T &value)\n"
                  "{\n"
                  "  std::cout << std::boolalpha << value << '\\n';\n"
                  "}\n\n"
                  "void sagan_print(const std::int8_t value)\n"
                  "{\n"
                  "  std::cout << static_cast<int>(value) << '\\n';\n"
                  "}\n\n"
                  "struct sagan_assertion_failure final : std::exception\n"
                  "{\n"
                  "  std::string message;\n";
        if (map_enabled) output << "  sagan_source_site site;\n  std::vector<sagan_call_frame> calls;\n";
        output << "  explicit sagan_assertion_failure(std::string text) : message(std::move(text))";
        if (map_enabled) output << ", site(sagan_active_site), calls(sagan_call_stack)";
        output << " {}\n"
                  "  const char *what() const noexcept override { return message.c_str(); }\n"
                  "};\n\n"
                  "void sagan_assert(const bool condition, const std::string &message = \"Assertion failed\")\n"
                  "{\n"
                  "  if (!condition) throw sagan_assertion_failure(message);\n"
                  "}\n\n"
                  "struct sagan_exit_signal { int code; };\n\n"
                  "[[noreturn]] void sagan_exit(const std::int64_t code)\n"
                  "{\n"
                  "  if (code < 0 || code > 255)\n"
                  "    sagan_runtime_failure(sagan_runtime_error::invalid_conversion, \"Exit status must be between 0 and 255\");\n"
                  "  throw sagan_exit_signal{static_cast<int>(code)};\n"
                  "}\n\n"
                  "template <typename T>\n"
                  "std::string sagan_stringify(const T &value)\n"
                  "{\n"
                  "  std::ostringstream stream;\n"
                  "  stream << std::boolalpha << value;\n"
                  "  return stream.str();\n"
                  "}\n\n"
                  "std::string sagan_stringify(const std::int8_t value)\n"
                  "{\n"
                  "  return std::to_string(static_cast<int>(value));\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_add(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  if constexpr (sagan_is_vector_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_vector_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_add<Component>(left_value.components[index], right_value.components[index]);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else if constexpr (sagan_is_point_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_point_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_add<Component>(left_value.components[index], right_value.components[index]);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    constexpr Result minimum = std::numeric_limits<Result>::min();\n"
                  "    constexpr Result maximum = std::numeric_limits<Result>::max();\n"
                  "    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer addition overflow (\" + sagan_stringify(left) + \" + \" + sagan_stringify(right) + \" cannot fit)\");\n"
                  "  }\n"
                  "  return static_cast<Result>(left + right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_subtract(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  if constexpr (sagan_is_vector_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_vector_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_subtract<Component>(left_value.components[index], right_value.components[index]);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else if constexpr (sagan_is_point_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_point_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_subtract<Component>(left_value.components[index], right_value.components[index]);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    constexpr Result minimum = std::numeric_limits<Result>::min();\n"
                  "    constexpr Result maximum = std::numeric_limits<Result>::max();\n"
                  "    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right))\n"
                  "      sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer subtraction overflow\");\n"
                  "  }\n"
                  "  return static_cast<Result>(left - right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_multiply(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  if constexpr (sagan_is_vector_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_vector_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "    {\n"
                  "      if constexpr (sagan_is_vector_v<Left>)\n"
                  "        result.components[index] = sagan_multiply<Component>(left_value.components[index], right_value);\n"
                  "      else result.components[index] = sagan_multiply<Component>(left_value, right_value.components[index]);\n"
                  "    }\n"
                  "    return result;\n"
                  "  }\n"
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if constexpr (!std::is_integral_v<Result>) return static_cast<Result>(left * right);\n"
                  "  constexpr Result minimum = std::numeric_limits<Result>::min();\n"
                  "  constexpr Result maximum = std::numeric_limits<Result>::max();\n"
                  "  const bool overflow =\n"
                  "      left > 0 ? (right > 0 ? left > maximum / right : right < minimum / left)\n"
                  "               : (left < 0 ? (right > 0 ? left < minimum / right\n"
                  "                                      : right < maximum / left)\n"
                  "                           : false);\n"
                  "  if (overflow) sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer multiplication overflow\");\n"
                  "  return static_cast<Result>(left * right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result>\n"
                  "Result sagan_power_multiply(const Result left, const Result right)\n"
                  "{\n"
                  "  try { return sagan_multiply<Result>(left, right); }\n"
                  "  catch (const sagan_exception &)\n"
                  "  { sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer exponentiation overflow\"); }\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_divide(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  if constexpr (sagan_is_vector_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_vector_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_divide<Component>(left_value.components[index], right_value);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if (right == Result{0}) sagan_runtime_failure(sagan_runtime_error::division_by_zero, \"Sagan division by zero\");\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "    if (left == std::numeric_limits<Result>::min() && right == Result{-1})\n"
                  "      sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer division overflow\");\n"
                  "  return static_cast<Result>(left / right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_modulo(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if (right == Result{0}) sagan_runtime_failure(sagan_runtime_error::modulo_by_zero, \"Sagan modulo by zero\");\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    if (left == std::numeric_limits<Result>::min() && right == Result{-1}) return Result{0};\n"
                  "    return static_cast<Result>(left % right);\n"
                  "  }\n"
                  "  return static_cast<Result>(std::fmod(left, right));\n"
                  "}\n\n"
                  "template <typename Result, typename Value>\n"
                  "Result sagan_negate(const Value value_input)\n"
                  "{\n"
                  "  if constexpr (sagan_is_vector_v<Result>)\n"
                  "  {\n"
                  "    using Traits = sagan_vector_traits<Result>;\n"
                  "    using Component = typename Traits::component;\n"
                  "    Result result{};\n"
                  "    for (std::size_t index = 0; index < Traits::size; ++index)\n"
                  "      result.components[index] = sagan_negate<Component>(value_input.components[index]);\n"
                  "    return result;\n"
                  "  }\n"
                  "  else\n"
                  "  {\n"
                  "  const Result value = static_cast<Result>(value_input);\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "    if (value == std::numeric_limits<Result>::min())\n"
                  "      sagan_runtime_failure(sagan_runtime_error::integer_overflow, \"Sagan integer negation overflow\");\n"
                  "  return static_cast<Result>(-value);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result, typename Target>\n"
                  "Result sagan_increment(Target &target, const bool postfix)\n"
                  "{\n"
                  "  const Result previous = static_cast<Result>(target);\n"
                  "  target = sagan_add<Result>(target, Result{1});\n"
                  "  return postfix ? previous : static_cast<Result>(target);\n"
                  "}\n\n"
                  "template <typename Result, typename Target>\n"
                  "Result sagan_decrement(Target &target, const bool postfix)\n"
                  "{\n"
                  "  const Result previous = static_cast<Result>(target);\n"
                  "  target = sagan_subtract<Result>(target, Result{1});\n"
                  "  return postfix ? previous : static_cast<Result>(target);\n"
                  "}\n\n"
                  "template <typename Result, typename Base, typename Exponent>\n"
                  "Result sagan_power(const Base base_value, const Exponent exponent_value)\n"
                  "{\n"
                  "  const Result base = static_cast<Result>(base_value);\n"
                  "  if constexpr (std::is_floating_point_v<Result> && std::is_integral_v<Exponent>)\n"
                  "  {\n"
                  "    if (base == Result{0} && exponent_value == Exponent{0})\n"
                  "      sagan_runtime_failure(sagan_runtime_error::undefined_exponentiation, \"Sagan exponentiation does not define 0 ^ 0\");\n"
                  "    if (base == Result{0} && exponent_value < Exponent{0})\n"
                  "      sagan_runtime_failure(sagan_runtime_error::division_by_zero, \"Sagan zero cannot have a negative exponent\");\n"
                  "    using Unsigned = std::make_unsigned_t<Exponent>;\n"
                  "    Unsigned remaining = exponent_value < Exponent{0}\n"
                  "        ? Unsigned{0} - static_cast<Unsigned>(exponent_value)\n"
                  "        : static_cast<Unsigned>(exponent_value);\n"
                  "    Result factor = base;\n"
                  "    Result result = Result{1};\n"
                  "    while (remaining != 0)\n"
                  "    {\n"
                  "      if ((remaining & Unsigned{1}) != 0) result *= factor;\n"
                  "      remaining >>= 1;\n"
                  "      if (remaining != 0) factor *= factor;\n"
                  "    }\n"
                  "    return exponent_value < Exponent{0} ? Result{1} / result : result;\n"
                  "  }\n"
                  "  const Result exponent = static_cast<Result>(exponent_value);\n"
                  "  if (base == Result{0} && exponent == Result{0})\n"
                  "    sagan_runtime_failure(sagan_runtime_error::undefined_exponentiation, \"Sagan exponentiation does not define 0 ^ 0\");\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    if (exponent < Result{0})\n"
                  "      sagan_runtime_failure(sagan_runtime_error::negative_integer_exponent, \"Sagan integer exponentiation requires a non-negative exponent\");\n"
                  "    using Unsigned = std::make_unsigned_t<Result>;\n"
                  "    Unsigned remaining = static_cast<Unsigned>(exponent);\n"
                  "    Result factor = base;\n"
                  "    Result result = Result{1};\n"
                  "    while (remaining != 0)\n"
                  "    {\n"
                  "      if ((remaining & Unsigned{1}) != 0) result = sagan_power_multiply(result, factor);\n"
                  "      remaining >>= 1;\n"
                  "      if (remaining != 0) factor = sagan_power_multiply(factor, factor);\n"
                  "    }\n"
                  "    return result;\n"
                  "  }\n"
                  "  return static_cast<Result>(std::pow(base, exponent));\n"
                  "}\n\n"
                  "template <typename Result, typename Target, typename Exponent>\n"
                  "void sagan_power_assign(Target &target, const Exponent exponent)\n"
                  "{\n"
                  "  target = sagan_power<Result>(target, exponent);\n"
                  "}\n\n"
                  "template <typename Result, typename Target, typename Value>\n"
                  "void sagan_add_assign(Target &target, const Value value)\n"
                  "{ target = sagan_add<Result>(target, value); }\n\n"
                  "template <typename Result, typename Target, typename Value>\n"
                  "void sagan_subtract_assign(Target &target, const Value value)\n"
                  "{ target = sagan_subtract<Result>(target, value); }\n\n"
                  "template <typename Result, typename Target, typename Value>\n"
                  "void sagan_multiply_assign(Target &target, const Value value)\n"
                  "{ target = sagan_multiply<Result>(target, value); }\n\n"
                  "template <typename Result, typename Target, typename Value>\n"
                  "void sagan_divide_assign(Target &target, const Value value)\n"
                  "{ target = sagan_divide<Result>(target, value); }\n\n"
                  "template <typename Result, typename Target, typename Value>\n"
                  "void sagan_modulo_assign(Target &target, const Value value)\n"
                  "{ target = sagan_modulo<Result>(target, value); }\n\n";
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
              type && type->type_kind != parser::type_declaration::kind::enum_type)
          {
            if (!type->type_parameters.empty())
            {
              output << "template <";
              for (std::size_t index = 0; index < type->type_parameters.size(); ++index)
              {
                if (index != 0) output << ", ";
                output << "typename " << identifier(type->type_parameters[index]);
              }
              output << "> ";
            }
            output << "struct " << identifier(type->name) << ";\n";
          }
        output << '\n';
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
              type && type->type_kind == parser::type_declaration::kind::enum_type)
          {
            source_of(*entry);
            enumeration(*type);
          }
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get());
              type && type->type_kind == parser::type_declaration::kind::interface_type)
          {
            source_of(*entry);
            interface(*type);
          }
        open_boxed_scope();
        for (const auto &entry : tree.statements)
        {
          if (const auto *binding = dynamic_cast<const parser::let_declaration *>(entry.get()))
          {
            box(binding->name);
            output << "std::shared_ptr<" << type_name(declaration_type(*binding), binding->range)
                   << "> " << identifier(binding->name) << ";\n";
          }
          else if (const auto *group = dynamic_cast<const parser::parallel_let_declaration *>(entry.get()))
            for (const auto &binding : group->bindings)
            {
              box(binding.name);
              output << "std::shared_ptr<" << type_name(declaration_type(binding.name_range, binding.name),
                                                        binding.name_range)
                     << "> " << identifier(binding.name) << ";\n";
            }
        }
        output << '\n';
        for (const auto &entry : tree.statements)
        {
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get()))
          {
            source_of(*entry);
            if (type->type_kind == parser::type_declaration::kind::class_type) object(*type);
          }
        }
        output << '\n';
        for (const auto &entry : tree.statements)
        {
          const auto *declaration = dynamic_cast<const parser::function_declaration *>(entry.get());
          if (!declaration)
          {
            if (dynamic_cast<const parser::type_declaration *>(entry.get()) ||
                dynamic_cast<const parser::measurement_declaration *>(entry.get()) ||
                dynamic_cast<const parser::const_declaration *>(entry.get())) continue;
            continue;
          }
          source_of(*entry);
          function(*declaration);
        }
        output << "\nint main() {\n";
        ++depth;
        output << indentation() << "sagan_initialize_runtime();\n";
        output << indentation() << "try {\n";
        ++depth;
        const std::string previous_function = active_function;
        active_function = "main";
        for (const auto &entry : tree.statements)
        {
          if (dynamic_cast<const parser::function_declaration *>(entry.get()) ||
              dynamic_cast<const parser::type_declaration *>(entry.get()) ||
              dynamic_cast<const parser::measurement_declaration *>(entry.get())) continue;
          if (selected_test && !dynamic_cast<const parser::let_declaration *>(entry.get()) &&
              !dynamic_cast<const parser::parallel_let_declaration *>(entry.get())) continue;
          source_of(*entry);
          if (const auto *binding = dynamic_cast<const parser::let_declaration *>(entry.get()))
          {
            output << indentation() << identifier(binding->name) << " = std::make_shared<"
                   << type_name(declaration_type(*binding), binding->range) << ">(";
            if (binding->initializer)
              output << converted_expression(*binding->initializer, declaration_type(*binding));
            output << ");\n";
          }
          else if (const auto *group = dynamic_cast<const parser::parallel_let_declaration *>(entry.get()))
          {
            std::vector<std::string> snapshots;
            for (const auto &binding : group->bindings)
            {
              const std::string snapshot = "sagan_root_let_" + std::to_string(temporary_index++);
              output << indentation() << "auto " << snapshot << " = "
                     << converted_expression(*binding.initializer,
                                             declaration_type(binding.name_range, binding.name)) << ";\n";
              snapshots.push_back(snapshot);
            }
            for (std::size_t index = 0; index < group->bindings.size(); ++index)
            {
              const auto &binding = group->bindings[index];
              output << indentation() << identifier(binding.name) << " = std::make_shared<"
                     << type_name(declaration_type(binding.name_range, binding.name), binding.name_range)
                     << ">(" << snapshots[index] << ");\n";
            }
          }
          else statement(*entry);
        }
        if (selected_test)
          output << indentation() << identifier(*selected_test) << "();\n";
        active_function = previous_function;
        output << indentation() << "return 0;\n";
        --depth;
        output << indentation() << "} catch (const sagan_exit_signal &requested) {\n"
               << indentation() << "  return requested.code;\n" << indentation() << "}\n";
        output << indentation() << "catch (const sagan_assertion_failure &error) {\n";
        if (map_enabled)
        {
          output << indentation() << "  std::cerr << \"SAGAN_RUNTIME_ERROR\\t\""
                 << " << error.site.begin << '\\t' << error.site.end"
                 << " << '\\t' << error.site.path";
          output << " << \"\\tSAG-RUN-0200\\t\" << error.what() << '\\n';\n";
        }
        else
          output << indentation() << "  std::cerr << \"error[SAG-RUN-0200]: \""
                 << " << error.what() << '\\n';\n";
        if (map_enabled)
          output << indentation() << "  for (auto frame = error.calls.rbegin(); frame != error.calls.rend(); ++frame)\n"
                 << indentation() << "    std::cerr << \"SAGAN_RUNTIME_FRAME\\t\" << frame->path << '\\t'"
                 << " << frame->begin << '\\t' << frame->end << '\\t' << frame->name << '\\n';\n";
        output << indentation() << "  return 1;\n" << indentation() << "}\n";
        if (map_enabled)
        {
          output << indentation() << "catch (const sagan_exception &error) {\n"
                 << indentation() << "  std::cerr << \"SAGAN_RUNTIME_ERROR\\t\" << error.site.begin"
                 << " << '\\t' << error.site.end << '\\t' << error.site.path"
                 << " << '\\t' << sagan_exception_code(error) << '\\t' << error.what() << '\\n';\n"
                 << indentation() << "  for (auto frame = error.calls.rbegin(); frame != error.calls.rend(); ++frame)\n"
                 << indentation() << "    std::cerr << \"SAGAN_RUNTIME_FRAME\\t\" << frame->path << '\\t'"
                 << " << frame->begin << '\\t' << frame->end << '\\t' << frame->name << '\\n';\n"
                 << indentation() << "  return 1;\n" << indentation() << "}\n";
        }
        else
          output << indentation() << "catch (const sagan_exception &error) {\n"
                 << indentation() << "  std::cerr << \"error[\" << sagan_exception_code(error)"
                 << " << \"]: \" << error.what() << '\\n';\n"
                 << indentation() << "  return 1;\n" << indentation() << "}\n";
        output << indentation() << "catch (const std::exception &error) {\n";
        if (map_enabled)
          output << indentation() << "  std::cerr << \"SAGAN_RUNTIME_ERROR\\t0\\t0\\t\\tSAG-RUN-0999\\tNative runtime failure: \"";
        else
          output << indentation() << "  std::cerr << \"error[SAG-RUN-0999]: Native runtime failure: \"";
        output << " << error.what() << '\\n';\n"
               << indentation() << "  return 1;\n" << indentation() << "}\n"
               << indentation() << "catch (...) {\n";
        if (map_enabled)
          output << indentation() << "  std::cerr << \"SAGAN_RUNTIME_ERROR\\t0\\t0\\t\\tSAG-RUN-0998\\tUnknown native runtime failure\\n\";\n";
        else
          output << indentation() << "  std::cerr << \"error[SAG-RUN-0998]: Unknown native runtime failure\\n\";\n";
        output << indentation() << "  return 1;\n" << indentation() << "}\n";
        --depth;
        output << "}\n";
        close_boxed_scope();
        return output.str();
      }
    };
  }

  auto generate_cpp(const parser::program &tree, const semantic::type_model &types) -> std::string
  {
    return cpp_generator(types).generate(tree);
  }

  auto generated_identifier(const std::string_view name) -> std::string
  {
    if (name == "print") return "sagan_print";
    if (name == "assert") return "sagan_assert";
    if (name == "exit") return "sagan_exit";
    std::ostringstream encoded;
    encoded << "sagan_" << std::hex << std::setfill('0');
    for (const unsigned char byte : name) encoded << std::setw(2) << static_cast<unsigned int>(byte);
    return encoded.str();
  }

  auto generate_cpp_mapped(const parser::program &tree, const semantic::type_model &types,
                          std::optional<std::filesystem::path> default_source) -> generated_cpp
  {
    cpp_generator generator(types, true, std::move(default_source));
    auto text = generator.generate(tree);
    return {std::move(text), generator.take_mappings()};
  }

  auto generate_cpp_mapped_test(const parser::program &tree, const semantic::type_model &types,
                               const std::string_view internal_test_name,
                               std::optional<std::filesystem::path> default_source) -> generated_cpp
  {
    const bool found = std::any_of(tree.statements.begin(), tree.statements.end(),
        [&](const auto &entry)
        {
          const auto *function = dynamic_cast<const parser::function_declaration *>(entry.get());
          return function && function->test_name && function->name == internal_test_name;
        });
    if (!found) throw std::invalid_argument("Selected test is not present in the analyzed program");
    cpp_generator generator(types, true, std::move(default_source), std::string(internal_test_name));
    auto text = generator.generate(tree);
    return {std::move(text), generator.take_mappings()};
  }
}
