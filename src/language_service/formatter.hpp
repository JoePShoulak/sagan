#pragma once

#include "edits.hpp"

namespace sagan::language_service
{
  struct format_result
  {
    edit_state state{edit_state::unsupported};
    std::string reason;
    workspace_edit edits;
  };

  // Phase 6's conservative layout pass: two-space structural indentation.
  // It preserves all non-whitespace source and refuses a change unless both
  // strict parses and their lossless token streams agree.
  auto format_document(const source::document_snapshot &document) -> format_result;
  auto format_range(const source::document_snapshot &document, source::byte_range range)
    -> format_result;
  auto format_on_type(const source::document_snapshot &document, source::byte_offset offset,
                      char trigger) -> format_result;
}
