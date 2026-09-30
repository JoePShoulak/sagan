#pragma once

#include <string_view>

namespace parser
{
  // Deliberately ASCII-only for the first explicit-constant feature. Ordinary
  // identifiers have a separate Unicode/emoji lexical rule.
  inline auto is_constant_name(const std::string_view name) -> bool
  {
    if (name.empty() || name.front() < 'A' || name.front() > 'Z') return false;
    for (const unsigned char value : name)
      if ((value < 'A' || value > 'Z') && (value < '0' || value > '9') && value != '_') return false;
    return true;
  }
}
