#include "cpp_generator.hpp"

#include "../semantic/semantic_error.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>
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
      bool in_method = false;

      auto indentation() const -> std::string
      {
        return std::string(static_cast<std::size_t>(depth) * 2, ' ');
      }

      auto fail(const std::string &message, const parser::span range) const -> void
      {
        throw semantic::semantic_error("C++ backend: " + message, range);
      }

      auto identifier(const std::string &name) const -> std::string
      {
        if (name == "main") return "main";
        if (name == "print") return "sagan_print";
        if (name == "self" && in_method) return "(*this)";
        std::ostringstream encoded;
        encoded << "sagan_" << std::hex << std::setfill('0');
        for (const unsigned char byte : name) encoded << std::setw(2) << static_cast<unsigned int>(byte);
        return encoded.str();
      }

      auto type_name(const std::string &name, const parser::span range,
                     const bool entry = false) const -> std::string
      {
        if (entry && name == "Int") return "int";
        if (entry && name == "Void") return "int";
        if (name == "Void") return "void";
        if (name == "Bool") return "bool";
        if (name == "String") return "std::string";
        if (name == "Float" || name == "Float64") return "double";
        if (name == "Float32") return "float";
        if (name == "Int" || name == "Int64") return "std::int64_t";
        if (name == "Int32") return "std::int32_t";
        if (name == "Int16") return "std::int16_t";
        if (name == "Int8") return "std::int8_t";
        if (user_types.contains(name)) return identifier(name);
        for (const std::string_view family : {std::string_view{"Vector"}, std::string_view{"Coordinate"}})
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
          const std::string component = name.substr(open + 1, name.size() - open - 2);
          const std::string runtime_family = family == "Vector" ? "sagan_vector" : "sagan_coordinate";
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
          return identifier(name->name);
        if (const auto *literal = dynamic_cast<const parser::literal_expression *>(&value))
        {
          std::string spelling = literal->spelling;
          spelling.erase(std::remove(spelling.begin(), spelling.end(), '_'), spelling.end());
          return spelling;
        }
        if (const auto *string = dynamic_cast<const parser::string_expression *>(&value))
        {
          std::string result = "std::string{}";
          for (const auto &part : string->parts)
          {
            result += part.interpolation
                          ? " + sagan_stringify(" + expression(*part.interpolation) + ")"
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
              return "static_cast<" + type_name(expression_type(value), value.range) + ">(-" +
                     expression(*literal) + ")";
            return "sagan_negate<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*unary->operand) + ")";
          }
          if (op == "+" && std::string_view{expression_type(value)}.starts_with("Vector"))
            return expression(*unary->operand);
          return "(" + op + expression(*unary->operand) + ")";
        }
        if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
        {
          if (binary->operator_text == "^")
            return "sagan_power<" + type_name(expression_type(value), value.range) + ">(" +
                   expression(*binary->left) + ", " + expression(*binary->right) + ")";
          const std::string checked_type = type_name(expression_type(value), value.range);
          if (binary->operator_text == "+")
            return "sagan_add<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "-")
            return "sagan_subtract<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "*")
            return "sagan_multiply<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "/")
            return "sagan_divide<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          if (binary->operator_text == "%")
            return "sagan_modulo<" + checked_type + ">(" + expression(*binary->left) + ", " +
                   expression(*binary->right) + ")";
          return "(" + expression(*binary->left) + " " + operation(binary->operator_text, value.range) + " " +
                 expression(*binary->right) + ")";
        }
        if (const auto *conditional = dynamic_cast<const parser::conditional_expression *>(&value))
          return "(" + expression(*conditional->condition) + " ? " + expression(*conditional->when_true) +
                 " : " + expression(*conditional->when_false) + ")";
        if (const auto *assignment = dynamic_cast<const parser::assignment_expression *>(&value))
          return "(" + expression(*assignment->target) + " = " + expression(*assignment->value) + ")";
        if (const auto *call = dynamic_cast<const parser::call_expression *>(&value))
        {
          std::string result = expression(*call->callee) + "(";
          for (std::size_t index = 0; index < call->arguments.size(); ++index)
          {
            if (index != 0) result += ", ";
            result += expression(*call->arguments[index]);
          }
          return result + ")";
        }
        if (const auto *index = dynamic_cast<const parser::index_expression *>(&value))
          return expression(*index->target) + ".at(" + expression(*index->index) + ")";
        if (const auto *member = dynamic_cast<const parser::member_expression *>(&value))
        {
          if (member->safe) fail("safe member access is not available in the initial native subset", value.range);
          const std::string target_type = expression_type(*member->target);
          if (std::string_view{target_type}.starts_with("Vector") ||
              std::string_view{target_type}.starts_with("Coordinate"))
          {
            const std::string_view component_names = "xyzw";
            const std::size_t component = component_names.find(member->member_name);
            if (member->member_name.size() != 1 || component == std::string_view::npos)
              fail("dimensioned member access is invalid", value.range);
            return expression(*member->target) + ".at(" + std::to_string(component) + ")";
          }
          return expression(*member->target) + "." + identifier(member->member_name);
        }
        if (const auto *collection = dynamic_cast<const parser::collection_expression *>(&value))
        {
          const std::string checked = expression_type(value);
          if (collection->collection_kind == parser::collection_expression::kind::vector ||
              collection->collection_kind == parser::collection_expression::kind::coordinate)
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
                result += temporary + ".insert(" + temporary + ".end(), " + spread_temporary + ".begin(), " +
                          spread_temporary + ".end()); ";
              }
              else result += temporary + ".push_back(" + expression(*collection->elements[index_value]) + "); ";
            }
            return result + "return " + temporary + "; }())";
          }
          std::string result = "std::vector<" + type_name(element, value.range) + ">{";
          for (std::size_t index_value = 0; index_value < collection->elements.size(); ++index_value)
          {
            if (index_value != 0) result += ", ";
            result += expression(*collection->elements[index_value]);
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
              result += "for (const auto &[sagan_key, sagan_value] : " + spread_temporary + ") " + temporary +
                        ".insert_or_assign(sagan_key, sagan_value); ";
            }
            else
            {
              result += temporary + ".insert_or_assign(" + expression(*entry.key) + ", " +
                        expression(*entry.value) + "); ";
            }
          }
          return result + "return " + temporary + "; }())";
        }
        if (const auto *lambda = dynamic_cast<const parser::lambda_expression *>(&value))
        {
          std::string result = "[&](";
          for (std::size_t index = 0; index < lambda->parameters.size(); ++index)
          {
            if (index != 0) result += ", ";
            const auto &parameter = lambda->parameters[index];
            result += type(parameter.type_name, value.range) + " " + identifier(parameter.name);
          }
          result += ") -> " + type_name(expression_type(*lambda->body), value.range) + " { return " +
                    expression(*lambda->body) + "; }";
          return result;
        }
        fail("expression is not available in the initial native subset", value.range);
        return {};
      }

      auto block(const parser::block_statement &value) -> void
      {
        output << "{\n";
        ++depth;
        for (const auto &entry : value.statements) statement(*entry);
        --depth;
        output << indentation() << '}';
      }

      auto statement(const parser::statement &value) -> void
      {
        output << indentation();
        if (const auto *declaration = dynamic_cast<const parser::let_declaration *>(&value))
        {
          if (declaration->initializer)
            output << "auto " << identifier(declaration->name) << " = " << expression(*declaration->initializer);
          else
            output << type(declaration->type_name, declaration->range) << ' ' << identifier(declaration->name);
          output << ";\n";
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
                   << expression(*assignment->target) << ", " << expression(*assignment->value) << ");\n";
          }
          else
            output << expression(*assignment->target) << ' ' << assignment->operation << ' '
                   << expression(*assignment->value) << ";\n";
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
              output << "\n";
              ++depth;
              statement(*conditional->else_branch);
              --depth;
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
          output << "for (auto " << identifier(loop->binding) << " : " << expression(*loop->iterable) << ") ";
          block(*loop->body);
          output << "\n";
        }
        else if (const auto *control = dynamic_cast<const parser::loop_control_statement *>(&value))
          output << (control->control_kind == parser::loop_control_statement::kind::break_loop ? "break;\n" :
                                                                                              "continue;\n");
        else if (const auto *returned = dynamic_cast<const parser::return_statement *>(&value))
        {
          output << "return";
          if (returned->value) output << ' ' << expression(*returned->value);
          output << ";\n";
        }
        else if (const auto *matched = dynamic_cast<const parser::match_statement *>(&value))
        {
          const std::string temporary = "sagan_match_" + std::to_string(temporary_index++);
          output << "const auto " << temporary << " = " << expression(*matched->subject) << ";\n";
          bool emitted_condition = false;
          for (const auto &branch : matched->cases)
          {
            output << indentation();
            if (branch.pattern)
            {
              output << (emitted_condition ? "else if" : "if") << " (" << temporary << " == "
                     << expression(*branch.pattern) << ") ";
              emitted_condition = true;
            }
            else output << (emitted_condition ? "else " : "if (true) ");
            block(*branch.body);
            output << "\n";
          }
        }
        else fail("statement is not available in the initial native subset", value.range);
      }

      auto function(const parser::function_declaration &value) -> void
      {
        const bool entry = value.name == "main";
        output << type(value.return_type, value.range, entry) << ' ' << identifier(value.name) << '(';
        for (std::size_t index = 0; index < value.parameters.size(); ++index)
        {
          if (index != 0) output << ", ";
          output << type(value.parameters[index].type_name, value.range) << ' '
                 << identifier(value.parameters[index].name);
        }
        output << ") ";
        if (value.expression_body)
        {
          output << "{ ";
          if (entry) output << "sagan_initialize_runtime(); ";
          output << "return " << expression(*value.expression_body) << "; }\n\n";
          return;
        }
        if (!value.body) fail("function has no executable body", value.range);
        if (entry)
        {
          output << "{\n";
          ++depth;
          output << indentation() << "sagan_initialize_runtime();\n";
          for (const auto &entry_statement : value.body->statements) statement(*entry_statement);
          if (value.return_type == "Void") output << indentation() << "return 0;\n";
          --depth;
          output << indentation() << '}';
        }
        else block(*value.body);
        output << "\n\n";
      }

      auto method(const parser::function_declaration &value) -> void
      {
        output << indentation() << type(value.return_type, value.range) << ' ' << identifier(value.name) << '(';
        for (std::size_t index = 0; index < value.parameters.size(); ++index)
        {
          if (index != 0) output << ", ";
          output << type(value.parameters[index].type_name, value.range) << ' '
                 << identifier(value.parameters[index].name);
        }
        output << ") ";
        const bool previous_method = in_method;
        in_method = true;
        if (value.expression_body)
          output << "{ return " << expression(*value.expression_body) << "; }";
        else
        {
          if (!value.body) fail("method has no executable body", value.range);
          block(*value.body);
        }
        in_method = previous_method;
        output << "\n";
      }

      auto object(const parser::type_declaration &value) -> void
      {
        if (value.type_kind != parser::type_declaration::kind::class_type)
          fail("only class declarations are available in the initial native object subset", value.range);
        output << "struct " << identifier(value.name) << "\n{\n";
        ++depth;
        for (const auto &entry : value.members)
        {
          if (const auto *field = dynamic_cast<const parser::let_declaration *>(entry.get()))
          {
            output << indentation() << type(field->type_name, field->range) << ' ' << identifier(field->name);
            if (field->initializer) output << " = " << expression(*field->initializer);
            else output << "{}";
            output << ";\n";
          }
          else if (const auto *member_method = dynamic_cast<const parser::function_declaration *>(entry.get()))
            method(*member_method);
          else fail("class member is not available in the initial native object subset", entry->range);
        }
        --depth;
        output << "};\n\n";
      }

    public:
      explicit cpp_generator(const semantic::type_model &checked_types) : types(checked_types) {}

      auto generate(const parser::program &tree) -> std::string
      {
        for (const auto &entry : tree.statements)
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get()))
            user_types.insert(type->name);
        output << "// Generated by Sagan.\n#include <array>\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n#include <iostream>\n#include <limits>\n#include <sstream>\n#include <stdexcept>\n#include <string>\n#include <type_traits>\n#include <unordered_map>\n#include <vector>\n"
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
                  "  bool operator==(const sagan_vector &) const = default;\n"
                  "};\n\n"
                  "template <typename T, std::size_t Size>\n"
                  "struct sagan_coordinate\n"
                  "{\n"
                  "  std::array<T, Size> components;\n"
                  "  T &at(const std::size_t index) { return components.at(index); }\n"
                  "  const T &at(const std::size_t index) const { return components.at(index); }\n"
                  "  auto begin() { return components.begin(); }\n"
                  "  auto end() { return components.end(); }\n"
                  "  auto begin() const { return components.begin(); }\n"
                  "  auto end() const { return components.end(); }\n"
                  "  bool operator==(const sagan_coordinate &) const = default;\n"
                  "};\n\n"
                  "template <typename T> struct sagan_vector_traits;\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_vector_traits<sagan_vector<Component, Size>>\n"
                  "{ using component = Component; static constexpr std::size_t size = Size; };\n"
                  "template <typename T> struct sagan_is_vector : std::false_type {};\n"
                  "template <typename Component, std::size_t Size>\n"
                  "struct sagan_is_vector<sagan_vector<Component, Size>> : std::true_type {};\n"
                  "template <typename T>\n"
                  "inline constexpr bool sagan_is_vector_v = sagan_is_vector<std::remove_cvref_t<T>>::value;\n\n"
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
                  "std::ostream &operator<<(std::ostream &stream, const sagan_coordinate<T, Size> &value)\n"
                  "{\n"
                  "  stream << '(';\n"
                  "  for (std::size_t index = 0; index < Size; ++index)\n"
                  "  {\n"
                  "    if (index != 0) stream << \", \";\n"
                  "    sagan_stream_component(stream, value.components[index]);\n"
                  "  }\n"
                  "  return stream << ')';\n"
                  "}\n\n"
                  "template <typename T>\n"
                  "void sagan_print(const T &value)\n"
                  "{\n"
                  "  std::cout << std::boolalpha << value << '\\n';\n"
                  "}\n\n"
                  "void sagan_print(const std::int8_t value)\n"
                  "{\n"
                  "  std::cout << static_cast<int>(value) << '\\n';\n"
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
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    constexpr Result minimum = std::numeric_limits<Result>::min();\n"
                  "    constexpr Result maximum = std::numeric_limits<Result>::max();\n"
                  "    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right))\n"
                  "      throw std::overflow_error(\"Sagan integer addition overflow\");\n"
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
                  "  else\n"
                  "  {\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    constexpr Result minimum = std::numeric_limits<Result>::min();\n"
                  "    constexpr Result maximum = std::numeric_limits<Result>::max();\n"
                  "    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right))\n"
                  "      throw std::overflow_error(\"Sagan integer subtraction overflow\");\n"
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
                  "  if (overflow) throw std::overflow_error(\"Sagan integer multiplication overflow\");\n"
                  "  return static_cast<Result>(left * right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result>\n"
                  "Result sagan_power_multiply(const Result left, const Result right)\n"
                  "{\n"
                  "  try { return sagan_multiply<Result>(left, right); }\n"
                  "  catch (const std::overflow_error &)\n"
                  "  { throw std::overflow_error(\"Sagan integer exponentiation overflow\"); }\n"
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
                  "  if (right == Result{0}) throw std::domain_error(\"Sagan division by zero\");\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "    if (left == std::numeric_limits<Result>::min() && right == Result{-1})\n"
                  "      throw std::overflow_error(\"Sagan integer division overflow\");\n"
                  "  return static_cast<Result>(left / right);\n"
                  "  }\n"
                  "}\n\n"
                  "template <typename Result, typename Left, typename Right>\n"
                  "Result sagan_modulo(const Left left_value, const Right right_value)\n"
                  "{\n"
                  "  const Result left = static_cast<Result>(left_value);\n"
                  "  const Result right = static_cast<Result>(right_value);\n"
                  "  if (right == Result{0}) throw std::domain_error(\"Sagan modulo by zero\");\n"
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
                  "      throw std::overflow_error(\"Sagan integer negation overflow\");\n"
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
                  "  const Result exponent = static_cast<Result>(exponent_value);\n"
                  "  if (base == Result{0} && exponent == Result{0})\n"
                  "    throw std::domain_error(\"Sagan exponentiation does not define 0 ^ 0\");\n"
                  "  if constexpr (std::is_integral_v<Result>)\n"
                  "  {\n"
                  "    if (exponent < Result{0})\n"
                  "      throw std::domain_error(\"Sagan integer exponentiation requires a non-negative exponent\");\n"
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
          if (const auto *type = dynamic_cast<const parser::type_declaration *>(entry.get())) object(*type);
        for (const auto &entry : tree.statements)
        {
          const auto *declaration = dynamic_cast<const parser::function_declaration *>(entry.get());
          if (!declaration)
          {
            if (dynamic_cast<const parser::type_declaration *>(entry.get())) continue;
            fail("only functions and classes are supported at the top level", entry->range);
          }
          function(*declaration);
        }
        return output.str();
      }
    };
  }

  auto generate_cpp(const parser::program &tree, const semantic::type_model &types) -> std::string
  {
    return cpp_generator(types).generate(tree);
  }
}
