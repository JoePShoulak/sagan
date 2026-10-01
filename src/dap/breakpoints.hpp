#pragma once

#include "../language_service/debug_metadata.hpp"

#include <optional>
#include <string>

namespace sagan::dap
{
  inline constexpr std::string_view breakpoint_mapping_schema = "sagan-dap-breakpoints-v1";

  struct mapped_breakpoint
  {
    source::source_range source;
    source::utf16_position position;
    std::size_t generated_line{};
    std::size_t generated_column{};
  };

  struct breakpoint_result
  {
    std::optional<mapped_breakpoint> resolved;
    std::string message;
  };

  // Resolve an editor UTF-16 position against *executable* Sagan mappings.
  // A different line is deliberately refused: guessing the next statement
  // can silently move a breakpoint into another function or control branch.
  auto map_breakpoint(const language_service::debug_metadata &metadata,
                      const codegen::generated_cpp &generated,
                      const source::document_snapshot &document,
                      source::utf16_position requested) -> breakpoint_result;
}
