#pragma once

#include "edits.hpp"
#include <string_view>

namespace sagan::language_service
{
  inline constexpr std::string_view formatting_schema_version = "sagan-formatting-v1";

  struct format_result
  {
    edit_state state{edit_state::unsupported};
    std::string reason;
    workspace_edit edits;
  };

  // Conservative two-space layout. Recovered documents are limited to lines
  // with proven token/trivia ownership; a proposed edit must preserve the
  // token stream, strict/recovered state, and diagnostic meaning.
  auto format_document(const source::document_snapshot &document) -> format_result;
  auto format_range(const source::document_snapshot &document, source::byte_range range)
    -> format_result;
  auto format_on_type(const source::document_snapshot &document, source::byte_offset offset,
                      char trigger) -> format_result;
}
