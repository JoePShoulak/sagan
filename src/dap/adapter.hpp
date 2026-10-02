#pragma once

#include <filesystem>
#include <iosfwd>

namespace sagan::dap
{
  // The optional debugger path is used by protocol tests. Production lookup
  // finds a bundled toolchain beside sagan-dap, not an editor-supplied tool.
  auto run_adapter(std::istream &input, std::ostream &output, std::ostream &errors,
                   std::filesystem::path debugger = {}) -> int;
}
