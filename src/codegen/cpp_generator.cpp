#include "cpp_generator.hpp"

#include "../semantic/semantic_error.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>
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
        if (value == "+" || value == "-" || value == "*" || value == "/" || value == "%" ||
            value == "==" || value == "!=" || value == "<" || value == "<=" || value == ">" ||
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
            const std::string operand = expression(*unary->operand);
            return unary->postfix ? "(" + operand + unary->operator_text + ")"
                                  : "(" + unary->operator_text + operand + ")";
          }
          if (unary->postfix) fail("unsupported postfix operator", value.range);
          const std::string op = unary->operator_text == "not" ? "!" : unary->operator_text;
          if (op != "!" && op != "+" && op != "-") fail("unsupported unary operator", value.range);
          return "(" + op + expression(*unary->operand) + ")";
        }
        if (const auto *binary = dynamic_cast<const parser::binary_expression *>(&value))
          return "(" + expression(*binary->left) + " " + operation(binary->operator_text, value.range) + " " +
                 expression(*binary->right) + ")";
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
        if (const auto *collection = dynamic_cast<const parser::collection_expression *>(&value))
        {
          if (collection->collection_kind != parser::collection_expression::kind::array)
            fail("only array collections are available in the initial native subset", value.range);
          const std::string checked = expression_type(value);
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
          output << expression(*assignment->target) << ' ' << assignment->operation << ' '
                 << expression(*assignment->value) << ";\n";
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

    public:
      explicit cpp_generator(const semantic::type_model &checked_types) : types(checked_types) {}

      auto generate(const parser::program &tree) -> std::string
      {
        output << "// Generated by Sagan.\n#include <cstdint>\n#include <iostream>\n#include <sstream>\n#include <string>\n#include <unordered_map>\n#include <vector>\n"
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
                  "}\n\n";
        for (const auto &entry : tree.statements)
        {
          const auto *declaration = dynamic_cast<const parser::function_declaration *>(entry.get());
          if (!declaration) fail("only functions are supported at the top level", entry->range);
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
