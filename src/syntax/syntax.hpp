#pragma once

#include "../diagnostics/diagnostic.hpp"
#include "../parser/ast_node.hpp"
#include "../parser/token.hpp"
#include "../source/source.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sagan::syntax
{
  struct node_id
  {
    std::uint64_t value{};
    auto operator==(const node_id &) const -> bool = default;
  };

  enum class trivia_kind { whitespace, line_comment, block_comment, skipped_text };

  struct trivia
  {
    trivia_kind kind;
    source::byte_range range;
    std::string text;
  };

  struct lossless_token
  {
    node_id id;
    int kind;
    source::byte_range range;
    std::string source_text;
    std::vector<trivia> leading_trivia;
    bool error_token{};
  };

  enum class node_kind { document, declaration, error_region };

  struct syntax_node
  {
    node_id id;
    node_kind kind;
    source::byte_range range;
    std::vector<node_id> tokens;
  };

  struct syntax_document
  {
    source::document_id document;
    source::document_version version;
    std::vector<lossless_token> tokens;
    std::vector<trivia> trailing_trivia;
    std::vector<syntax_node> nodes;
    std::unique_ptr<parser::program> strict_ast;
    std::unique_ptr<parser::program> recovered_ast;
  };

  struct analysis_options
  {
    bool recover{true};
    std::size_t maximum_diagnostics{64};
  };

  auto analyze(const source::document_snapshot &document, analysis_options options = {},
               diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<syntax_document>;
}
