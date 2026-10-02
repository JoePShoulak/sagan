#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace modules { struct module_graph; }

namespace driver
{
  struct native_compilation_inputs
  {
    std::optional<std::filesystem::path> header;
    std::optional<std::filesystem::path> source;
    std::optional<std::filesystem::path> working_directory;
    std::vector<std::string> libraries;
  };

  auto compilation_inputs_for(const modules::module_graph &graph) -> native_compilation_inputs;
  struct native_compiler_configuration
  {
    std::string executable;
    std::string flags;
    std::vector<std::pair<std::string, std::string>> environment;
  };

  auto configured_compiler() -> native_compiler_configuration;
  auto compile_and_run(const std::string &generated_cpp,
                       const native_compilation_inputs &inputs = {}) -> int;
}
