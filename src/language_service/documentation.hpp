#pragma once

#include "../semantic/index.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::language_service
{
  struct documentation_parameter
  {
    std::string name;
    std::string description;
  };

  struct documentation_entry
  {
    semantic::symbol_id symbol;
    std::string summary;
    std::string detail;
    std::vector<documentation_parameter> parameters;
    std::vector<documentation_parameter> generic_parameters;
    std::string returns;
    std::vector<std::string> examples;
    bool deprecated{};
    std::string availability;
    std::string module;
    std::optional<source::source_range> source;
  };

  inline constexpr std::string_view documentation_schema_version = "sagan-documentation-v1";
  auto documentation_for(const semantic::indexed_symbol &symbol, std::string module = "local")
    -> documentation_entry;
}
