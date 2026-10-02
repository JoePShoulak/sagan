#include "framing.hpp"

#include <charconv>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace sagan::dap
{
  auto read_frame(std::istream &input) -> std::optional<std::string>
  {
    std::string header;
    char byte{};
    while (header.size() < 8192 && input.get(byte))
    {
      header.push_back(byte);
      if (header.ends_with("\r\n\r\n")) break;
    }
    if (header.empty() && input.eof()) return {};
    if (!header.ends_with("\r\n\r\n")) throw std::runtime_error("Malformed or oversized DAP header");
    std::optional<std::size_t> length;
    std::size_t begin = 0;
    while (begin < header.size() - 2)
    {
      const auto end = header.find("\r\n", begin);
      if (end == std::string::npos) break;
      const std::string_view line(header.data() + begin, end - begin);
      if (line.starts_with("Content-Length:"))
      {
        if (length) throw std::runtime_error("Duplicate DAP Content-Length header");
        auto value = line.substr(sizeof("Content-Length:") - 1);
        while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
        std::size_t parsed{};
        const auto [last, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || last != value.data() + value.size() || parsed > max_frame_bytes)
          throw std::runtime_error("Invalid or oversized DAP Content-Length");
        length = parsed;
      }
      begin = end + 2;
    }
    if (!length) throw std::runtime_error("DAP Content-Length is missing");
    std::string body(*length, '\0');
    input.read(body.data(), static_cast<std::streamsize>(*length));
    if (input.gcount() != static_cast<std::streamsize>(*length))
      throw std::runtime_error("Truncated DAP frame");
    return body;
  }

  auto write_frame(std::ostream &output, const std::string_view body) -> void
  {
    if (body.size() > max_frame_bytes) throw std::runtime_error("Oversized DAP response");
    output << "Content-Length: " << body.size() << "\r\n\r\n";
    output.write(body.data(), static_cast<std::streamsize>(body.size()));
    output.flush();
    if (!output) throw std::runtime_error("Could not write DAP frame");
  }
}
