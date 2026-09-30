#pragma once

#include <string>
#include <utility>
#include <vector>

namespace driver
{
  struct native_compiler_configuration
  {
    std::string executable;
    std::string flags;
    std::vector<std::pair<std::string, std::string>> environment;
  };

  auto configured_compiler() -> native_compiler_configuration;
  auto compile_and_run(const std::string &generated_cpp) -> int;
}
