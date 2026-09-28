#include "cpp_generator.hpp"

#include "../semantic/semantic_error.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace codegen
{
  namespace
  {
    class cpp_generator
    {
      std::ostringstream output;
      int depth = 0;

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

      auto type(const std::optional<std::string> &name, const parser::span range,
                const bool entry = false) const -> std::string
      {
        if (!name) fail("native functions require explicit parameter and return types", range);
        if (entry && *name == "Int") return "int";
        if (entry && *name == "Void") return "int";
        if (*name == "Void") return "void";
        if (*name == "Bool") return "bool";
        if (*name == "String") return "std::string";
        if (*name == "Float" || *name == "Float64") return "double";
        if (*name == "Float32") return "float";
        if (*name == "Int" || *name == "Int64") return "std::int64_t";
        if (*name == "Int32") return "std::int32_t";
        if (*name == "Int16") return "std::int16_t";
        if (*name == "Int8") return "std::int8_t";
        fail("type '" + *name + "' is not available in the initial native subset", range);
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
          if (string->parts.size() != 1 || string->parts.front().interpolation)
            fail("string interpolation is not available in the initial native subset", value.range);
          return "std::string{" + escaped_string(string->parts.front().text) + "}";
        }
        if (const auto *group = dynamic_cast<const parser::grouping_expression *>(&value))
          return "(" + expression(*group->value) + ")";
        if (const auto *unary = dynamic_cast<const parser::unary_expression *>(&value))
        {
          if (unary->postfix) fail("postfix operators are not available in the initial native subset", value.range);
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
        else if (const auto *control = dynamic_cast<const parser::loop_control_statement *>(&value))
          output << (control->control_kind == parser::loop_control_statement::kind::break_loop ? "break;\n" :
                                                                                              "continue;\n");
        else if (const auto *returned = dynamic_cast<const parser::return_statement *>(&value))
        {
          output << "return";
          if (returned->value) output << ' ' << expression(*returned->value);
          output << ";\n";
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
      auto generate(const parser::program &tree) -> std::string
      {
        output << "// Generated by Sagan.\n#include <cstdint>\n#include <iostream>\n#include <string>\n"
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

  auto generate_cpp(const parser::program &tree) -> std::string
  {
    return cpp_generator().generate(tree);
  }
}
