#pragma once

#include "../diagnostics/diagnostic.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace driver
{
  struct process_result
  {
    int exit_status{};
    bool cancelled{};
    bool output_truncated{};
    std::string standard_output;
    std::string standard_error;
  };

  using output_observer = std::function<void(bool is_error, std::string_view text)>;

  auto run_process(const std::filesystem::path &executable,
                   const std::vector<std::string> &arguments,
                   const std::filesystem::path &working_directory,
                   const std::vector<std::pair<std::string, std::string>> &environment,
                   sagan::diagnostics::cancellation_token cancellation = {},
                   const output_observer &observer = {}) -> process_result;
}
