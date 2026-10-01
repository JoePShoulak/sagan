#include "../src/dap/framing.hpp"

#include <sstream>
#include <stdexcept>

namespace
{
  auto require(const bool value, const char *message) -> void
  {
    if (!value) throw std::runtime_error(message);
  }

  auto rejects(const std::string &data) -> bool
  {
    std::istringstream input(data);
    try { static_cast<void>(sagan::dap::read_frame(input)); }
    catch (const std::exception &) { return true; }
    return false;
  }
}

auto main() -> int
{
  std::stringstream transport;
  sagan::dap::write_frame(transport, R"({"type":"request","command":"initialize"})");
  sagan::dap::write_frame(transport, R"({"type":"request","command":"disconnect"})");
  const auto first = sagan::dap::read_frame(transport);
  const auto second = sagan::dap::read_frame(transport);
  require(first && first->find("initialize") != std::string::npos &&
              second && second->find("disconnect") != std::string::npos &&
              !sagan::dap::read_frame(transport),
          "DAP framing lost request boundaries or EOF");
  require(rejects("Content-Length: 4\r\n\r\nabc") &&
              rejects("Content-Length: 16777217\r\n\r\n") &&
              rejects("Content-Length: x\r\n\r\n") &&
              rejects("Content-Length: 1\r\nContent-Length: 1\r\n\r\na") &&
              rejects("Content-Type: json\r\n\r\n{}") &&
              rejects("Content-Length: 1\n\na"),
          "DAP framing accepted malformed or oversized input");
  return 0;
}
