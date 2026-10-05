#include "parser.hpp"

#include "parse_error.hpp"
#include "naming.hpp"
#include "tokens.hpp"

#include <utility>
#include <unordered_set>

namespace parser
{
  syntax_parser::syntax_parser(std::vector<token> tokens,
                               const sagan::diagnostics::cancellation_token stop)
      : input(std::move(tokens)), cancellation(stop) {}

  auto syntax_parser::at_end() const -> bool
  {
    return current >= input.size();
  }

  auto syntax_parser::peek() const -> const token *
  {
    return at_end() ? nullptr : &input[current];
  }

  auto syntax_parser::previous() const -> const token &
  {
    return input[current - 1];
  }

  auto syntax_parser::advance() -> const token &
  {
    if (cancellation.is_cancelled()) throw parse_cancelled{};
    if (!at_end())
    {
      current++;
    }
    return previous();
  }

  auto syntax_parser::check(const int token_id) const -> bool
  {
    return !at_end() && peek()->id == token_id;
  }

  auto syntax_parser::match(const int token_id) -> bool
  {
    if (!check(token_id))
    {
      return false;
    }
    advance();
    return true;
  }

  auto syntax_parser::expect(const int token_id, const std::string &description) -> const token &
  {
    if (match(token_id))
    {
      return previous();
    }
    // LCOV_EXCL_START - the empty-input arm is unreachable because parse() accepts an empty program.
    const span error_range = at_end()
                                 ? (input.empty() ? span{0, 0}
                                                  : span{input.back().range.end, input.back().range.end})
                                 : peek()->range;
    // LCOV_EXCL_STOP
    const std::string found = at_end() ? "end of file" : "'" + peek()->text + "'";
    throw parse_error("Expected " + description + ", but found " + found, error_range);
  }

  auto syntax_parser::skip_newlines() -> void
  {
    while (match(tokens::NEWLINE))
    {
    }
  }

  auto syntax_parser::match_after_newlines(const int token_id) -> bool
  {
    const std::size_t before_newlines = current;
    skip_newlines();
    if (match(token_id)) return true;
    current = before_newlines;
    return false;
  }

  auto syntax_parser::parse_documentation_comments() -> std::vector<documentation_comment>
  {
    std::vector<documentation_comment> comments;
    while (match(tokens::DOC_COMMENT))
    {
      const token comment = previous();
      comments.emplace_back(comment.range, comment.text);
      expect(tokens::NEWLINE, "a newline after a documentation comment");
      skip_newlines();
    }
    return comments;
  }

  auto syntax_parser::parse() -> program
  {
    if (cancellation.is_cancelled()) throw parse_cancelled{};
    std::vector<statement_ref> body;
    bool seen_module = false;
    bool seen_non_module = false;
    std::unordered_set<std::string> test_names;
    skip_newlines();
    while (!at_end())
    {
      auto documentation = parse_documentation_comments();
      if (!documentation.empty() && at_end())
      {
        throw parse_error("A documentation comment must precede a declaration", documentation.back().range);
      }
      statement_ref declaration;
      if (match(tokens::KWD_LET))
      {
        declaration = parse_let_declaration();
      }
      else if (match(tokens::KWD_CONST))
      {
        declaration = parse_const_declaration();
      }
      else if (match(tokens::KWD_FUN))
      {
        declaration = parse_function_declaration();
      }
      else if (match(tokens::KWD_TEST))
      {
        declaration = parse_test_declaration();
        const auto &test = *dynamic_cast<const function_declaration *>(declaration.get());
        if (!test_names.insert(*test.test_name).second)
          throw parse_error("Duplicate test name '" + *test.test_name + "'", *test.test_name_range);
      }
      else if (match(tokens::KWD_FACE))
      {
        declaration = parse_type_declaration(type_declaration::kind::interface_type);
      }
      else if (match(tokens::KWD_CLASS))
      {
        declaration = parse_type_declaration(type_declaration::kind::class_type);
      }
      else if (match(tokens::KWD_ENUM))
      {
        declaration = parse_type_declaration(type_declaration::kind::enum_type);
      }
      else if (match(tokens::KWD_DIMENSION))
      {
        declaration = parse_measurement_declaration(measurement_declaration::kind::dimension);
      }
      else if (match(tokens::KWD_QUANTITY))
      {
        declaration = parse_measurement_declaration(measurement_declaration::kind::quantity);
      }
      else if (match(tokens::KWD_UNIT))
      {
        declaration = parse_measurement_declaration(measurement_declaration::kind::linear_unit);
      }
      else if (match(tokens::KWD_AFFINE))
      {
        expect(tokens::KWD_UNIT, "'unit' after 'affine'");
        declaration = parse_measurement_declaration(measurement_declaration::kind::affine_unit);
      }
      else if (match(tokens::KWD_MODULE))
      {
        if (seen_module)
        {
          throw parse_error("A source file can declare only one module", previous().range);
        }
        if (seen_non_module)
        {
          throw parse_error("The module declaration must precede every other declaration", previous().range);
        }
        seen_module = true;
        declaration = parse_module_declaration();
      }
      else if (match(tokens::KWD_IMPORT))
      {
        declaration = parse_import_declaration();
      }
      else if (match(tokens::KWD_EXPORT))
      {
        declaration = parse_export_declaration();
      }
      else
      {
        if (!documentation.empty())
          throw parse_error("A documentation comment must precede a declaration", documentation.back().range);
        declaration = parse_statement();
      }
      if (dynamic_cast<module_declaration *>(declaration.get()) == nullptr)
      {
        seen_non_module = true;
      }
      declaration->documentation = std::move(documentation);
      body.push_back(std::move(declaration));
      if (!at_end())
      {
        expect(tokens::NEWLINE, "a newline after the top-level statement");
        skip_newlines();
      }
    }
    return program(std::move(body));
  }

  auto syntax_parser::parse_statement() -> statement_ref
  {
    if (check(tokens::DOC_COMMENT))
    {
      auto documentation = parse_documentation_comments();
      if (!match(tokens::KWD_LET) && !match(tokens::KWD_CONST))
      {
        throw parse_error("A documentation comment inside a block must precede a binding declaration",
                          documentation.back().range);
      }
      auto declaration = previous().id == tokens::KWD_CONST
                             ? parse_const_declaration() : parse_let_declaration();
      declaration->documentation = std::move(documentation);
      return declaration;
    }
    if (match(tokens::KWD_LET))
    {
      return parse_let_declaration();
    }
    if (match(tokens::KWD_CONST))
    {
      return parse_const_declaration();
    }
    if (match(tokens::KWD_IF))
    {
      return parse_if_statement();
    }
    if (match(tokens::KWD_WHILE))
    {
      return parse_condition_loop(condition_loop_statement::kind::while_loop);
    }
    if (match(tokens::KWD_UNTIL))
    {
      return parse_condition_loop(condition_loop_statement::kind::until_loop);
    }
    if (match(tokens::KWD_FOR))
    {
      return parse_for_statement();
    }
    if (match(tokens::KWD_BREAK))
    {
      return parse_loop_control(loop_control_statement::kind::break_loop);
    }
    if (match(tokens::KWD_CONTINUE))
    {
      return parse_loop_control(loop_control_statement::kind::continue_loop);
    }
    if (match(tokens::KWD_RETURN))
    {
      return parse_return_statement();
    }
    if (match(tokens::KWD_YIELD))
    {
      return parse_yield_statement();
    }
    if (match(tokens::KWD_MATCH))
    {
      return parse_match_statement();
    }
    if (match(tokens::KWD_CASE))
    {
      throw parse_error("'case' is only valid inside a match statement", previous().range);
    }
    if (match(tokens::KWD_HOPE))
    {
      return parse_hope_statement();
    }
    if (match(tokens::KWD_SCREAM))
    {
      return parse_scream_statement();
    }
    if (match(tokens::KWD_ELSE))
    {
      throw parse_error("'else' is only valid after an if statement", previous().range);
    }
    if (match(tokens::KWD_UNLESS) || match(tokens::KWD_FINALLY))
    {
      throw parse_error("'" + previous().text + "' is only valid after a hope block", previous().range);
    }
    return parse_expression_statement();
  }

  auto syntax_parser::parse_let_declaration(const bool allow_private, const bool weak_member) -> statement_ref
  {
    const token &keyword = previous();
    const bool private_member = allow_private && match(tokens::DOT);
    const token &name = expect(tokens::IDENTIFIER, "an identifier after 'let'");
    if (is_constant_name(name.text))
      throw parse_error("SCREAMING_SNAKE_CASE names are reserved for const declarations; use 'const " +
                            name.text + "' or rename this mutable variable",
                        name.range);
    std::optional<std::string> type_name;
    if (match(tokens::COLON))
    {
      type_name = parse_type_annotation("a type name after ':'");
    }

    if (match(tokens::COMMA))
    {
      if (allow_private || weak_member)
        throw parse_error("Class fields must be declared one at a time", previous().range);
      std::vector<parallel_let_binding> bindings;
      bindings.push_back(parallel_let_binding{name.range, name.text, std::move(type_name), {}});
      do
      {
        const token &next_name = expect(tokens::IDENTIFIER, "a variable name after ',' in a let declaration");
        if (is_constant_name(next_name.text))
          throw parse_error("SCREAMING_SNAKE_CASE names are reserved for const declarations", next_name.range);
        std::optional<std::string> next_type;
        if (match(tokens::COLON)) next_type = parse_type_annotation("a type name after ':'");
        bindings.push_back(parallel_let_binding{next_name.range, next_name.text, std::move(next_type), {}});
      } while (match(tokens::COMMA));

      expect(tokens::EQUAL, "'=' after the variable names");
      for (std::size_t index = 0; index < bindings.size(); ++index)
      {
        bindings[index].initializer = parse_expression();
        if (index + 1 < bindings.size())
          expect(tokens::COMMA, "one initializer for each variable");
      }
      if (check(tokens::COMMA))
        throw parse_error("A parallel let declaration needs exactly one initializer per variable", peek()->range);
      return std::make_unique<parallel_let_declaration>(
          span{keyword.range.begin, bindings.back().initializer->range.end}, std::move(bindings));
    }

    expression_ref initializer;
    if (match(tokens::EQUAL))
    {
      initializer = parse_expression();
    }

    const int end = initializer ? initializer->range.end : (type_name ? previous().range.end : name.range.end);
    return std::make_unique<let_declaration>(span{keyword.range.begin, end}, name.text, private_member, weak_member,
                                             std::move(type_name),
                                             std::move(initializer));
  }

  auto syntax_parser::parse_const_declaration(const bool allow_private,
                                               const bool face_requirement) -> statement_ref
  {
    const token &keyword = previous();
    const bool private_member = allow_private && match(tokens::DOT);
    const token &name = expect(tokens::IDENTIFIER, "a SCREAMING_SNAKE_CASE identifier after 'const'");
    if (!is_constant_name(name.text))
      throw parse_error("Constant names must use ASCII SCREAMING_SNAKE_CASE ([A-Z][A-Z0-9_]*)",
                        name.range);
    std::optional<std::string> type_name;
    if (match(tokens::COLON)) type_name = parse_type_annotation("a type name after ':'");
    if (allow_private && !type_name)
      throw parse_error("Constant class field '" + name.text + "' requires a type annotation", name.range);
    if (face_requirement)
    {
      if (check(tokens::EQUAL))
        throw parse_error("A face field promise cannot have an initializer", peek()->range);
      return std::make_unique<const_declaration>(span{keyword.range.begin, previous().range.end},
                                                  name.text, private_member, std::move(type_name), nullptr);
    }
    if (!match(tokens::EQUAL))
      throw parse_error("Constant '" + name.text + "' requires an initializer", name.range);
    auto initializer = parse_expression();
    return std::make_unique<const_declaration>(span{keyword.range.begin, initializer->range.end},
                                               name.text, private_member, std::move(type_name),
                                               std::move(initializer));
  }

  auto syntax_parser::parse_function_declaration(const bool body_optional, const bool allow_private,
                                                  const bool allow_mutating) -> statement_ref
  {
    const token &keyword = previous();
    const bool private_member = allow_private && match(tokens::DOT);
    const token &name = allow_mutating && match(tokens::METHOD_IDENTIFIER)
                            ? previous()
                            : expect(tokens::IDENTIFIER, "a function name after 'fun'");
    std::vector<std::string> type_parameters;
    std::vector<std::optional<std::string>> type_constraints;
    if (match(tokens::LANGLE))
    {
      do
      {
        type_parameters.push_back(expect(tokens::IDENTIFIER, "a generic type parameter").text);
        if (match(tokens::KWD_IS))
          type_constraints.emplace_back(parse_type_annotation("a face constraint after 'is'"));
        else type_constraints.emplace_back();
      } while (match(tokens::COMMA));
      expect(tokens::RANGLE, "'>' after generic type parameters");
    }
    if (!type_parameters.empty() && body_optional)
      throw parse_error("Generic face methods are not supported", name.range);
    expect(tokens::LPAREN, "'(' after the function name");
    std::vector<function_parameter> parameters;
    while (!check(tokens::RPAREN))
    {
      const token &parameter_name = expect(tokens::IDENTIFIER, "a parameter name");
      std::optional<std::string> parameter_type;
      if (match(tokens::COLON))
      {
        parameter_type = parse_type_annotation("a parameter type after ':'");
      }
      expression_ref fallback;
      if (match(tokens::EQUAL)) fallback = parse_nested_expression();
      parameters.emplace_back(parameter_name.text, std::move(parameter_type), std::move(fallback));
      if (!match(tokens::COMMA) || check(tokens::RPAREN))
      {
        break;
      }
    }
    expect(tokens::RPAREN, "')' after the function parameters");
    std::optional<std::string> return_type;
    if (match(tokens::COLON))
    {
      return_type = parse_type_annotation("a return type after ':'");
    }
    std::unique_ptr<block_statement> body;
    expression_ref expression_body;
    if (check(tokens::LBRACE))
    {
      body = parse_block();
    }
    else if (match(tokens::FAT_ARROW))
    {
      expression_body = parse_expression();
    }
    else if (!body_optional)
    {
      expect(tokens::LBRACE, "'{' to begin the function body");
    }
    const int end = body ? body->range.end : expression_body ? expression_body->range.end : previous().range.end;
    return std::make_unique<function_declaration>(span{keyword.range.begin, end}, name.text, private_member, false,
                                                  std::move(type_parameters),
                                                  std::move(type_constraints),
                                                  std::move(parameters), std::move(return_type),
                                                  std::move(body), std::move(expression_body));
  }

  auto syntax_parser::parse_test_declaration() -> statement_ref
  {
    const token keyword = previous();
    if (!check(tokens::STRING_BEGIN))
      throw parse_error("A test requires a quoted name after 'test'",
                        at_end() ? keyword.range : peek()->range);
    auto name_expression = parse_string();
    const auto *literal = dynamic_cast<const string_expression *>(name_expression.get());
    std::string display_name;
    for (const auto &part : literal->parts)
    {
      if (part.interpolation)
        throw parse_error("A test name cannot contain interpolation", name_expression->range);
      display_name += part.text;
    }
    if (display_name.empty())
      throw parse_error("A test name cannot be empty", name_expression->range);
    if (display_name.front() == '/' || display_name.back() == '/' ||
        display_name.find("//") != std::string::npos)
      throw parse_error("A test name must have nonempty slash-separated suite and case names",
                        name_expression->range);
    const auto name_range = name_expression->range;
    auto body = parse_block();
    const auto end = body->range.end;
    auto test = std::make_unique<function_declaration>(
        span{keyword.range.begin, end}, "$sagan_test_" + std::to_string(keyword.range.begin),
        false, false, std::vector<std::string>{},
        std::vector<std::optional<std::string>>{}, std::vector<function_parameter>{},
        std::optional<std::string>{"Void"}, std::move(body), expression_ref{});
    test->test_name = std::move(display_name);
    test->test_name_range = name_range;
    return test;
  }

  auto syntax_parser::parse_constructor_declaration() -> statement_ref
  {
    const token &keyword = previous();
    expect(tokens::LPAREN, "'(' after 'new'");
    std::vector<function_parameter> parameters;
    while (!check(tokens::RPAREN))
    {
      const token &parameter_name = expect(tokens::IDENTIFIER, "a constructor parameter name");
      expect(tokens::COLON, "':' after a constructor parameter name");
      auto annotation = parse_type_annotation("a constructor parameter type after ':'");
      expression_ref fallback;
      if (match(tokens::EQUAL)) fallback = parse_nested_expression();
      parameters.emplace_back(parameter_name.text, std::move(annotation), std::move(fallback));
      if (!match(tokens::COMMA) || check(tokens::RPAREN)) break;
    }
    expect(tokens::RPAREN, "')' after the constructor parameters");
    std::vector<function_declaration::parent_initializer> parent_initializers;
    if (match(tokens::KWD_IS))
    {
      do
      {
        const token &parent = expect(tokens::IDENTIFIER, "a parent class name after 'is'");
        expect(tokens::LPAREN, "'(' after a parent class name");
        std::vector<expression_ref> arguments;
        if (!check(tokens::RPAREN))
          while (true)
          {
            arguments.push_back(parse_nested_expression());
            if (!match(tokens::COMMA) || check(tokens::RPAREN)) break;
          }
        const token &close = expect(tokens::RPAREN, "')' after parent constructor arguments");
        parent_initializers.push_back({parent.text, {parent.range.begin, close.range.end},
                                       std::move(arguments)});
      } while (match(tokens::COMMA));
    }
    auto body = parse_block();
    auto result = std::make_unique<function_declaration>(span{keyword.range.begin, body->range.end}, "new", false,
        true, std::vector<std::string>{}, std::vector<std::optional<std::string>>{},
        std::move(parameters), std::optional<std::string>{"Void"}, std::move(body), nullptr);
    result->parent_initializers = std::move(parent_initializers);
    return result;
  }

  auto syntax_parser::parse_type_declaration(const type_declaration::kind type) -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a type name after '" + keyword.text + "'");
    std::vector<std::string> type_parameters;
    std::vector<std::optional<std::string>> type_constraints;
    if (match(tokens::LANGLE))
    {
      do
      {
        type_parameters.push_back(expect(tokens::IDENTIFIER, "a generic type parameter").text);
        if (match(tokens::KWD_IS))
          type_constraints.emplace_back(parse_type_annotation("a face constraint after 'is'"));
        else type_constraints.emplace_back();
      } while (match(tokens::COMMA));
      expect(tokens::RANGLE, "'>' after generic type parameters");
    }
    std::optional<std::string> composition_keyword;
    std::vector<std::string> base_classes;
    std::vector<std::string> interfaces;
    if (type == type_declaration::kind::class_type && match(tokens::KWD_IS))
    {
      base_classes.push_back(parse_type_annotation("a superclass after 'is'"));
      while (match(tokens::COMMA))
      {
        if (match(tokens::KWD_HAS))
        {
          composition_keyword = "has";
          break;
        }
        base_classes.push_back(parse_type_annotation("a superclass after ','"));
      }
    }
    else if (match(tokens::KWD_IS) || match(tokens::KWD_HAS))
    {
      composition_keyword = previous().text;
      if (type == type_declaration::kind::class_type && *composition_keyword != "has")
        throw parse_error("Classes use 'is' for a superclass and 'has' for faces", previous().range);
    }
    if (composition_keyword)
    {
      do
      {
        interfaces.push_back(parse_type_annotation("an interface name in the composition list"));
      } while (match(tokens::COMMA));
    }

    const token &open = expect(tokens::LBRACE, "'{' to begin the type body");
    std::vector<statement_ref> members;
    std::vector<type_declaration::enum_member> enum_members;
    skip_newlines();
    while (!check(tokens::RBRACE))
    {
      if (at_end())
      {
        throw parse_error("Expected '}' after the type body", open.range);
      }
      auto documentation = parse_documentation_comments();
      if (!documentation.empty() && check(tokens::RBRACE))
      {
        throw parse_error("A documentation comment must precede a declaration", documentation.back().range);
      }
      if (type == type_declaration::kind::enum_type)
      {
        const token &member = expect(tokens::IDENTIFIER, "an enum member name");
        std::vector<std::string> payload_types;
        std::optional<std::string> numeric_value;
        int member_end = member.range.end;
        if (match(tokens::LPAREN))
        {
          if (!check(tokens::RPAREN))
          {
            do
            {
              payload_types.push_back(expect(tokens::IDENTIFIER, "a payload type").text);
            } while (match(tokens::COMMA));
          }
          member_end = expect(tokens::RPAREN, "')' after enum payload types").range.end;
        }
        if (match(tokens::EQUAL))
        {
          const bool negative = match(tokens::MINUS);
          const token &number = expect(tokens::INTEGER, "an integer after '=' in an enum case");
          numeric_value = std::string(negative ? "-" : "") + number.text;
          member_end = number.range.end;
        }
        enum_members.emplace_back(member.text, span{member.range.begin, member_end},
                                  std::move(payload_types), std::move(numeric_value),
                                  std::move(documentation));
        if (match(tokens::COMMA))
        {
          skip_newlines();
          continue;
        }
      }
      else if (match(tokens::KWD_FUN))
      {
        const bool is_interface = type == type_declaration::kind::interface_type;
        auto member = parse_function_declaration(is_interface, true, true);
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type == type_declaration::kind::class_type && match(tokens::KWD_NEW))
      {
        auto member = parse_constructor_declaration();
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type != type_declaration::kind::enum_type && match(tokens::KWD_LET))
      {
        auto member = parse_let_declaration(true);
        if (type == type_declaration::kind::interface_type)
        {
          const auto *field = dynamic_cast<const let_declaration *>(member.get());
          if (!field || !field->type_name || field->initializer)
            throw parse_error("A face field promise needs a type and cannot have an initializer", member->range);
        }
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type != type_declaration::kind::enum_type && match(tokens::KWD_CONST))
      {
        auto member = parse_const_declaration(true, type == type_declaration::kind::interface_type);
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type == type_declaration::kind::class_type && match(tokens::KWD_WEAK))
      {
        expect(tokens::KWD_LET, "'let' after 'weak'");
        auto member = parse_let_declaration(true, true);
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else
      {
        const std::string expected = type == type_declaration::kind::interface_type
                                         ? "an interface method declaration"
                                         : "a field or method declaration";
        throw parse_error("Expected " + expected, peek()->range);
      }
      if (!check(tokens::RBRACE))
      {
        expect(tokens::NEWLINE, "a newline after the type member");
        skip_newlines();
      }
    }
    const token &close = expect(tokens::RBRACE, "'}' after the type body");
    return std::make_unique<type_declaration>(span{keyword.range.begin, close.range.end}, type, name.text,
                                              std::move(type_parameters),
                                              std::move(type_constraints),
                                              std::move(composition_keyword), std::move(base_classes),
                                              std::move(interfaces),
                                              std::move(members), std::move(enum_members));
  }

  auto syntax_parser::parse_unit_expression(const bool allow_composite) -> std::string
  {
    const auto factor = [&]() -> std::string
    {
      if (match(tokens::IDENTIFIER))
      {
        std::string result = previous().text;
        if ((result == "Delta" || result == "Δ") && match(tokens::LANGLE))
        {
          result = "Delta<" + parse_unit_expression(true);
          expect(tokens::RANGLE, "'>' after the delta unit");
          result += '>';
        }
        return result;
      }
      if (match(tokens::INTEGER) || match(tokens::FLOAT)) return previous().text;
      if (allow_composite && match(tokens::LPAREN))
      {
        std::string result = "(" + parse_unit_expression(true);
        expect(tokens::RPAREN, "')' after the unit expression");
        return result + ')';
      }
      throw parse_error("Expected a unit name or scale factor", at_end() ? previous().range : peek()->range);
    };

    std::string result = factor();
    if (match(tokens::CARET))
    {
      const bool negative = match(tokens::MINUS);
      const token &exponent = expect(tokens::INTEGER, "an integer unit exponent");
      result += '^' + std::string(negative ? "-" : "") + exponent.text;
    }
    while (allow_composite && (match(tokens::STAR) || match(tokens::SLASH)))
    {
      const std::string operation = previous().text;
      result += ' ' + operation + ' ' + factor();
      if (match(tokens::CARET))
      {
        const bool negative = match(tokens::MINUS);
        const token &exponent = expect(tokens::INTEGER, "an integer unit exponent");
        result += '^' + std::string(negative ? "-" : "") + exponent.text;
      }
    }
    return result;
  }

  auto syntax_parser::parse_measurement_declaration(const measurement_declaration::kind type) -> statement_ref
  {
    const token keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a measurement name");
    std::optional<std::string> dimension;
    std::optional<std::string> definition;
    if (match(tokens::COLON)) dimension = expect(tokens::IDENTIFIER, "a dimension name after ':'").text;
    if (match(tokens::EQUAL)) definition = parse_unit_expression(true);
    if ((type == measurement_declaration::kind::quantity || type == measurement_declaration::kind::linear_unit) &&
        !definition)
      throw parse_error("This measurement declaration requires a definition after '='", name.range);

    std::vector<std::pair<std::string, std::string>> properties;
    int end = previous().range.end;
    if (match(tokens::LBRACE))
    {
      skip_newlines();
      while (!check(tokens::RBRACE))
      {
        const token &property = expect(tokens::IDENTIFIER, "a unit property name");
        expect(tokens::COLON, "':' after the unit property name");
        std::string value;
        while (!at_end() && !check(tokens::NEWLINE) && !check(tokens::RBRACE))
        {
          if (!value.empty()) value += ' ';
          value += advance().text;
        }
        if (value.empty()) throw parse_error("A unit property requires a value", property.range);
        properties.emplace_back(property.text, std::move(value));
        skip_newlines();
      }
      end = expect(tokens::RBRACE, "'}' after unit properties").range.end;
    }
    return std::make_unique<measurement_declaration>(span{keyword.range.begin, end}, type, name.text,
                                                      std::move(dimension), std::move(definition),
                                                      std::move(properties));
  }

  auto syntax_parser::parse_module_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const std::string name = parse_qualified_name("a module name after 'module'");
    return std::make_unique<module_declaration>(span{keyword.range.begin, previous().range.end}, name);
  }

  auto syntax_parser::parse_import_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const std::string name = parse_qualified_name("a name after 'import'");
    std::optional<std::string> source;
    if (match(tokens::KWD_FROM))
    {
      if (name.find('.') != std::string::npos)
        throw parse_error("A selective import must name one exported identifier before 'from'", previous().range);
      source = parse_qualified_name("a module name after 'from'");
    }
    std::optional<std::string> alias;
    if (match(tokens::KWD_AS))
    {
      alias = expect(tokens::IDENTIFIER, "an alias after 'as'").text;
    }
    return std::make_unique<import_declaration>(span{keyword.range.begin, previous().range.end}, name,
                                                std::move(source), std::move(alias));
  }

  auto syntax_parser::parse_qualified_name(const std::string &description) -> std::string
  {
    std::string name = expect(tokens::IDENTIFIER, description).text;
    while (match(tokens::DOT))
    {
      name += '.';
      name += expect(tokens::IDENTIFIER, "an identifier after '.' in a qualified name").text;
    }
    return name;
  }

  auto syntax_parser::parse_export_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a name after 'export'");
    std::optional<std::string> alias;
    if (match(tokens::KWD_AS))
    {
      alias = expect(tokens::IDENTIFIER, "an alias after 'as'").text;
    }
    return std::make_unique<export_declaration>(span{keyword.range.begin, previous().range.end}, name.text,
                                                std::move(alias));
  }

  auto syntax_parser::parse_expression_statement() -> statement_ref
  {
    auto target = parse_expression();
    if (match(tokens::COMMA))
    {
      std::vector<expression_ref> targets;
      targets.push_back(std::move(target));
      do
      {
        if (check(tokens::EQUAL) || check(tokens::NEWLINE) || at_end())
          throw parse_error("Expected a variable after ',' in parallel assignment", previous().range);
        targets.push_back(parse_expression());
      } while (match(tokens::COMMA));
      const token &assignment = expect(tokens::EQUAL, "'=' after parallel assignment targets");
      std::vector<expression_ref> values;
      do
      {
        if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
          throw parse_error("Expected a value in parallel assignment", assignment.range);
        values.push_back(parse_expression());
      } while (match(tokens::COMMA));
      if (values.size() != targets.size())
        throw parse_error("Parallel assignment requires the same number of targets and values",
                          span{targets.front()->range.begin, values.back()->range.end});
      const span range{targets.front()->range.begin, values.back()->range.end};
      return std::make_unique<parallel_assignment_statement>(range, std::move(targets), std::move(values));
    }
    if (!match(tokens::EQUAL) && !match(tokens::PLUS_EQUAL) && !match(tokens::MINUS_EQUAL) &&
        !match(tokens::STAR_EQUAL) && !match(tokens::SLASH_EQUAL) && !match(tokens::PERCENT_EQUAL) &&
        !match(tokens::CARET_EQUAL))
    {
      const span range = target->range;
      return std::make_unique<expression_statement>(range, std::move(target));
    }
    const token assignment = previous();
    if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
    {
      throw parse_error("Expected an expression after '" + assignment.text + "'", assignment.range);
    }
    auto value = parse_expression();
    if (check(tokens::EQUAL) || check(tokens::PLUS_EQUAL) || check(tokens::MINUS_EQUAL) ||
        check(tokens::STAR_EQUAL) || check(tokens::SLASH_EQUAL) || check(tokens::PERCENT_EQUAL) ||
        check(tokens::CARET_EQUAL))
    {
      throw parse_error("Assignment statements cannot be chained; use ':=' for value-producing assignment",
                        peek()->range);
    }
    const span range{target->range.begin, value->range.end};
    return std::make_unique<assignment_statement>(range, std::move(target), assignment.text, std::move(value));
  }

  auto syntax_parser::parse_if_statement() -> statement_ref
  {
    const token &keyword = previous();
    auto condition = parse_expression();
    auto then_branch = parse_statement_body("the 'if' body");
    statement_ref else_branch;
    if (match_after_newlines(tokens::KWD_ELSE))
    {
      if (match(tokens::KWD_IF))
      {
        else_branch = parse_if_statement();
      }
      else
      {
        else_branch = parse_statement_body("the 'else' body");
      }
    }
    const int end = else_branch ? else_branch->range.end : then_branch->range.end;
    return std::make_unique<if_statement>(span{keyword.range.begin, end}, std::move(condition),
                                          std::move(then_branch), std::move(else_branch));
  }

  auto syntax_parser::parse_condition_loop(const condition_loop_statement::kind type) -> statement_ref
  {
    const token &keyword = previous();
    auto condition = parse_expression();
    loop_depth++;
    auto body = parse_statement_body("the loop body");
    loop_depth--;
    return std::make_unique<condition_loop_statement>(span{keyword.range.begin, body->range.end}, type,
                                                       std::move(condition), std::move(body));
  }

  auto syntax_parser::parse_for_statement() -> statement_ref
  {
    const token &keyword = previous();
    const token &binding = expect(tokens::IDENTIFIER, "an identifier after 'for'");
    expect(tokens::KWD_IN, "'in' after the loop binding");
    auto iterable = parse_expression();
    loop_depth++;
    auto body = parse_statement_body("the 'for' body");
    loop_depth--;
    return std::make_unique<for_statement>(span{keyword.range.begin, body->range.end}, binding.text,
                                            std::move(iterable), std::move(body));
  }

  auto syntax_parser::parse_loop_control(const loop_control_statement::kind type) -> statement_ref
  {
    const token &keyword = previous();
    if (loop_depth == 0)
    {
      throw parse_error("'" + keyword.text + "' is only valid inside a loop", keyword.range);
    }
    return std::make_unique<loop_control_statement>(keyword.range, type);
  }

  auto syntax_parser::parse_return_statement() -> statement_ref
  {
    const token &keyword = previous();
    if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
    {
      return std::make_unique<return_statement>(keyword.range, nullptr);
    }
    auto value = parse_expression();
    const int end = value->range.end;
    return std::make_unique<return_statement>(span{keyword.range.begin, end}, std::move(value));
  }

  auto syntax_parser::parse_yield_statement() -> statement_ref
  {
    const token &keyword = previous();
    if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
    {
      return std::make_unique<yield_statement>(keyword.range, nullptr);
    }
    auto value = parse_expression();
    const int end = value->range.end;
    return std::make_unique<yield_statement>(span{keyword.range.begin, end}, std::move(value));
  }

  auto syntax_parser::parse_match_statement() -> statement_ref
  {
    const token &keyword = previous();
    auto subject = parse_expression();
    const token &open = expect(tokens::LBRACE, "'{' to begin the match body");
    std::vector<match_case> cases;
    bool found_fallback = false;
    skip_newlines();
    while (!check(tokens::RBRACE))
    {
      if (at_end())
      {
        throw parse_error("Expected '}' after the match cases", open.range);
      }
      const token &case_keyword = expect(tokens::KWD_CASE, "'case' inside the match body");
      expression_ref pattern;
      if (match(tokens::KWD_ELSE))
      {
        if (found_fallback)
        {
          throw parse_error("A match statement can have only one 'case else' branch", previous().range);
        }
        found_fallback = true;
      }
      else
      {
        if (found_fallback)
        {
          throw parse_error("'case else' must be the last match branch", case_keyword.range);
        }
        pattern = parse_expression();
      }
      auto body = parse_statement_body("the 'case' body");
      const span case_range{case_keyword.range.begin, body->range.end};
      cases.emplace_back(std::move(pattern), std::move(body), case_range);
      if (!check(tokens::RBRACE))
      {
        expect(tokens::NEWLINE, "a newline after the match branch");
        skip_newlines();
      }
    }
    const token &close = expect(tokens::RBRACE, "'}' after the match cases");
    if (cases.empty())
    {
      throw parse_error("A match statement requires at least one case", span{open.range.begin, close.range.end});
    }
    return std::make_unique<match_statement>(span{keyword.range.begin, close.range.end}, std::move(subject),
                                             std::move(cases));
  }

  auto syntax_parser::parse_hope_statement() -> statement_ref
  {
    const token &keyword = previous();
    auto protected_body = parse_statement_body("the 'hope' body");
    std::vector<exception_handler> handlers;
    while (match_after_newlines(tokens::KWD_UNLESS))
    {
      const token &unless_keyword = previous();
      auto pattern = parse_expression();
      auto body = parse_statement_body("the 'unless' body");
      const span handler_range{unless_keyword.range.begin, body->range.end};
      handlers.emplace_back(std::move(pattern), std::move(body), handler_range);
    }
    std::unique_ptr<block_statement> cleanup;
    if (match_after_newlines(tokens::KWD_FINALLY))
    {
      cleanup = parse_statement_body("the 'finally' body");
    }
    if (handlers.empty() && !cleanup)
    {
      throw parse_error("A hope statement requires at least one 'unless' or 'finally' clause",
                        keyword.range);
    }
    const int end = cleanup ? cleanup->range.end : handlers.back().range.end;
    return std::make_unique<hope_statement>(span{keyword.range.begin, end}, std::move(protected_body),
                                            std::move(handlers), std::move(cleanup));
  }

  auto syntax_parser::parse_scream_statement() -> statement_ref
  {
    const token &keyword = previous();
    if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
    {
      throw parse_error("Expected an exception value after 'scream'", keyword.range);
    }
    auto value = parse_expression();
    const int end = value->range.end;
    return std::make_unique<scream_statement>(span{keyword.range.begin, end}, std::move(value));
  }

  auto syntax_parser::parse_block() -> std::unique_ptr<block_statement>
  {
    const token &open = expect(tokens::LBRACE, "'{' to begin a block");
    std::vector<statement_ref> body;
    skip_newlines();
    while (!check(tokens::RBRACE))
    {
      if (at_end())
      {
        throw parse_error("Expected '}' after the block", span{open.range.begin, open.range.end});
      }
      body.push_back(parse_statement());
      if (!check(tokens::RBRACE))
      {
        expect(tokens::NEWLINE, "a newline after the statement");
        skip_newlines();
      }
    }
    const token &close = expect(tokens::RBRACE, "'}' after the block");
    return std::make_unique<block_statement>(span{open.range.begin, close.range.end}, std::move(body));
  }

  auto syntax_parser::parse_statement_body(const std::string &description) -> std::unique_ptr<block_statement>
  {
    if (check(tokens::LBRACE)) return parse_block();
    if (at_end() || check(tokens::NEWLINE) || check(tokens::RBRACE))
    {
      const span error_range = at_end() ? previous().range : peek()->range;
      throw parse_error("Expected '{' or a same-line statement for " + description, error_range);
    }
    std::vector<statement_ref> body;
    body.push_back(parse_statement());
    const span range = body.front()->range;
    return std::make_unique<block_statement>(range, std::move(body));
  }

  auto syntax_parser::parse_expression() -> expression_ref
  {
    return parse_assignment();
  }

  auto syntax_parser::parse_type_annotation(const std::string &description) -> std::string
  {
    if (match(tokens::LPAREN))
    {
      std::string result = "(";
      if (!check(tokens::RPAREN))
      {
        do
        {
          if (result.size() != 1) result += ", ";
          result += parse_type_annotation("a function parameter type");
        } while (match(tokens::COMMA));
      }
      expect(tokens::RPAREN, "')' after function parameter types");
      expect(tokens::FAT_ARROW, "'=>' after function parameter types");
      return result + ") => " + parse_type_annotation("a function result type");
    }
    std::string result = expect(tokens::IDENTIFIER, description).text;
    if (result == "Δ") result = "Delta";
    if (match(tokens::LANGLE))
    {
      result += '<';
      do
      {
        if (result.back() != '<') result += ", ";
        result += parse_type_annotation("a type argument");
      } while (match(tokens::COMMA));
      expect(tokens::RANGLE, "'>' after type arguments");
      result += '>';
    }
    const auto parse_unit_exponent = [&]()
    {
      if (!match(tokens::CARET)) return;
      const bool negative = match(tokens::MINUS);
      result += '^' + std::string(negative ? "-" : "") +
                expect(tokens::INTEGER, "an integer unit exponent").text;
    };
    parse_unit_exponent();
    while (match(tokens::STAR) || match(tokens::SLASH))
    {
      const std::string operation = previous().text;
      result += ' ' + operation + ' ' + expect(tokens::IDENTIFIER, "a unit name").text;
      parse_unit_exponent();
    }
    return result;
  }

  auto syntax_parser::parse_nested_expression() -> expression_ref
  {
    const std::size_t surrounding_vector_depth = vector_literal_depth;
    vector_literal_depth = 0;
    auto value = parse_expression();
    vector_literal_depth = surrounding_vector_depth;
    return value;
  }

  auto syntax_parser::parse_assignment() -> expression_ref
  {
    auto target = parse_conditional();
    if (!match(tokens::ASSIGN_VALUE))
    {
      return target;
    }
    auto value = parse_assignment();
    const span range{target->range.begin, value->range.end};
    return std::make_unique<assignment_expression>(range, std::move(target), std::move(value));
  }

  auto syntax_parser::parse_conditional() -> expression_ref
  {
    auto condition = parse_coalesce();
    if (!match(tokens::QUESTION))
    {
      return condition;
    }
    auto when_true = parse_assignment();
    expect(tokens::SEMICOLON, "';' between conditional branches");
    auto when_false = parse_conditional();
    const span range{condition->range.begin, when_false->range.end};
    return std::make_unique<conditional_expression>(range, std::move(condition), std::move(when_true),
                                                     std::move(when_false));
  }

  auto syntax_parser::parse_coalesce() -> expression_ref
  {
    auto left = parse_or();
    if (!match(tokens::COALESCE)) return left;
    const token operation = previous();
    auto right = parse_coalesce();
    return std::make_unique<binary_expression>(span{left->range.begin, right->range.end}, std::move(left),
                                               operation.text, std::move(right));
  }

  auto syntax_parser::parse_or() -> expression_ref
  {
    auto left = parse_and();
    while (match(tokens::KWD_OR))
    {
      const token operation = previous();
      auto right = parse_and();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_and() -> expression_ref
  {
    auto left = parse_equality();
    while (match(tokens::KWD_AND))
    {
      const token operation = previous();
      auto right = parse_equality();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_equality() -> expression_ref
  {
    auto left = parse_comparison();
    while (match(tokens::EQUAL_EQUAL) || match(tokens::BANG_EQUAL))
    {
      const token operation = previous();
      auto right = parse_comparison();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_comparison() -> expression_ref
  {
    auto left = parse_additive();
    const bool has_comparison = match(tokens::LANGLE) || match(tokens::LESS_EQUAL) ||
                                (vector_literal_depth == 0 && match(tokens::RANGLE)) ||
                                match(tokens::GREATER_EQUAL) || match(tokens::KWD_IS) ||
                                match(tokens::KWD_HAS);
    if (!has_comparison)
    {
      return left;
    }
    const token operation = previous();
    auto right = parse_additive();
    if (check(tokens::LANGLE) || check(tokens::LESS_EQUAL) ||
        (vector_literal_depth == 0 && check(tokens::RANGLE)) ||
        check(tokens::GREATER_EQUAL) || check(tokens::KWD_IS) || check(tokens::KWD_HAS))
    {
      throw parse_error("Chained comparisons are not allowed; combine comparisons with 'and'", peek()->range);
    }
    const span range{left->range.begin, right->range.end};
    return std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
  }

  auto syntax_parser::parse_additive() -> expression_ref
  {
    auto left = parse_multiplicative();
    while (match(tokens::PLUS) || match(tokens::MINUS))
    {
      const token operation = previous();
      auto right = parse_multiplicative();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_multiplicative() -> expression_ref
  {
    auto left = parse_unary();
    while (match(tokens::STAR) || match(tokens::SLASH) || match(tokens::PERCENT))
    {
      const token operation = previous();
      auto right = parse_unary();
      const span range{left->range.begin, right->range.end};
      left = std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
    }
    return left;
  }

  auto syntax_parser::parse_unary() -> expression_ref
  {
    if (match(tokens::SPREAD))
    {
      const token operation = previous();
      auto value = parse_unary();
      return std::make_unique<spread_expression>(span{operation.range.begin, value->range.end}, std::move(value));
    }
    if (match(tokens::PLUS_PLUS) || match(tokens::MINUS_MINUS) || match(tokens::BANG) ||
        match(tokens::KWD_NOT) || match(tokens::PLUS) || match(tokens::MINUS))
    {
      const token operation = previous();
      auto operand = parse_unary();
      return std::make_unique<unary_expression>(span{operation.range.begin, operand->range.end}, operation.text,
                                                std::move(operand));
    }
    return parse_exponent();
  }

  auto syntax_parser::parse_exponent() -> expression_ref
  {
    auto left = parse_postfix();
    if (!match(tokens::CARET))
    {
      return left;
    }
    const token operation = previous();
    auto right = parse_unary();
    const span range{left->range.begin, right->range.end};
    return std::make_unique<binary_expression>(range, std::move(left), operation.text, std::move(right));
  }

  auto syntax_parser::parse_postfix() -> expression_ref
  {
    auto value = parse_primary();
    while (!at_end())
    {
      const bool unit_attachable = dynamic_cast<literal_expression *>(value.get()) != nullptr ||
                                   dynamic_cast<collection_expression *>(value.get()) != nullptr;
      if (unit_attachable && check(tokens::IDENTIFIER) &&
          (current + 1 >= input.size() || input[current + 1].id != tokens::LPAREN))
      {
        const int begin = value->range.begin;
        std::string unit = parse_unit_expression(false);
        const int end = previous().range.end;
        value = std::make_unique<measured_expression>(span{begin, end}, std::move(value),
                                                      std::move(unit), false);
        continue;
      }
      if (unit_attachable && check(tokens::LPAREN) && value->range.end < peek()->range.begin)
      {
        const int begin = value->range.begin;
        advance();
        std::string unit = parse_unit_expression(true);
        const int end = expect(tokens::RPAREN, "')' after the composite unit suffix").range.end;
        value = std::make_unique<measured_expression>(span{begin, end}, std::move(value),
                                                      std::move(unit), false);
        continue;
      }
      if (match(tokens::KWD_AS))
      {
        const int begin = value->range.begin;
        const bool parenthesized = match(tokens::LPAREN);
        std::string unit = parse_unit_expression(parenthesized);
        if (parenthesized) expect(tokens::RPAREN, "')' after the conversion unit");
        const int end = previous().range.end;
        value = std::make_unique<measured_expression>(span{begin, end}, std::move(value),
                                                      std::move(unit), true);
        continue;
      }
      if (match(tokens::LPAREN))
      {
        std::vector<expression_ref> arguments;
        if (!check(tokens::RPAREN))
        {
          while (true)
          {
            arguments.push_back(parse_nested_expression());
            if (!match(tokens::COMMA) || check(tokens::RPAREN))
            {
              break;
            }
          }
        }
        const token &close = expect(tokens::RPAREN, "')' after the call arguments");
        value = std::make_unique<call_expression>(span{value->range.begin, close.range.end}, std::move(value),
                                                  std::move(arguments));
        continue;
      }
      if (match(tokens::LBRACKET))
      {
        auto index = parse_nested_expression();
        const token &close = expect(tokens::RBRACKET, "']' after the index expression");
        value = std::make_unique<index_expression>(span{value->range.begin, close.range.end}, std::move(value),
                                                   std::move(index));
        continue;
      }
      if (match(tokens::DOT) || match(tokens::SAFE_DOT))
      {
        const token access = previous();
        const bool safe = access.id == tokens::SAFE_DOT;
        if (!match(tokens::IDENTIFIER) && !match(tokens::METHOD_IDENTIFIER))
        {
          const span error_range = at_end() || check(tokens::NEWLINE) ? access.range : peek()->range;
          throw parse_error("Expected a member name after '" + std::string(safe ? "?." : ".") + "'",
                            error_range);
        }
        const token member = previous();
        std::string member_name = member.text;
        int member_end = member.range.end;
        if (check(tokens::LANGLE))
        {
          std::size_t scan = current;
          int depth = 0;
          do
          {
            if (input[scan].id == tokens::LANGLE) ++depth;
            else if (input[scan].id == tokens::RANGLE) --depth;
            ++scan;
          } while (scan < input.size() && depth > 0);
          if (depth == 0 && scan < input.size() && input[scan].id == tokens::LPAREN)
          {
            advance();
            member_name += '<';
            bool first = true;
            do
            {
              if (!first) member_name += ", ";
              member_name += parse_type_annotation("an explicit method type argument");
              first = false;
            } while (match(tokens::COMMA));
            member_end = expect(tokens::RANGLE, "'>' after explicit method type arguments").range.end;
            member_name += '>';
          }
        }
        value = std::make_unique<member_expression>(span{value->range.begin, member_end}, std::move(value),
                                                    std::move(member_name), safe);
        continue;
      }
      if (match(tokens::PLUS_PLUS) || match(tokens::MINUS_MINUS))
      {
        const token operation = previous();
        value = std::make_unique<unary_expression>(span{value->range.begin, operation.range.end}, operation.text,
                                                   std::move(value), true);
        continue;
      }
      break;
    }
    return value;
  }

  auto syntax_parser::parse_primary() -> expression_ref
  {
    if (match(tokens::STRING))
    {
      const token &value = previous();
      std::vector<string_part> parts;
      parts.emplace_back(value.text);
      return std::make_unique<string_expression>(value.range, std::move(parts), true, false);
    }
    if (check(tokens::STRING_BEGIN))
    {
      return parse_string();
    }
    if (check(tokens::LBRACKET))
    {
      return parse_array();
    }
    if (check(tokens::LBRACE))
    {
      return parse_dictionary();
    }
    if (check(tokens::LANGLE))
    {
      return parse_vector();
    }
    if (check(tokens::IDENTIFIER) && peek()->text == "s" && current + 1 < input.size() &&
        input[current].range.end == input[current + 1].range.begin &&
        (input[current + 1].id == tokens::LANGLE || input[current + 1].id == tokens::LPAREN))
    {
      const int prefix_begin = advance().range.begin;
      if (check(tokens::LANGLE))
        return parse_vector(collection_expression::kind::spherical_vector, prefix_begin);
      return parse_parenthesized(collection_expression::kind::spherical_point, prefix_begin);
    }
    if (match(tokens::IDENTIFIER) || match(tokens::KWD_SELF) || match(tokens::KWD_SUPER))
    {
      const token &value = previous();
      std::string name = value.text;
      int end = value.range.end;
      if (check(tokens::LANGLE))
      {
        std::size_t scan = current;
        int depth = 0;
        do
        {
          if (input[scan].id == tokens::LANGLE) ++depth;
          else if (input[scan].id == tokens::RANGLE) --depth;
          ++scan;
        } while (scan < input.size() && depth > 0);
        if (depth == 0 && scan < input.size() &&
            (input[scan].id == tokens::DOT || input[scan].id == tokens::LPAREN))
        {
          advance();
          name += '<';
          bool first = true;
          do
          {
            if (!first) name += ", ";
            name += parse_type_annotation("an explicit generic type argument");
            first = false;
          } while (match(tokens::COMMA));
          end = expect(tokens::RANGLE, "'>' after explicit generic type arguments").range.end;
          name += '>';
        }
      }
      return std::make_unique<identifier_expression>(span{value.range.begin, end}, std::move(name));
    }
    if (match(tokens::INTEGER) || match(tokens::FLOAT))
    {
      const token &value = previous();
      const auto type = value.id == tokens::INTEGER ? literal_expression::kind::integer
                                                    : literal_expression::kind::floating_point;
      return std::make_unique<literal_expression>(value.range, type, value.text, value.value);
    }
    if (match(tokens::KWD_TRUE) || match(tokens::KWD_FALSE))
    {
      const token &value = previous();
      return std::make_unique<literal_expression>(value.range, literal_expression::kind::boolean, value.text,
                                                  value.id == tokens::KWD_TRUE ? 1.0 : 0.0);
    }
    if (match(tokens::KWD_INF) || match(tokens::KWD_NAN))
    {
      const token &value = previous();
      return std::make_unique<literal_expression>(value.range, literal_expression::kind::floating_point,
                                                  value.text, value.value);
    }
    if (match(tokens::KWD_FUN))
    {
      return parse_lambda();
    }
    if (check(tokens::LPAREN))
    {
      return parse_parenthesized();
    }
    // LCOV_EXCL_START - an empty token stream is accepted before expression parsing begins.
    const span error_range = at_end()
                                 ? (input.empty() ? span{0, 0}
                                                  : span{input.back().range.end, input.back().range.end})
                                 : peek()->range;
    // LCOV_EXCL_STOP
    throw parse_error("Expected an expression", error_range);
  }

  auto syntax_parser::parse_lambda() -> expression_ref
  {
    const token &keyword = previous();
    expect(tokens::LPAREN, "'(' after 'fun' in a lambda");
    std::vector<function_parameter> parameters;
    while (!check(tokens::RPAREN))
    {
      const token &name = expect(tokens::IDENTIFIER, "a lambda parameter name");
      std::optional<std::string> type_name;
      if (match(tokens::COLON))
      {
        type_name = parse_type_annotation("a lambda parameter type after ':'");
      }
      expression_ref fallback;
      if (match(tokens::EQUAL)) fallback = parse_nested_expression();
      parameters.emplace_back(name.text, std::move(type_name), std::move(fallback));
      if (!match(tokens::COMMA) || check(tokens::RPAREN))
      {
        break;
      }
    }
    expect(tokens::RPAREN, "')' after the lambda parameters");
    std::optional<std::string> return_type;
    if (match(tokens::COLON))
    {
      return_type = parse_type_annotation("a lambda return type after ':'");
    }
    expect(tokens::FAT_ARROW, "'=>' before the lambda expression");
    auto body = parse_expression();
    const int end = body->range.end;
    return std::make_unique<lambda_expression>(span{keyword.range.begin, end}, std::move(parameters),
                                               std::move(return_type), std::move(body));
  }

  auto syntax_parser::parse_string() -> expression_ref
  {
    const token &begin = expect(tokens::STRING_BEGIN, "the beginning of a string");
    const bool multiline = begin.text == "\"\"\"";
    std::vector<string_part> parts;

    while (!check(tokens::STRING_END))
    {
      if (at_end())
      {
        throw parse_error("Expected the end of the string", span{begin.range.begin, begin.range.end}); // LCOV_EXCL_LINE
      }
      if (match(tokens::STRING_SEGMENT))
      {
        parts.emplace_back(previous().text);
        continue;
      }
      if (match(tokens::INTERPOLATION_BEGIN))
      {
        if (check(tokens::INTERPOLATION_END))
        {
          throw parse_error("Expected an expression inside string interpolation", peek()->range);
        }
        auto embedded = parse_nested_expression();
        expect(tokens::INTERPOLATION_END, "'}' after the interpolated expression");
        parts.emplace_back(std::move(embedded));
        continue;
      }
      throw parse_error("Expected string text, interpolation, or the closing delimiter", peek()->range); // LCOV_EXCL_LINE
    }

    const token &end = expect(tokens::STRING_END, "the end of the string");
    return std::make_unique<string_expression>(span{begin.range.begin, end.range.end}, std::move(parts), false,
                                               multiline);
  }

  auto syntax_parser::parse_array() -> expression_ref
  {
    const token &open = expect(tokens::LBRACKET, "'[' to begin an array");
    std::vector<expression_ref> elements;
    while (!check(tokens::RBRACKET))
    {
      elements.push_back(parse_nested_expression());
      if (!match(tokens::COMMA))
      {
        break;
      }
      if (check(tokens::RBRACKET))
      {
        break;
      }
    }
    const token &close = expect(tokens::RBRACKET, "']' after the array elements");
    return std::make_unique<collection_expression>(span{open.range.begin, close.range.end},
                                                   collection_expression::kind::array, std::move(elements));
  }

  auto syntax_parser::parse_dictionary() -> expression_ref
  {
    const token &open = expect(tokens::LBRACE, "'{' to begin a dictionary");
    std::vector<dictionary_entry> entries;
    skip_newlines();
    while (!check(tokens::RBRACE))
    {
      auto key_or_spread = parse_nested_expression();
      if (dynamic_cast<spread_expression *>(key_or_spread.get()) != nullptr)
      {
        entries.emplace_back(std::move(key_or_spread));
      }
      else
      {
        expect(tokens::COLON, "':' between a dictionary key and value");
        auto value = parse_nested_expression();
        entries.emplace_back(std::move(key_or_spread), std::move(value));
      }
      skip_newlines();
      if (!match(tokens::COMMA))
      {
        break;
      }
      skip_newlines();
      if (check(tokens::RBRACE))
      {
        break;
      }
    }
    const token &close = expect(tokens::RBRACE, "'}' after the dictionary entries");
    return std::make_unique<dictionary_expression>(span{open.range.begin, close.range.end}, std::move(entries));
  }

  auto syntax_parser::parse_vector(const collection_expression::kind type,
                                   const std::optional<int> prefix_begin) -> expression_ref
  {
    const token &open = expect(tokens::LANGLE, "'<' to begin a vector");
    std::vector<expression_ref> elements;
    vector_literal_depth++;
    skip_newlines();
    while (!check(tokens::RANGLE))
    {
      elements.push_back(parse_expression());
      skip_newlines();
      if (!match(tokens::COMMA))
      {
        break;
      }
      skip_newlines();
      if (check(tokens::RANGLE))
      {
        break;
      }
    }
    vector_literal_depth--;
    const token &close = expect(tokens::RANGLE, "'>' after the vector elements");
    if (type == collection_expression::kind::spherical_vector && elements.size() != 3)
    {
      throw parse_error("A spherical vector literal requires exactly three elements",
                        span{prefix_begin.value_or(open.range.begin), close.range.end});
    }
    if (type == collection_expression::kind::vector && elements.size() < 2)
    {
      throw parse_error("A vector literal requires at least two elements", span{open.range.begin, close.range.end});
    }
    return std::make_unique<collection_expression>(span{prefix_begin.value_or(open.range.begin), close.range.end},
                                                   type, std::move(elements));
  }

  auto syntax_parser::parse_parenthesized(const collection_expression::kind type,
                                          const std::optional<int> prefix_begin) -> expression_ref
  {
    const token &open = expect(tokens::LPAREN, "'(' to begin a grouped expression or point");
    if (check(tokens::RPAREN))
    {
      throw parse_error("Empty parentheses are not an expression", peek()->range);
    }
    auto first = parse_nested_expression();
    if (!match(tokens::COMMA) && type == collection_expression::kind::point)
    {
      const token &close = expect(tokens::RPAREN, "')' after the grouped expression");
      return std::make_unique<grouping_expression>(span{open.range.begin, close.range.end}, std::move(first));
    }

    std::vector<expression_ref> elements;
    elements.push_back(std::move(first));
    while (!check(tokens::RPAREN))
    {
      elements.push_back(parse_nested_expression());
      if (!match(tokens::COMMA))
      {
        break;
      }
    }
    const token &close = expect(tokens::RPAREN, "')' after the point elements");
    if (type == collection_expression::kind::spherical_point && elements.size() != 3)
    {
      throw parse_error("A spherical point literal requires exactly three elements",
                        span{prefix_begin.value_or(open.range.begin), close.range.end});
    }
    if (type == collection_expression::kind::point && elements.size() < 2)
    {
      throw parse_error("A point literal requires at least two elements", span{open.range.begin, close.range.end});
    }
    return std::make_unique<collection_expression>(span{prefix_begin.value_or(open.range.begin), close.range.end},
                                                   type, std::move(elements));
  }
}
