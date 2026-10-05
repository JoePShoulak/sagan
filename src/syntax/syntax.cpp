#include "syntax.hpp"

#include "../parser/lex.hpp"
#include "../parser/parse_error.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"
#include "../parser/tokens.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

namespace sagan::syntax
{
  namespace
  {
    auto source_range(const source::document_snapshot &document, const parser::span range)
      -> source::source_range
    {
      const auto size = static_cast<int>(document.text().size());
      const auto begin = static_cast<source::byte_offset>(std::clamp(range.begin, 0, size));
      const auto end = static_cast<source::byte_offset>(std::clamp(range.end, static_cast<int>(begin), size));
      return {document.identity().id, {begin, end}};
    }

    auto parse_diagnostic(const source::document_snapshot &document, const diagnostics::phase phase,
                          const parser::parse_error &error) -> diagnostics::diagnostic
    {
      return diagnostics::diagnostic{std::string(diagnostics::default_code(phase)), diagnostics::severity::error,
                                     phase, source_range(document, error.range), error.what(), {}, {}, {}};
    }

    auto trivia_between(const std::string_view text, std::size_t begin, const std::size_t end)
      -> std::vector<trivia>
    {
      std::vector<trivia> result;
      while (begin < end)
      {
        const std::size_t start = begin;
        trivia_kind kind = trivia_kind::skipped_text;
        if (std::isspace(static_cast<unsigned char>(text[begin])) != 0)
        {
          kind = trivia_kind::whitespace;
          while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) ++begin;
        }
        else if (begin + 1 < end && text[begin] == '/' && text[begin + 1] == '/')
        {
          kind = trivia_kind::line_comment;
          begin += 2;
          while (begin < end && text[begin] != '\r' && text[begin] != '\n') ++begin;
        }
        else if (begin + 1 < end && text[begin] == '/' && text[begin + 1] == '*')
        {
          kind = trivia_kind::block_comment;
          int depth = 1;
          begin += 2;
          while (begin < end && depth > 0)
          {
            if (begin + 1 < end && text[begin] == '/' && text[begin + 1] == '*') { ++depth; begin += 2; }
            else if (begin + 1 < end && text[begin] == '*' && text[begin + 1] == '/') { --depth; begin += 2; }
            else ++begin;
          }
        }
        else ++begin;
        result.push_back(trivia{kind,
                                {static_cast<source::byte_offset>(start), static_cast<source::byte_offset>(begin)},
                                std::string(text.substr(start, begin - start))});
      }
      return result;
    }

    auto duplicate_diagnostic(const std::vector<diagnostics::diagnostic> &values,
                              const diagnostics::diagnostic &candidate) -> bool
    {
      return std::ranges::any_of(values, [&candidate](const auto &existing)
      {
        return existing.code == candidate.code && existing.primary == candidate.primary &&
               existing.message == candidate.message;
      });
    }

    auto declaration_start(const int kind) -> bool
    {
      return kind == tokens::KWD_LET || kind == tokens::KWD_CONST ||
             kind == tokens::KWD_FUN || kind == tokens::KWD_TEST || kind == tokens::KWD_FACE ||
             kind == tokens::KWD_CLASS || kind == tokens::KWD_ENUM || kind == tokens::KWD_MODULE ||
             kind == tokens::KWD_IMPORT || kind == tokens::KWD_EXPORT;
    }

    auto declaration_chunks(const std::vector<parser::token> &tokens, const std::string_view source_text)
      -> std::vector<std::pair<std::size_t, std::size_t>>
    {
      std::vector<std::pair<std::size_t, std::size_t>> result;
      std::size_t start = 0;
      int braces = 0;
      for (std::size_t index = 0; index < tokens.size(); ++index)
      {
        if (index > start && braces == 0 && declaration_start(tokens[index].id) &&
            tokens[index - 1].id != tokens::DOC_COMMENT)
        {
          const auto gap_begin = static_cast<std::size_t>(std::max(tokens[index - 1].range.end, 0));
          const auto gap_end = static_cast<std::size_t>(std::max(tokens[index].range.begin, 0));
          if (gap_end < gap_begin || gap_begin > source_text.size()) continue;
          const auto gap = source_text.substr(gap_begin, std::min(gap_end, source_text.size()) - gap_begin);
          if (gap.find_first_of("\r\n") != std::string_view::npos)
          {
            std::size_t end = index;
            while (end > start && tokens[end - 1].id == tokens::NEWLINE) --end;
            if (end > start) result.emplace_back(start, end);
            start = index;
          }
        }
        if (tokens[index].id == tokens::LBRACE) ++braces;
        else if (tokens[index].id == tokens::RBRACE && braces > 0) --braces;
        if (tokens[index].id != tokens::NEWLINE || braces != 0) continue;

        std::size_t last = index;
        while (last > start && tokens[last - 1].id == tokens::NEWLINE) --last;
        if (last > start && tokens[last - 1].id == tokens::DOC_COMMENT) continue;
        if (last > start) result.emplace_back(start, last);
        start = index + 1;
      }
      while (start < tokens.size() && tokens[start].id == tokens::NEWLINE) ++start;
      if (start < tokens.size()) result.emplace_back(start, tokens.size());
      return result;
    }
  }

  auto analyze(const source::document_snapshot &document, const analysis_options options,
               const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<syntax_document>
  {
    syntax_document result{document.identity().id, document.version(), {}, {}, {}, {}, {}};
    std::vector<parser::token> parser_tokens;
    std::vector<diagnostics::diagnostic> reported;
    parser::tokenizer lexer(parser::programText{std::string(document.text())}, get_token);
    std::uint64_t next_id = 2;

    while (true)
    {
      if (cancellation.is_cancelled())
        return {diagnostics::result_state::cancelled, {}, {}, document.version()};
      try
      {
        auto next = lexer.next();
        if (!next) break;
        parser_tokens.push_back(std::move(*next));
      }
      catch (const parser::parse_error &error)
      {
        reported.push_back(parse_diagnostic(document, diagnostics::phase::lexical, error));
        const auto range = source_range(document, error.range).bytes;
        parser_tokens.push_back(parser::token{tokens::UNKNOWN, error.range,
                                              std::string(document.text().substr(range.begin, range.end - range.begin))});
        if (!options.recover || reported.size() >= options.maximum_diagnostics ||
            range.end >= document.text().size()) break;
        lexer.recover_after_error(error.range);
      }
    }

    std::stable_sort(parser_tokens.begin(), parser_tokens.end(), [](const auto &left, const auto &right)
    {
      return left.range.begin < right.range.begin;
    });
    std::size_t previous_end = 0;
    for (const auto &token : parser_tokens)
    {
      const auto size = document.text().size();
      const auto begin = std::min(static_cast<std::size_t>(std::max(token.range.begin, 0)), size);
      const auto end = std::min(static_cast<std::size_t>(std::max(token.range.end, token.range.begin)), size);
      auto leading = trivia_between(document.text(), previous_end, std::max(previous_end, begin));
      result.tokens.push_back(lossless_token{node_id{next_id++}, token.id,
                                             {static_cast<source::byte_offset>(begin), static_cast<source::byte_offset>(end)},
                                             std::string(document.text().substr(begin, end - begin)),
                                             std::move(leading), token.id == tokens::UNKNOWN});
      previous_end = std::max(previous_end, end);
    }
    result.trailing_trivia = trivia_between(document.text(), previous_end, document.text().size());

    std::vector<node_id> all_token_ids;
    all_token_ids.reserve(result.tokens.size());
    for (const auto &token : result.tokens) all_token_ids.push_back(token.id);
    result.nodes.push_back(syntax_node{node_id{1}, node_kind::document,
                                       {0, static_cast<source::byte_offset>(document.text().size())},
                                       std::move(all_token_ids)});

    if (reported.empty())
    {
      try
      {
        parser::syntax_parser strict(parser_tokens, cancellation);
        result.strict_ast = std::make_unique<parser::program>(strict.parse());
      }
      catch (const parser::parse_cancelled &)
      { return {diagnostics::result_state::cancelled, {}, {}, document.version()}; }
      catch (const parser::parse_error &error)
      {
        reported.push_back(parse_diagnostic(document, diagnostics::phase::syntax, error));
      }
    }

    if (!reported.empty() && options.recover)
    {
      std::vector<parser::statement_ref> recovered_statements;
      for (const auto &[begin, end] : declaration_chunks(parser_tokens, document.text()))
      {
        if (reported.size() >= options.maximum_diagnostics || cancellation.is_cancelled()) break;
        std::vector<parser::token> chunk(parser_tokens.begin() + static_cast<std::ptrdiff_t>(begin),
                                         parser_tokens.begin() + static_cast<std::ptrdiff_t>(end));
        const source::byte_range chunk_range{static_cast<source::byte_offset>(chunk.front().range.begin),
                                             static_cast<source::byte_offset>(chunk.back().range.end)};
        std::vector<node_id> chunk_tokens;
        for (const auto &value : result.tokens)
          if (value.range.begin >= chunk_range.begin && value.range.end <= chunk_range.end)
            chunk_tokens.push_back(value.id);
        try
        {
          parser::syntax_parser partial(std::move(chunk), cancellation);
          auto tree = partial.parse();
          for (auto &statement : tree.statements) recovered_statements.push_back(std::move(statement));
          result.nodes.push_back(syntax_node{node_id{next_id++}, node_kind::declaration, chunk_range,
                                             std::move(chunk_tokens)});
        }
        catch (const parser::parse_cancelled &)
        { return {diagnostics::result_state::cancelled, {}, {}, document.version()}; }
        catch (const parser::parse_error &error)
        {
          auto value = parse_diagnostic(document, diagnostics::phase::syntax, error);
          if (!duplicate_diagnostic(reported, value)) reported.push_back(std::move(value));
          result.nodes.push_back(syntax_node{node_id{next_id++}, node_kind::error_region, chunk_range,
                                             std::move(chunk_tokens)});
        }
      }
      result.recovered_ast = std::make_unique<parser::program>(std::move(recovered_statements));
    }

    if (cancellation.is_cancelled())
      return {diagnostics::result_state::cancelled, {}, {}, document.version()};
    const auto state = reported.empty() ? diagnostics::result_state::complete
                                        : (options.recover ? diagnostics::result_state::recovered
                                                           : diagnostics::result_state::incomplete);
    return {state, std::move(result), std::move(reported), document.version()};
  }
}
