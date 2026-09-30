#include "cpp_generator.hpp"

#include <limits>

namespace codegen
{
  auto generated_offset(const std::string &text, const std::size_t one_based_line,
                        const std::size_t one_based_column) -> std::optional<std::size_t>
  {
    if (one_based_line == 0 || one_based_column == 0) return {};
    std::size_t offset = 0;
    for (std::size_t line = 1; line < one_based_line; ++line)
    {
      const auto newline = text.find('\n', offset);
      if (newline == std::string::npos) return {};
      offset = newline + 1;
    }
    const auto end = text.find('\n', offset);
    const auto length = (end == std::string::npos ? text.size() : end) - offset;
    if (one_based_column > length + 1) return {};
    return offset + one_based_column - 1;
  }

  auto source_for_generated_offset(const generated_cpp &output, const std::size_t offset)
    -> const source_map_entry *
  {
    const source_map_entry *best = nullptr;
    auto smallest = std::numeric_limits<std::size_t>::max();
    for (const auto &entry : output.mappings)
    {
      if (entry.generated_begin > offset || offset >= entry.generated_end) continue;
      const auto size = entry.generated_end - entry.generated_begin;
      if (size < smallest || (size == smallest && entry.breakpoint && best && !best->breakpoint))
      {
        best = &entry;
        smallest = size;
      }
    }
    return best;
  }
}
