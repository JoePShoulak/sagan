#include "parser.hpp"

#include "parse_error.hpp"
#include "tokens.hpp"

#include <utility>

namespace parser
{
  syntax_parser::syntax_parser(std::vector<token> tokens) : input(std::move(tokens)) {}

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
    std::vector<statement_ref> body;
    bool seen_module = false;
    bool seen_non_module = false;
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
      else if (match(tokens::KWD_FUN))
      {
        declaration = parse_function_declaration();
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
        throw parse_error("Only declarations are allowed at the top level", peek()->range);
      }
      if (dynamic_cast<module_declaration *>(declaration.get()) == nullptr)
      {
        seen_non_module = true;
      }
      declaration->documentation = std::move(documentation);
      body.push_back(std::move(declaration));
      if (!at_end())
      {
        expect(tokens::NEWLINE, "a newline after the declaration");
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
      if (!match(tokens::KWD_LET))
      {
        throw parse_error("A documentation comment inside a block must precede a variable declaration",
                          documentation.back().range);
      }
      auto declaration = parse_let_declaration();
      declaration->documentation = std::move(documentation);
      return declaration;
    }
    if (match(tokens::KWD_LET))
    {
      return parse_let_declaration();
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
    std::optional<std::string> type_name;
    if (match(tokens::COLON))
    {
      type_name = parse_type_annotation("a type name after ':'");
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

  auto syntax_parser::parse_function_declaration(const bool body_optional, const bool allow_private,
                                                  const bool allow_mutating) -> statement_ref
  {
    const token &keyword = previous();
    const bool private_member = allow_private && match(tokens::DOT);
    const token &name = allow_mutating && match(tokens::METHOD_IDENTIFIER)
                            ? previous()
                            : expect(tokens::IDENTIFIER, "a function name after 'fun'");
    std::vector<std::string> type_parameters;
    if (match(tokens::LANGLE))
    {
      do
      {
        type_parameters.push_back(expect(tokens::IDENTIFIER, "a generic type parameter").text);
      } while (match(tokens::COMMA));
      expect(tokens::RANGLE, "'>' after generic type parameters");
    }
    if (!type_parameters.empty() && (body_optional || allow_private || allow_mutating))
      throw parse_error("Generic methods are not implemented yet", name.range);
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
      parameters.emplace_back(parameter_name.text, std::move(parameter_type));
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
                                                  std::move(parameters), std::move(return_type),
                                                  std::move(body), std::move(expression_body));
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
      parameters.emplace_back(parameter_name.text,
                              parse_type_annotation("a constructor parameter type after ':'"));
      if (!match(tokens::COMMA) || check(tokens::RPAREN)) break;
    }
    expect(tokens::RPAREN, "')' after the constructor parameters");
    auto body = parse_block();
    return std::make_unique<function_declaration>(span{keyword.range.begin, body->range.end}, "new", false, true,
                                                  std::vector<std::string>{},
                                                  std::move(parameters), std::optional<std::string>{"Void"},
                                                  std::move(body), nullptr);
  }

  auto syntax_parser::parse_type_declaration(const type_declaration::kind type) -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a type name after '" + keyword.text + "'");
    std::vector<std::string> type_parameters;
    if (match(tokens::LANGLE))
    {
      do
      {
        type_parameters.push_back(expect(tokens::IDENTIFIER, "a generic type parameter").text);
      } while (match(tokens::COMMA));
      expect(tokens::RANGLE, "'>' after generic type parameters");
    }
    std::optional<std::string> composition_keyword;
    std::vector<std::string> interfaces;
    if (match(tokens::KWD_IS) || match(tokens::KWD_HAS))
    {
      composition_keyword = previous().text;
      do
      {
        interfaces.push_back(expect(tokens::IDENTIFIER, "an interface name in the composition list").text);
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
        enum_members.emplace_back(member.text, span{member.range.begin, member_end},
                                  std::move(payload_types), std::move(documentation));
        if (match(tokens::COMMA))
        {
          skip_newlines();
          continue;
        }
      }
      else if (match(tokens::KWD_FUN))
      {
        const bool is_interface = type == type_declaration::kind::interface_type;
        auto member = parse_function_declaration(is_interface, !is_interface, true);
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type == type_declaration::kind::class_type && match(tokens::KWD_NEW))
      {
        auto member = parse_constructor_declaration();
        member->documentation = std::move(documentation);
        members.push_back(std::move(member));
      }
      else if (type == type_declaration::kind::class_type && match(tokens::KWD_LET))
      {
        auto member = parse_let_declaration(true);
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
                                              std::move(composition_keyword), std::move(interfaces),
                                              std::move(members), std::move(enum_members));
  }

  auto syntax_parser::parse_module_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a module name after 'module'");
    return std::make_unique<module_declaration>(span{keyword.range.begin, name.range.end}, name.text);
  }

  auto syntax_parser::parse_import_declaration() -> statement_ref
  {
    const token &keyword = previous();
    const token &name = expect(tokens::IDENTIFIER, "a name after 'import'");
    std::optional<std::string> source;
    if (match(tokens::KWD_FROM))
    {
      source = expect(tokens::IDENTIFIER, "a module name after 'from'").text;
    }
    std::optional<std::string> alias;
    if (match(tokens::KWD_AS))
    {
      alias = expect(tokens::IDENTIFIER, "an alias after 'as'").text;
    }
    return std::make_unique<import_declaration>(span{keyword.range.begin, previous().range.end}, name.text,
                                                std::move(source), std::move(alias));
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
    std::string result = expect(tokens::IDENTIFIER, description).text;
    if (!match(tokens::LANGLE)) return result;
    result += '<';
    do
    {
      if (result.back() != '<') result += ", ";
      result += parse_type_annotation("a type argument");
    } while (match(tokens::COMMA));
    expect(tokens::RANGLE, "'>' after type arguments");
    return result + '>';
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
                                match(tokens::GREATER_EQUAL) || match(tokens::KWD_IS);
    if (!has_comparison)
    {
      return left;
    }
    const token operation = previous();
    auto right = parse_additive();
    if (check(tokens::LANGLE) || check(tokens::LESS_EQUAL) ||
        (vector_literal_depth == 0 && check(tokens::RANGLE)) ||
        check(tokens::GREATER_EQUAL) || check(tokens::KWD_IS))
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
        value = std::make_unique<member_expression>(span{value->range.begin, member.range.end}, std::move(value),
                                                    member.text, safe);
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
    if (match(tokens::IDENTIFIER) || match(tokens::KWD_SELF))
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
        if (depth == 0 && scan < input.size() && input[scan].id == tokens::DOT)
        {
          advance();
          name += '<';
          bool first = true;
          do
          {
            if (!first) name += ", ";
            name += parse_type_annotation("a qualified generic type argument");
            first = false;
          } while (match(tokens::COMMA));
          end = expect(tokens::RANGLE, "'>' after qualified generic type arguments").range.end;
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
      parameters.emplace_back(name.text, std::move(type_name));
      if (!match(tokens::COMMA) || check(tokens::RPAREN))
      {
        break;
      }
    }
    expect(tokens::RPAREN, "')' after the lambda parameters");
    std::optional<std::string> return_type;
    if (match(tokens::COLON))
    {
      return_type = expect(tokens::IDENTIFIER, "a lambda return type after ':'").text;
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

  auto syntax_parser::parse_vector() -> expression_ref
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
    if (elements.size() < 2)
    {
      throw parse_error("A vector literal requires at least two elements", span{open.range.begin, close.range.end});
    }
    return std::make_unique<collection_expression>(span{open.range.begin, close.range.end},
                                                   collection_expression::kind::vector, std::move(elements));
  }

  auto syntax_parser::parse_parenthesized() -> expression_ref
  {
    const token &open = expect(tokens::LPAREN, "'(' to begin a grouped expression or coordinate");
    if (check(tokens::RPAREN))
    {
      throw parse_error("Empty parentheses are not an expression", peek()->range);
    }
    auto first = parse_nested_expression();
    if (!match(tokens::COMMA))
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
    const token &close = expect(tokens::RPAREN, "')' after the coordinate elements");
    if (elements.size() < 2)
    {
      throw parse_error("A coordinate literal requires at least two elements", span{open.range.begin, close.range.end});
    }
    return std::make_unique<collection_expression>(span{open.range.begin, close.range.end},
                                                   collection_expression::kind::coordinate, std::move(elements));
  }
}
