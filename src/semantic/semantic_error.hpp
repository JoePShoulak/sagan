#pragma once

#include "../parser/span.hpp"

#include <stdexcept>
#include <string>

namespace semantic
{
  struct semantic_error final : std::runtime_error
  {
    parser::span range;

    semantic_error(std::string message, parser::span source_range)
        : std::runtime_error(std::move(message)), range(source_range)
    {
    }
  };
}
