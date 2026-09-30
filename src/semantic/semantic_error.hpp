#pragma once

#include "../parser/span.hpp"

#include <stdexcept>
#include <string>
#include <filesystem>
#include <optional>

namespace semantic
{
  struct semantic_error final : std::runtime_error
  {
    parser::span range;
    std::optional<std::filesystem::path> origin_path;

    semantic_error(std::string message, parser::span source_range,
                   std::optional<std::filesystem::path> source_path = {})
        : std::runtime_error(std::move(message)), range(source_range),
          origin_path(std::move(source_path))
    {
    }
  };
}
