#pragma once

#include "language_service.hpp"
#include "../semantic/workspace_index.hpp"
#include "../syntax/syntax.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sagan::language_service
{
  struct symbol_occurrence
  {
    semantic::symbol_id id;
    semantic::symbol_kind kind;
    semantic::symbol_origin origin;
    source::source_range selection;
    source::source_range declaration;
    std::string name;
  };

  struct hover_information
  {
    symbol_occurrence symbol;
    std::optional<std::string> type;
    std::vector<std::string> documentation;
  };

  struct document_symbol
  {
    symbol_occurrence symbol;
    std::vector<document_symbol> children;
  };

  // These queries consume one immutable analysis result. A caller must never
  // combine an index with a newer editor buffer: a mismatch returns stale.
  class document_queries
  {
    const source::document_snapshot &document_;
    const semantic::semantic_index &index_;
    const semantic::workspace_semantic_index *workspace_;
    std::vector<syntax::lossless_token> tokens_;

    auto occurrence_at(source::byte_offset offset) const -> std::optional<symbol_occurrence>;

  public:
    document_queries(const source::document_snapshot &document, const semantic::semantic_index &index,
                     const semantic::workspace_semantic_index *workspace = nullptr);

    auto symbol_at(source::byte_offset offset) const -> diagnostics::analysis_result<symbol_occurrence>;
    auto symbol_at(source::utf16_position position) const -> diagnostics::analysis_result<symbol_occurrence>;
    auto definitions(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto implementations(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto references(source::byte_offset offset, bool include_declaration = false) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto document_highlights(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto resolved_type(source::byte_offset offset) const -> diagnostics::analysis_result<std::string>;
    auto hover(source::byte_offset offset) const -> diagnostics::analysis_result<hover_information>;
    auto document_symbols() const -> diagnostics::analysis_result<std::vector<document_symbol>>;
  };
}
