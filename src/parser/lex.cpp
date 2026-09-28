#include <regex>
#include "lex.hpp"

const std::regex IDENTIFIER("^[a-zA-Z_]\\w*");
const std::regex WHITESPACE("^\\s+");
const std::regex TAG("^@\\w+");
const std::regex EVENT("^!\\w+");
const std::regex NUMBER_DEC("^(\\.[0-9_]+|[0-9][0-9_]*(\\.[0-9_]+)?)\\b");
const std::regex NUMBER_HEX("^\\b0x[0-9a-fA-F_]*\\b");
const std::regex NUMBER_OCT("^0c[0-7]*\\b");
const std::regex NUMBER_BIN("^0b[01]+\\b");
const std::regex KWD_NODE("^node\\b");
const std::regex KWD_AS("^as\\b");
const std::regex KWD_IN("^in\\b");
const std::regex KWD_OUT("^out\\b");
const std::regex KWD_ON("^on\\b");
const std::regex KWD_INCLUDE("^include\\b");
const std::regex KWD_PARTIAL("^partial\\b");
const std::regex KWD_EXTENDS("^extends\\b");
const std::regex COMMENT("^//.*");
const std::regex COMMENT_MULTILINE("^/\\*[^*]*\\*+(?:[^/*][^*]*\\*+)*/", std::regex_constants::multiline);

#define CHAR_TOKEN(chr, id)                                                                        \
  if (c == chr)                                                                                    \
  {                                                                                                \
    return parser::token{tokens::id, parser::span{state.index, ++state.index}, std::string(1, c)}; \
  }

#define CHECK_TOKEN(id) CHECK_TOKEN_WITH(id, match.str())

#define CHECK_TOKEN_WITH(id, match_expr)      \
  if (std::regex_search(str, match, id))      \
  {                                           \
    const int old_index = state.index;        \
    state.index += match.length();            \
    return parser::token{                     \
        tokens::id,                           \
        parser::span{old_index, state.index}, \
        match_expr,                           \
    };                                        \
  }

#define CHECK_TOKEN_WITH_VALUE(id, out_id, match_expr) \
  if (std::regex_search(str, match, id))               \
  {                                                    \
    const int old_index = state.index;                 \
    state.index += match.length();                     \
    return parser::token{                              \
        tokens::out_id,                                \
        parser::span{old_index, state.index - 1},      \
        match_expr,                                    \
        std::stof(match_expr),                         \
    };                                                 \
  }

auto get_token(parser::programText &state) -> std::optional<parser::token>
{
  const auto &text = state.text;

  char read_until = '\0';
  int read_until_index = 0;

  while (text.length() > state.index)
  {
    char c = text[state.index];

    if (read_until)
    {
      state.index++;
      if (c == read_until)
      {
        return parser::token{
            c == '"' ? tokens::STRING : tokens::CODE,
            parser::span{read_until_index, state.index},
            text.substr(read_until_index + 1, state.index - read_until_index - 2),
        };
      }
      continue;
    }

    if (c == '`' || c == '"')
    {
      read_until = c;
      read_until_index = state.index++;
      continue;
    }

    CHAR_TOKEN('[', LBRACKET)
    CHAR_TOKEN(']', RBRACKET)
    CHAR_TOKEN('{', LBRACE)
    CHAR_TOKEN('}', RBRACE)
    CHAR_TOKEN(':', COLON)
    CHAR_TOKEN(',', COMMA)

    const char *str = text.c_str() + state.index;
    std::cmatch match;

    // Skip whitespace and comments
    if (std::regex_search(str, match, WHITESPACE) || std::regex_search(str, match, COMMENT))
    {
      state.index += match.length();
      continue;
    }

    CHECK_TOKEN_WITH(TAG, match.str().substr(1))
    CHECK_TOKEN_WITH(EVENT, match.str().substr(1))
    CHECK_TOKEN(KWD_NODE)
    CHECK_TOKEN(KWD_AS)
    CHECK_TOKEN(KWD_IN)
    CHECK_TOKEN(KWD_OUT)
    CHECK_TOKEN(KWD_ON)
    CHECK_TOKEN(KWD_INCLUDE)
    CHECK_TOKEN(KWD_PARTIAL)
    CHECK_TOKEN(KWD_EXTENDS)
    CHECK_TOKEN(IDENTIFIER)
    CHECK_TOKEN_WITH_VALUE(NUMBER_DEC, NUMBER, std::string(match.str()))

    if (std::regex_search(str, match, COMMENT_MULTILINE))
    {
      state.index += match.length();
      continue;
    }

    throw parser::parse_error(std::string("Unknown character `") + c + "`", {state.index, state.index++});
  }

  // Spit out any remaining string or code segments
  if (read_until)
  {
    state.index = text.length();
    return parser::token{
        read_until == '"' ? tokens::STRING : tokens::CODE,
        parser::span{read_until_index, state.index},
        text.substr(read_until_index + 1, state.index - read_until_index - 1),
    };
  }

  return {};
}
