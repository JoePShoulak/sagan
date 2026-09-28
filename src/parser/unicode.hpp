#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace sagan::unicode
{
  struct code_point
  {
    char32_t value;
    std::size_t width;
  };

  auto decode(std::string_view text, std::size_t offset) -> std::optional<code_point>;
  auto first_invalid_utf8(std::string_view text) -> std::optional<std::size_t>;
  auto is_xid_start(char32_t value) -> bool;
  auto is_xid_continue(char32_t value) -> bool;
  auto emoji_sequence_length(std::string_view text, std::size_t offset) -> std::size_t;
  auto normalize_nfc(std::string_view text) -> std::string;
}
