#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace sagan::dap
{
  inline constexpr std::size_t max_frame_bytes = 16 * 1024 * 1024;

  // DAP uses the same Content-Length framing as LSP, but keeps its own
  // bounded reader so the debugger transport never writes diagnostics to
  // protocol stdout.
  auto read_frame(std::istream &input) -> std::optional<std::string>;
  auto write_frame(std::ostream &output, std::string_view body) -> void;
}
