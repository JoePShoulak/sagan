#include "formatter.hpp"
#include "../syntax/syntax.hpp"
#include "../parser/tokens.hpp"

#include <algorithm>
#include <optional>
#include <string_view>

namespace sagan::language_service
{
  namespace
  {
    auto protected_line(const syntax::syntax_document &syntax, const source::byte_offset begin) -> bool
    {
      const auto crossing = [begin](const source::byte_range span)
      { return span.begin < begin && begin < span.end; };
      for (const auto &token : syntax.tokens)
      {
        if ((token.kind == tokens::STRING || token.kind == tokens::STRING_BEGIN ||
             token.kind == tokens::STRING_SEGMENT || token.kind == tokens::STRING_END ||
             token.kind == tokens::DOC_COMMENT) && crossing(token.range)) return true;
        for (const auto &trivia : token.leading_trivia)
          if (trivia.kind != syntax::trivia_kind::whitespace && crossing(trivia.range)) return true;
      }
      for (const auto &trivia : syntax.trailing_trivia)
        if (trivia.kind != syntax::trivia_kind::whitespace && crossing(trivia.range)) return true;
      return false;
    }

    auto matching_tokens(const syntax::syntax_document &before,
                         const syntax::syntax_document &after) -> bool
    {
      if (before.tokens.size() != after.tokens.size()) return false;
      for (std::size_t i = 0; i < before.tokens.size(); ++i)
        if (before.tokens[i].kind != after.tokens[i].kind ||
            before.tokens[i].source_text != after.tokens[i].source_text) return false;
      return true;
    }

    auto same_diagnostics(const std::vector<diagnostics::diagnostic> &before,
                          const std::vector<diagnostics::diagnostic> &after) -> bool
    {
      if (before.size() != after.size()) return false;
      for (std::size_t index = 0; index < before.size(); ++index)
        if (before[index].code != after[index].code ||
            before[index].owner != after[index].owner ||
            before[index].message != after[index].message) return false;
      return true;
    }

    auto unfinished_line_end(const int kind) -> bool
    {
      return kind == tokens::EQUAL || kind == tokens::ASSIGN_VALUE ||
             kind == tokens::FAT_ARROW || kind == tokens::COMMA ||
             kind == tokens::DOT || kind == tokens::SAFE_DOT ||
             kind == tokens::COLON || kind == tokens::LPAREN ||
             kind == tokens::LBRACKET || kind == tokens::LANGLE ||
             kind == tokens::PLUS || kind == tokens::MINUS ||
             kind == tokens::STAR || kind == tokens::SLASH ||
             kind == tokens::CARET || kind == tokens::PERCENT;
    }

    auto safe_recovered_line(const syntax::syntax_document &syntax,
                             const std::vector<diagnostics::diagnostic> &issues,
                             const source::byte_offset begin,
                             const source::byte_offset end) -> bool
    {
      for (const auto &issue : issues)
        if (begin <= issue.primary.bytes.begin && issue.primary.bytes.begin <= end)
          return false;
      int last = tokens::UNKNOWN;
      int parentheses = 0;
      int brackets = 0;
      for (const auto &token : syntax.tokens)
      {
        if (token.range.end <= begin || token.range.begin >= end) continue;
        if (token.error_token || token.range.begin < begin || token.range.end > end)
          return false;
        if (token.kind == tokens::NEWLINE) continue;
        last = token.kind;
        if (token.kind == tokens::LPAREN) ++parentheses;
        else if (token.kind == tokens::RPAREN) --parentheses;
        else if (token.kind == tokens::LBRACKET) ++brackets;
        else if (token.kind == tokens::RBRACKET) --brackets;
      }
      if (parentheses != 0 || brackets != 0 || unfinished_line_end(last)) return false;
      for (const auto &token : syntax.tokens)
        for (const auto &trivia : token.leading_trivia)
          if (trivia.kind == syntax::trivia_kind::skipped_text &&
              trivia.range.begin < end && trivia.range.end > begin) return false;
      for (const auto &trivia : syntax.trailing_trivia)
        if (trivia.kind == syntax::trivia_kind::skipped_text &&
            trivia.range.begin < end && trivia.range.end > begin) return false;
      return true;
    }

    auto protected_space(const syntax::syntax_document &syntax, const source::byte_offset offset) -> bool
    {
      const auto inside = [offset](const source::byte_range range)
      { return range.begin < offset && offset < range.end; };
      for (const auto &token : syntax.tokens)
      {
        if ((token.kind == tokens::STRING || token.kind == tokens::STRING_BEGIN ||
             token.kind == tokens::STRING_SEGMENT || token.kind == tokens::STRING_END ||
             token.kind == tokens::DOC_COMMENT) && inside(token.range)) return true;
        for (const auto &trivia : token.leading_trivia)
          if (trivia.kind != syntax::trivia_kind::whitespace && inside(trivia.range)) return true;
      }
      for (const auto &trivia : syntax.trailing_trivia)
        if (trivia.kind != syntax::trivia_kind::whitespace && inside(trivia.range)) return true;
      return false;
    }

    auto ends_expression(const int kind) -> bool
    {
      return kind == tokens::IDENTIFIER || kind == tokens::METHOD_IDENTIFIER ||
             kind == tokens::INTEGER || kind == tokens::FLOAT || kind == tokens::STRING ||
             kind == tokens::STRING_END || kind == tokens::RPAREN || kind == tokens::RBRACKET ||
             kind == tokens::RBRACE || kind == tokens::RANGLE ||
             kind == tokens::KWD_TRUE || kind == tokens::KWD_FALSE ||
             kind == tokens::KWD_INF || kind == tokens::KWD_NAN || kind == tokens::KWD_SELF;
    }

    auto canonical_gap(const int prior, const int left, const int right)
      -> std::optional<std::string_view>
    {
      if (left == tokens::NEWLINE || right == tokens::NEWLINE) return {};
      if (right == tokens::COMMA || right == tokens::COLON || right == tokens::RPAREN ||
          right == tokens::RBRACKET || left == tokens::LPAREN || left == tokens::LBRACKET)
        return "";
      if (left == tokens::COMMA || left == tokens::COLON) return " ";
      if (right == tokens::DOT &&
          (left == tokens::KWD_LET || left == tokens::KWD_CONST || left == tokens::KWD_FUN))
        return " ";
      if (left == tokens::DOT || left == tokens::SAFE_DOT || right == tokens::DOT ||
          right == tokens::SAFE_DOT) return "";
      if (right == tokens::LPAREN &&
          (left == tokens::IDENTIFIER || left == tokens::METHOD_IDENTIFIER ||
           left == tokens::RPAREN || left == tokens::RBRACKET)) return "";
      if ((left == tokens::PLUS || left == tokens::MINUS) && ends_expression(prior)) return " ";
      if (right == tokens::PLUS || right == tokens::MINUS)
      {
        if (ends_expression(left) || left == tokens::EQUAL || left == tokens::ASSIGN_VALUE ||
            left == tokens::FAT_ARROW || left == tokens::COMMA || left == tokens::COLON ||
            left == tokens::KWD_RETURN || left == tokens::KWD_YIELD ||
            left == tokens::KWD_SCREAM || left == tokens::KWD_AND ||
            left == tokens::KWD_OR) return " ";
        if (left == tokens::LPAREN || left == tokens::LBRACKET) return "";
        return {};
      }
      if (left == tokens::PLUS || left == tokens::MINUS) return "";
      if (left == tokens::STAR || right == tokens::STAR ||
          left == tokens::SLASH || right == tokens::SLASH ||
          left == tokens::PERCENT || right == tokens::PERCENT ||
          left == tokens::CARET || right == tokens::CARET ||
          left == tokens::KWD_AND || right == tokens::KWD_AND ||
          left == tokens::KWD_OR || right == tokens::KWD_OR ||
          left == tokens::KWD_IN || right == tokens::KWD_IN ||
          left == tokens::KWD_IS || right == tokens::KWD_IS ||
          left == tokens::KWD_HAS || right == tokens::KWD_HAS ||
          left == tokens::KWD_NOT) return " ";
      if (right == tokens::LBRACE &&
          (ends_expression(left) || left == tokens::KWD_ELSE ||
           left == tokens::KWD_FINALLY)) return " ";
      if (left == tokens::FAT_ARROW || right == tokens::FAT_ARROW ||
          left == tokens::EQUAL || right == tokens::EQUAL ||
          left == tokens::ASSIGN_VALUE || right == tokens::ASSIGN_VALUE ||
          left == tokens::PLUS_EQUAL || right == tokens::PLUS_EQUAL ||
          left == tokens::MINUS_EQUAL || right == tokens::MINUS_EQUAL ||
          left == tokens::STAR_EQUAL || right == tokens::STAR_EQUAL ||
          left == tokens::SLASH_EQUAL || right == tokens::SLASH_EQUAL ||
          left == tokens::PERCENT_EQUAL || right == tokens::PERCENT_EQUAL ||
          left == tokens::CARET_EQUAL || right == tokens::CARET_EQUAL ||
          left == tokens::EQUAL_EQUAL || right == tokens::EQUAL_EQUAL ||
          left == tokens::BANG_EQUAL || right == tokens::BANG_EQUAL ||
          left == tokens::COALESCE || right == tokens::COALESCE)
        return " ";
      return {};
    }

    auto format_lines(const source::document_snapshot &document, const source::byte_range requested)
      -> format_result
    {
      if (requested.begin > requested.end || !document.to_utf16(requested.begin) ||
          !document.to_utf16(requested.end))
        return {edit_state::invalid, "Formatting range is not a source boundary", {}};
      const auto analyzed = syntax::analyze(document, {.recover = true});
      if (!analyzed.value)
        return {edit_state::unsupported, "Source cannot be analyzed safely", {}};
      const bool recovered = !analyzed.value->strict_ast;
      const auto text = document.text();
      versioned_document_edits changes{document.identity().uri, document.version(), {}};
      std::vector<source::byte_range> selected_lines;
      std::size_t line_begin = 0;
      int depth = 0;
      bool selected_safe_line = false;
      while (line_begin < text.size())
      {
        auto line_end = text.find_first_of("\r\n", line_begin);
        if (line_end == std::string_view::npos) line_end = text.size();
        const auto content_end = line_end;
        auto content_begin = line_begin;
        while (content_begin < content_end && (text[content_begin] == ' ' || text[content_begin] == '\t'))
          ++content_begin;
        const bool protected_content = protected_line(*analyzed.value,
                                                       static_cast<source::byte_offset>(line_begin));
        const bool selected = line_begin < requested.end && line_end >= requested.begin;
        const bool safe = !recovered || safe_recovered_line(
            *analyzed.value, analyzed.diagnostics, static_cast<source::byte_offset>(line_begin),
            static_cast<source::byte_offset>(content_end));
        if (selected && safe)
        {
          selected_safe_line = true;
          selected_lines.push_back({static_cast<source::byte_offset>(line_begin),
                                    static_cast<source::byte_offset>(content_end)});
        }
        int line_depth = depth;
        const auto first_token = std::find_if(analyzed.value->tokens.begin(), analyzed.value->tokens.end(),
                                              [&](const auto &token)
                                              {
                                                return token.range.begin >= content_begin &&
                                                       token.range.begin < content_end &&
                                                       token.kind != tokens::NEWLINE;
                                              });
        if (first_token != analyzed.value->tokens.end() && first_token->kind == tokens::RBRACE)
          line_depth = std::max(0, depth - 1);
        if (selected && safe && !protected_content && content_begin < content_end)
        {
          const std::string indentation(static_cast<std::size_t>(line_depth) * 2, ' ');
          if (text.substr(line_begin, content_begin - line_begin) != indentation)
            changes.edits.push_back({{document.identity().id,
                                      {static_cast<source::byte_offset>(line_begin),
                                       static_cast<source::byte_offset>(content_begin)}}, indentation});
        }
        if (selected && safe && !protected_content)
        {
          auto trailing_begin = content_end;
          while (trailing_begin > line_begin &&
                 (text[trailing_begin - 1] == ' ' || text[trailing_begin - 1] == '\t'))
            --trailing_begin;
          if (trailing_begin < content_end &&
              !protected_space(*analyzed.value, static_cast<source::byte_offset>(trailing_begin)))
            changes.edits.push_back({{document.identity().id,
                                      {static_cast<source::byte_offset>(trailing_begin),
                                       static_cast<source::byte_offset>(content_end)}}, ""});
        }
        for (const auto &token : analyzed.value->tokens)
          if (token.range.begin >= line_begin && token.range.begin < line_end)
          {
            if (token.kind == tokens::LBRACE) ++depth;
            else if (token.kind == tokens::RBRACE) depth = std::max(0, depth - 1);
          }
        if (line_end == text.size()) break;
        line_begin = line_end + (text[line_end] == '\r' && line_end + 1 < text.size() &&
                                 text[line_end + 1] == '\n' ? 2 : 1);
      }
      if (recovered && !selected_safe_line)
        return {edit_state::unsupported, "No complete region has proven token ownership", {}};
      for (std::size_t i = 1; i < analyzed.value->tokens.size(); ++i)
      {
        const auto &left = analyzed.value->tokens[i - 1];
        const auto &right = analyzed.value->tokens[i];
        const auto begin = left.range.end;
        const auto end = right.range.begin;
        if (begin > end ||
            !std::any_of(selected_lines.begin(), selected_lines.end(), [&](const auto &line)
                         { return line.begin <= begin && end <= line.end; })) continue;
        const auto gap = text.substr(begin, end - begin);
        if (!std::all_of(gap.begin(), gap.end(), [](const char byte)
            { return byte == ' ' || byte == '\t'; })) continue;
        const auto prior = i >= 2 ? analyzed.value->tokens[i - 2].kind : tokens::UNKNOWN;
        const auto expected = canonical_gap(prior, left.kind, right.kind);
        if (expected && gap != *expected)
          changes.edits.push_back({{document.identity().id, {begin, end}}, std::string(*expected)});
      }
      workspace_edit edit{{std::move(changes)}};
      const auto preview = preview_edits(edit, {&document});
      if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
      const source::document_snapshot changed(document.identity(), document.version(),
                                               preview.documents.front().text);
      const auto verified = syntax::analyze(changed, {.recover = true});
      if (!verified.value ||
          static_cast<bool>(verified.value->strict_ast) != !recovered ||
          !matching_tokens(*analyzed.value, *verified.value) ||
          !same_diagnostics(analyzed.diagnostics, verified.diagnostics))
        return {edit_state::unsupported, "Formatting could change tokenization or syntax", {}};
      return {edit_state::ready, {}, std::move(edit)};
    }
  }

  auto format_document(const source::document_snapshot &document) -> format_result
  {
    return format_lines(document, {0, static_cast<source::byte_offset>(document.text().size())});
  }

  auto format_range(const source::document_snapshot &document, const source::byte_range range)
    -> format_result
  {
    return format_lines(document, range);
  }

  auto format_on_type(const source::document_snapshot &document, const source::byte_offset offset,
                      const char trigger) -> format_result
  {
    if (trigger != '}' && trigger != '\n')
      return {edit_state::unsupported, "No formatting action for this trigger", {}};
    if (!document.to_utf16(offset))
      return {edit_state::invalid, "Formatting position is not a source boundary", {}};
    const auto text = document.text();
    const auto before = offset == 0 ? std::string_view::npos : text.find_last_of("\r\n", offset - 1);
    const auto line_begin = before == std::string_view::npos ? 0 : before + 1;
    const auto after = text.find_first_of("\r\n", offset);
    const auto line_end = after == std::string_view::npos ? text.size() : after;
    return format_lines(document, {static_cast<source::byte_offset>(line_begin),
                                   static_cast<source::byte_offset>(line_end)});
  }
}
