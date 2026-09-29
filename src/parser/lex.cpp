#include "lex.hpp"
#include "unicode.hpp"

#include <cctype>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace
{
  using parser::programText;
  using parser::span;
  using parser::token;

  const std::unordered_map<std::string, int> keywords = {
      {"let", tokens::KWD_LET},
      {"weak", tokens::KWD_WEAK},
      {"fun", tokens::KWD_FUN},
      {"new", tokens::KWD_NEW},
      {"class", tokens::KWD_CLASS},
      {"face", tokens::KWD_FACE},
      {"enum", tokens::KWD_ENUM},
      {"if", tokens::KWD_IF},
      {"else", tokens::KWD_ELSE},
      {"match", tokens::KWD_MATCH},
      {"case", tokens::KWD_CASE},
      {"for", tokens::KWD_FOR},
      {"in", tokens::KWD_IN},
      {"while", tokens::KWD_WHILE},
      {"until", tokens::KWD_UNTIL},
      {"break", tokens::KWD_BREAK},
      {"continue", tokens::KWD_CONTINUE},
      {"return", tokens::KWD_RETURN},
      {"yield", tokens::KWD_YIELD},
      {"import", tokens::KWD_IMPORT},
      {"from", tokens::KWD_FROM},
      {"as", tokens::KWD_AS},
      {"module", tokens::KWD_MODULE},
      {"export", tokens::KWD_EXPORT},
      {"hope", tokens::KWD_HOPE},
      {"unless", tokens::KWD_UNLESS},
      {"finally", tokens::KWD_FINALLY},
      {"scream", tokens::KWD_SCREAM},
      {"and", tokens::KWD_AND},
      {"or", tokens::KWD_OR},
      {"not", tokens::KWD_NOT},
      {"self", tokens::KWD_SELF},
      {"is", tokens::KWD_IS},
      {"has", tokens::KWD_HAS},
      {"true", tokens::KWD_TRUE},
      {"false", tokens::KWD_FALSE},
      {"inf", tokens::KWD_INF},
      {"nan", tokens::KWD_NAN},
  };

  auto starts_with(const programText &state, const std::string_view value) -> bool
  {
    return state.text.compare(static_cast<std::size_t>(state.index), value.size(), value) == 0;
  }

  auto make_token(programText &state, const int id, const int begin, const int end, std::string text,
                  const double value = 0.0, const bool marks_line = true) -> token
  {
    if (marks_line)
    {
      state.line_has_token = true;
      if (id == tokens::LPAREN)
      {
        state.parenthesis_depth++;
      }
      else if (id == tokens::RPAREN && state.parenthesis_depth > 0)
      {
        state.parenthesis_depth--;
      }
      else if (id == tokens::LBRACKET)
      {
        state.bracket_depth++;
      }
      else if (id == tokens::RBRACKET && state.bracket_depth > 0)
      {
        state.bracket_depth--;
      }

      switch (id)
      {
      case tokens::COMMA:
      case tokens::COLON:
      case tokens::DOT:
      case tokens::QUESTION:
      case tokens::SEMICOLON:
      case tokens::PLUS:
      case tokens::MINUS:
      case tokens::STAR:
      case tokens::SLASH:
      case tokens::PERCENT:
      case tokens::CARET:
      case tokens::EQUAL:
      case tokens::BANG:
      case tokens::SPREAD:
      case tokens::SAFE_DOT:
      case tokens::ASSIGN_VALUE:
      case tokens::FAT_ARROW:
      case tokens::EQUAL_EQUAL:
      case tokens::BANG_EQUAL:
      case tokens::LESS_EQUAL:
      case tokens::GREATER_EQUAL:
      case tokens::PLUS_EQUAL:
      case tokens::MINUS_EQUAL:
      case tokens::STAR_EQUAL:
      case tokens::SLASH_EQUAL:
      case tokens::PERCENT_EQUAL:
      case tokens::CARET_EQUAL:
      case tokens::KWD_AND:
      case tokens::KWD_OR:
      case tokens::KWD_NOT:
        state.newline_continuation = true;
        break;
      default:
        state.newline_continuation = false;
        break;
      }
    }
    return token{id, span{begin, end}, std::move(text), value};
  }

  [[noreturn]] auto fail(const std::string &message, const int begin, const int end) -> void
  {
    throw parser::parse_error(message, span{begin, end});
  }

  auto identifier_start_length(const programText &state, const int offset) -> int
  {
    const auto current = sagan::unicode::decode(state.text, static_cast<std::size_t>(offset));
    if (!current)
    {
      return 0; // LCOV_EXCL_LINE - source-wide UTF-8 validation runs before identifier scanning.
    }
    if (current->value == U'_' || sagan::unicode::is_xid_start(current->value))
    {
      return static_cast<int>(current->width);
    }
    return static_cast<int>(sagan::unicode::emoji_sequence_length(state.text, static_cast<std::size_t>(offset)));
  }

  auto identifier_continue_length(const programText &state, const int offset) -> int
  {
    const auto current = sagan::unicode::decode(state.text, static_cast<std::size_t>(offset));
    if (!current)
    {
      return 0; // LCOV_EXCL_LINE - source-wide UTF-8 validation runs before identifier scanning.
    }
    if (current->value == U'_' ||
        (current->value != 0x200c && current->value != 0x200d &&
         sagan::unicode::is_xid_continue(current->value)))
    {
      return static_cast<int>(current->width);
    }
    return static_cast<int>(sagan::unicode::emoji_sequence_length(state.text, static_cast<std::size_t>(offset)));
  }

  auto append_utf8(std::string &out, const unsigned int codepoint) -> void
  {
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
    {
      fail("Unicode escape is outside the valid code-point range", 0, 0);
    }
    if (codepoint <= 0x7f)
    {
      out.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7ff)
    {
      out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    else if (codepoint <= 0xffff)
    {
      out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    else
    {
      out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
  }

  auto scan_escape(programText &state, std::string &value) -> void
  {
    const int escape_begin = state.index++;
    if (state.index >= static_cast<int>(state.text.size()))
    {
      fail("Unterminated escape sequence", escape_begin, state.index);
    }

    const char escaped = state.text[state.index++];
    switch (escaped)
    {
    case '\\':
      value.push_back('\\');
      return;
    case '"':
      value.push_back('"');
      return;
    case '\'':
      value.push_back('\'');
      return;
    case 'n':
      value.push_back('\n');
      return;
    case 'r':
      value.push_back('\r');
      return;
    case 't':
      value.push_back('\t');
      return;
    case '0':
      value.push_back('\0');
      return;
    case 'u':
      break;
    default:
      fail(std::string("Unknown escape sequence \\") + escaped, escape_begin, state.index);
    }

    if (state.index >= static_cast<int>(state.text.size()) || state.text[state.index] != '{')
    {
      fail("Unicode escapes use \\u{...}", escape_begin, state.index);
    }
    state.index++;

    const int digits_begin = state.index;
    unsigned int codepoint = 0;
    while (state.index < static_cast<int>(state.text.size()) && state.text[state.index] != '}')
    {
      const unsigned char digit = static_cast<unsigned char>(state.text[state.index]);
      if (!std::isxdigit(digit))
      {
        fail("Unicode escape contains a non-hexadecimal digit", escape_begin, state.index + 1);
      }
      if (state.index - digits_begin >= 6)
      {
        fail("Unicode escapes contain at most six hexadecimal digits", escape_begin, state.index + 1);
      }
      codepoint *= 16;
      if (digit >= '0' && digit <= '9')
      {
        codepoint += digit - '0';
      }
      else
      {
        codepoint += static_cast<unsigned int>(std::tolower(digit) - 'a' + 10);
      }
      state.index++;
    }

    if (state.index == digits_begin || state.index >= static_cast<int>(state.text.size()))
    {
      fail("Unterminated Unicode escape", escape_begin, state.index);
    }
    state.index++;
    append_utf8(value, codepoint);
  }

  auto scan_string_body(programText &state) -> token
  {
    const int segment_begin = state.index;
    std::string value;

    while (state.index < static_cast<int>(state.text.size()))
    {
      const bool at_close =
          state.string_multiline ? starts_with(state, "\"\"\"") : state.text[state.index] == state.string_quote;

      if (at_close)
      {
        const int close_begin = state.index;
        const int close_size = state.string_multiline ? 3 : 1;
        state.index += close_size;
        state.in_string = false;
        token end = make_token(state, tokens::STRING_END, close_begin, state.index,
                               state.text.substr(static_cast<std::size_t>(close_begin), close_size));
        if (!value.empty())
        {
          state.pending.push_back(std::move(end));
          return make_token(state, tokens::STRING_SEGMENT, segment_begin, close_begin, std::move(value));
        }
        return end;
      }

      if (starts_with(state, "${"))
      {
        const int interpolation_begin = state.index;
        state.index += 2;
        state.in_string = false;
        state.string_interpolated = true;
        state.interpolation_depth = 1;
        state.string_stack.push_back(
            {state.string_multiline, state.string_quote, state.string_open_index});
        token begin = make_token(state, tokens::INTERPOLATION_BEGIN, interpolation_begin, state.index, "${");
        if (!value.empty())
        {
          state.pending.push_back(std::move(begin));
          return make_token(state, tokens::STRING_SEGMENT, segment_begin, interpolation_begin, std::move(value));
        }
        return begin;
      }

      const char c = state.text[state.index];
      if (!state.string_multiline && (c == '\n' || c == '\r'))
      {
        fail("Unterminated string literal", state.string_open_index, state.index);
      }
      if (c == '\\')
      {
        scan_escape(state, value);
      }
      else
      {
        value.push_back(c);
        state.index++;
      }
    }

    fail("Unterminated string literal", state.string_open_index, state.index);
  }

  auto begin_string(programText &state, const bool raw) -> token
  {
    const int begin = state.index;
    if (raw)
    {
      state.index++;
    }

    const bool multiline = starts_with(state, "\"\"\"");
    const char quote = state.text[state.index];
    const int delimiter_size = multiline ? 3 : 1;
    state.index += delimiter_size;

    if (raw)
    {
      const int content_begin = state.index;
      while (state.index < static_cast<int>(state.text.size()))
      {
        const bool at_close =
            multiline ? starts_with(state, "\"\"\"") : state.text[state.index] == quote;
        if (at_close)
        {
          const int content_end = state.index;
          state.index += delimiter_size;
          return make_token(state, tokens::STRING, begin, state.index,
                            state.text.substr(static_cast<std::size_t>(content_begin),
                                              static_cast<std::size_t>(content_end - content_begin)));
        }
        if (!multiline && (state.text[state.index] == '\n' || state.text[state.index] == '\r'))
        {
          fail("Unterminated raw string literal", begin, state.index);
        }
        state.index++;
      }
      fail("Unterminated raw string literal", begin, state.index);
    }

    state.in_string = true;
    state.string_raw = false;
    state.string_multiline = multiline;
    state.string_interpolated = false;
    state.string_quote = quote;
    state.string_open_index = begin;
    return make_token(state, tokens::STRING_BEGIN, begin, state.index,
                      state.text.substr(static_cast<std::size_t>(begin),
                                        static_cast<std::size_t>(state.index - begin)));
  }

  auto scan_block_comment(programText &state, const bool documentation) -> std::optional<token>
  {
    const int begin = state.index;
    const int marker_size = documentation ? 3 : 2;
    state.index += marker_size;
    const int content_begin = state.index;
    int depth = 1;

    while (state.index < static_cast<int>(state.text.size()) && depth > 0)
    {
      if (starts_with(state, "/*"))
      {
        depth++;
        state.index += 2;
      }
      else if (starts_with(state, "*/"))
      {
        depth--;
        if (depth == 0)
        {
          const int content_end = state.index;
          state.index += 2;
          if (documentation)
          {
            return make_token(state, tokens::DOC_COMMENT, begin, state.index,
                              state.text.substr(static_cast<std::size_t>(content_begin),
                                                static_cast<std::size_t>(content_end - content_begin)));
          }
          return {};
        }
        state.index += 2;
      }
      else
      {
        state.index++;
      }
    }

    fail(documentation ? "Unterminated documentation comment" : "Unterminated block comment", begin, state.index);
  }

  auto scan_number(programText &state) -> token
  {
    const int begin = state.index;
    bool floating = false;

    const auto scan_digits = [&state, begin]() {
      while (state.index < static_cast<int>(state.text.size()))
      {
        const unsigned char c = static_cast<unsigned char>(state.text[state.index]);
        if (std::isdigit(c))
        {
          state.index++;
          continue;
        }
        if (c == '_')
        {
          const bool valid = state.index > begin &&
                             std::isdigit(static_cast<unsigned char>(state.text[state.index - 1])) != 0 &&
                             state.index + 1 < static_cast<int>(state.text.size()) &&
                             std::isdigit(static_cast<unsigned char>(state.text[state.index + 1])) != 0;
          if (!valid)
          {
            fail("Digit separators must appear between digits", begin, state.index + 1);
          }
          state.index++;
          continue;
        }
        break;
      }
    };

    scan_digits();
    if (state.index < static_cast<int>(state.text.size()) && state.text[state.index] == '.')
    {
      if (state.index + 1 >= static_cast<int>(state.text.size()) ||
          std::isdigit(static_cast<unsigned char>(state.text[state.index + 1])) == 0)
      {
        fail("A decimal point requires digits on both sides", begin, state.index + 1);
      }
      floating = true;
      state.index++;
      scan_digits();
    }

    if (state.index < static_cast<int>(state.text.size()) &&
        (state.text[state.index] == 'e' || state.text[state.index] == 'E'))
    {
      floating = true;
      state.index++;
      if (state.index < static_cast<int>(state.text.size()) &&
          (state.text[state.index] == '+' || state.text[state.index] == '-'))
      {
        state.index++;
      }
      if (state.index >= static_cast<int>(state.text.size()) ||
          std::isdigit(static_cast<unsigned char>(state.text[state.index])) == 0)
      {
        fail("Scientific notation requires exponent digits", begin, state.index);
      }
      scan_digits();
    }

    if (state.index < static_cast<int>(state.text.size()) && identifier_continue_length(state, state.index) > 0)
    {
      int end = state.index;
      while (end < static_cast<int>(state.text.size()))
      {
        const int length = identifier_continue_length(state, end);
        if (length == 0)
        {
          break;
        }
        end += length;
      }
      fail("Malformed numeric literal", begin, end);
    }

    std::string text = state.text.substr(static_cast<std::size_t>(begin),
                                         static_cast<std::size_t>(state.index - begin));
    std::string normalized;
    for (const char c : text)
    {
      if (c != '_')
      {
        normalized.push_back(c);
      }
    }
    return make_token(state, floating ? tokens::FLOAT : tokens::INTEGER, begin, state.index, text,
                      std::strtod(normalized.c_str(), nullptr));
  }

  auto scan_identifier(programText &state) -> token
  {
    const int begin = state.index;
    state.index += identifier_start_length(state, state.index);
    while (state.index < static_cast<int>(state.text.size()))
    {
      const int length = identifier_continue_length(state, state.index);
      if (length == 0)
      {
        break;
      }
      state.index += length;
    }

    bool method = false;
    if (state.index < static_cast<int>(state.text.size()) && state.text[state.index] == '!' &&
        (state.index + 1 >= static_cast<int>(state.text.size()) || state.text[state.index + 1] != '='))
    {
      method = true;
      state.index++;
    }

    const int identifier_end = method ? state.index - 1 : state.index;
    std::string text = sagan::unicode::normalize_nfc(
        std::string_view(state.text).substr(static_cast<std::size_t>(begin),
                                            static_cast<std::size_t>(identifier_end - begin)));
    if (method)
    {
      text.push_back('!');
    }
    if (!method)
    {
      const auto found = keywords.find(text);
      if (found != keywords.end())
      {
        return make_token(state, found->second, begin, state.index, std::move(text));
      }
    }
    return make_token(state, method ? tokens::METHOD_IDENTIFIER : tokens::IDENTIFIER, begin, state.index,
                      std::move(text));
  }

  auto match_operator(programText &state) -> std::optional<token>
  {
    struct operator_entry
    {
      std::string_view text;
      int id;
    };

    static constexpr operator_entry operators[] = {
        {"...", tokens::SPREAD},
        {"?.", tokens::SAFE_DOT},
        {"??", tokens::COALESCE},
        {":=", tokens::ASSIGN_VALUE},
        {"=>", tokens::FAT_ARROW},
        {"==", tokens::EQUAL_EQUAL},
        {"!=", tokens::BANG_EQUAL},
        {"<=", tokens::LESS_EQUAL},
        {">=", tokens::GREATER_EQUAL},
        {"++", tokens::PLUS_PLUS},
        {"--", tokens::MINUS_MINUS},
        {"+=", tokens::PLUS_EQUAL},
        {"-=", tokens::MINUS_EQUAL},
        {"*=", tokens::STAR_EQUAL},
        {"/=", tokens::SLASH_EQUAL},
        {"%=", tokens::PERCENT_EQUAL},
        {"^=", tokens::CARET_EQUAL},
    };

    for (const auto &entry : operators)
    {
      if (starts_with(state, entry.text))
      {
        const int begin = state.index;
        state.index += static_cast<int>(entry.text.size());
        return make_token(state, entry.id, begin, state.index, std::string(entry.text));
      }
    }
    return {};
  }

  auto scan_single_character(programText &state) -> std::optional<token>
  {
    const int begin = state.index;
    const char c = state.text[state.index++];
    int id = tokens::UNKNOWN;

    switch (c)
    {
    case '(':
      id = tokens::LPAREN;
      break;
    case ')':
      id = tokens::RPAREN;
      break;
    case '[':
      id = tokens::LBRACKET;
      break;
    case ']':
      id = tokens::RBRACKET;
      break;
    case '{':
      id = tokens::LBRACE;
      if (state.interpolation_depth > 0)
      {
        state.interpolation_depth++;
      }
      break;
    case '}':
      if (state.interpolation_depth == 1)
      {
        state.interpolation_depth = 0;
        if (state.string_stack.empty())
        {
          fail("Interpolation ended without a containing string", begin, state.index); // LCOV_EXCL_LINE
        }
        const auto context = state.string_stack.back();
        state.string_stack.pop_back();
        state.string_multiline = context.multiline;
        state.string_quote = context.quote;
        state.string_open_index = context.open_index;
        state.in_string = true;
        return make_token(state, tokens::INTERPOLATION_END, begin, state.index, "}");
      }
      if (state.interpolation_depth > 1)
      {
        state.interpolation_depth--;
      }
      id = tokens::RBRACE;
      break;
    case '<':
      id = tokens::LANGLE;
      break;
    case '>':
      id = tokens::RANGLE;
      break;
    case ',':
      id = tokens::COMMA;
      break;
    case ':':
      id = tokens::COLON;
      break;
    case '.':
      id = tokens::DOT;
      break;
    case '?':
      id = tokens::QUESTION;
      break;
    case ';':
      id = tokens::SEMICOLON;
      break;
    case '+':
      id = tokens::PLUS;
      break;
    case '-':
      id = tokens::MINUS;
      break;
    case '*':
      id = tokens::STAR;
      break;
    case '/':
      id = tokens::SLASH;
      break;
    case '%':
      id = tokens::PERCENT;
      break;
    case '^':
      id = tokens::CARET;
      break;
    case '=':
      id = tokens::EQUAL;
      break;
    case '!':
      id = tokens::BANG;
      break;
    default:
      state.index = begin;
      return {};
    }

    return make_token(state, id, begin, state.index, std::string(1, c));
  }
}

auto get_token(parser::programText &state) -> std::optional<parser::token>
{
  if (!state.source_validated)
  {
    state.source_validated = true;
    if (const auto invalid = sagan::unicode::first_invalid_utf8(state.text))
    {
      fail("Source contains malformed UTF-8", static_cast<int>(*invalid), static_cast<int>(*invalid + 1));
    }
  }

  if (!state.pending.empty())
  {
    token next = std::move(state.pending.front());
    state.pending.pop_front();
    return next;
  }

  if (state.in_string)
  {
    return scan_string_body(state);
  }

  while (state.index < static_cast<int>(state.text.size()))
  {
    const unsigned char c = static_cast<unsigned char>(state.text[state.index]);

    if (c == ' ' || c == '\t' || c == '\f' || c == '\v')
    {
      state.index++;
      continue;
    }

    if (c == '\r' || c == '\n')
    {
      const int begin = state.index;
      if (c == '\r')
      {
        if (state.index + 1 >= static_cast<int>(state.text.size()) || state.text[state.index + 1] != '\n')
        {
          fail("A carriage return must be followed by a line feed", begin, begin + 1);
        }
        state.index += 2;
      }
      else
      {
        state.index++;
      }

      if (state.parenthesis_depth > 0 || state.bracket_depth > 0 || state.newline_continuation)
      {
        continue;
      }

      if (state.line_has_token)
      {
        state.line_has_token = false;
        return make_token(state, tokens::NEWLINE, begin, state.index, "\n", 0.0, false);
      }
      continue;
    }

    if (starts_with(state, "///"))
    {
      const int begin = state.index;
      state.index += 3;
      if (state.index < static_cast<int>(state.text.size()) && state.text[state.index] == ' ')
      {
        state.index++;
      }
      const int content_begin = state.index;
      while (state.index < static_cast<int>(state.text.size()) && state.text[state.index] != '\n' &&
             state.text[state.index] != '\r')
      {
        state.index++;
      }
      return make_token(state, tokens::DOC_COMMENT, begin, state.index,
                        state.text.substr(static_cast<std::size_t>(content_begin),
                                          static_cast<std::size_t>(state.index - content_begin)));
    }

    if (starts_with(state, "/**"))
    {
      return scan_block_comment(state, true);
    }

    if (starts_with(state, "//"))
    {
      state.index += 2;
      while (state.index < static_cast<int>(state.text.size()) && state.text[state.index] != '\n' &&
             state.text[state.index] != '\r')
      {
        state.index++;
      }
      continue;
    }

    if (starts_with(state, "/*"))
    {
      scan_block_comment(state, false);
      continue;
    }

    if (c == 'r' && state.index + 1 < static_cast<int>(state.text.size()) &&
        (state.text[state.index + 1] == '"' || state.text[state.index + 1] == '\''))
    {
      return begin_string(state, true);
    }

    if (c == '"' || c == '\'')
    {
      return begin_string(state, false);
    }

    if (std::isdigit(c) != 0)
    {
      return scan_number(state);
    }

    if (c == '.' && state.index + 1 < static_cast<int>(state.text.size()) &&
        std::isdigit(static_cast<unsigned char>(state.text[state.index + 1])) != 0)
    {
      fail("A decimal point requires digits on both sides", state.index, state.index + 1);
    }

    if (identifier_start_length(state, state.index) > 0)
    {
      return scan_identifier(state);
    }

    if (auto op = match_operator(state))
    {
      return op;
    }

    if (auto single = scan_single_character(state))
    {
      return single;
    }

    const int begin = state.index++;
    fail(std::string("Unknown character '") + static_cast<char>(c) + "'", begin, state.index);
  }

  if (state.interpolation_depth > 0)
  {
    fail("Unterminated string interpolation", state.string_open_index, state.index);
  }

  if (state.line_has_token && !state.emitted_final_newline)
  {
    state.line_has_token = false;
    state.emitted_final_newline = true;
    return make_token(state, tokens::NEWLINE, state.index, state.index, "\n", 0.0, false);
  }

  return {};
}
