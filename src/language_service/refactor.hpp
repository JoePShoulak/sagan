#pragma once

#include "edits.hpp"
#include "../semantic/index.hpp"

#include <string_view>

namespace sagan::language_service
{
  // Only a private local binding whose entire identity-based reference set can
  // be reanalyzed is offered. Module/public/member rename is not yet proven.
  auto rename_local(const source::document_snapshot &document, const semantic::semantic_index &index,
                    source::byte_offset position, std::string_view new_name) -> edit_plan;

  // Sort one uninterrupted, comment-free import block at the top of a module.
  // Ambiguous trivia or interleaved imports cause a structured refusal.
  auto organize_imports(const source::document_snapshot &document) -> edit_plan;
}
