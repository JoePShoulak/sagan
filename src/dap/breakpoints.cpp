#include "breakpoints.hpp"

#include <algorithm>
#include <limits>

namespace sagan::dap
{
  auto map_breakpoint(const language_service::debug_metadata &metadata,
                      const codegen::generated_cpp &generated,
                      const source::document_snapshot &document,
                      const source::utf16_position requested) -> breakpoint_result
  {
    if (requested.line >= document.lines().line_count())
      return {{}, "Source line is outside the document"};
    if (!document.to_byte(requested))
      return {{}, "Source column is not a valid UTF-16 boundary"};
    const auto *selected = static_cast<const language_service::debug_breakpoint *>(nullptr);
    auto best_distance = std::numeric_limits<std::size_t>::max();
    for (const auto &candidate : metadata.breakpoints)
    {
      if (candidate.source.document != document.identity().id ||
          candidate.source.bytes.begin > document.text().size() ||
          candidate.generated_begin >= generated.text.size()) continue;
      if (document.identity().canonical_path && candidate.source_path &&
          document.identity().canonical_path->lexically_normal() !=
              candidate.source_path->lexically_normal()) continue;
      const auto position = document.to_utf16(candidate.source.bytes.begin);
      if (!position || position->line != requested.line) continue;
      const auto distance = position->character > requested.character
                                ? position->character - requested.character
                                : requested.character - position->character;
      if (distance < best_distance ||
          (distance == best_distance && selected &&
           candidate.generated_begin < selected->generated_begin))
      { selected = &candidate; best_distance = distance; }
    }
    if (!selected) return {{}, "This Sagan source line has no executable breakpoint location"};
    const auto position = document.to_utf16(selected->source.bytes.begin);
    if (!position) return {{}, "Breakpoint source position is invalid"};
    const auto prefix = std::string_view(generated.text).substr(0, selected->generated_begin);
    const auto line = 1 + static_cast<std::size_t>(std::count(prefix.begin(), prefix.end(), '\n'));
    const auto newline = prefix.rfind('\n');
    const auto column = selected->generated_begin - (newline == std::string_view::npos ? 0 : newline + 1) + 1;
    return {mapped_breakpoint{selected->source, *position, line, column}, {}};
  }
}
