#pragma once

#include "../diagnostics/diagnostic.hpp"
#include "../source/provider.hpp"
#include "operations.hpp"

#include <filesystem>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view test_schema_version = "sagan-tests-v1";

  struct discovered_test
  {
    std::string id;
    std::string name;
    std::vector<std::string> suites;
    std::string package;
    std::string module;
    source::document_uri uri;
    source::source_range declaration;
    source::source_range name_range;
    source::utf16_position start;
    source::utf16_position end;
    source::document_version version{};
  };

  struct test_discovery_result
  {
    diagnostics::result_state state{diagnostics::result_state::incomplete};
    std::vector<discovered_test> tests;
    std::vector<diagnostics::diagnostic> diagnostics;
  };

  auto discover_document_tests(const source::document_snapshot &document,
                               std::string package = "local",
                               std::string module = {},
                               diagnostics::cancellation_token cancellation = {})
    -> test_discovery_result;

  auto discover_project_tests(const std::filesystem::path &entry_or_package,
                              const source::source_provider &source,
                              diagnostics::cancellation_token cancellation = {})
    -> test_discovery_result;

  enum class test_case_state { queued, running, passed, failed, errored, skipped, cancelled };
  auto test_case_state_name(test_case_state state) -> std::string_view;

  struct test_case_result
  {
    discovered_test test;
    test_case_state state{test_case_state::queued};
    std::uint64_t duration_milliseconds{};
    std::optional<int> exit_status;
    std::string message;
    std::string standard_output;
    std::string standard_error;
    bool output_truncated{};

    explicit test_case_result(discovered_test discovered) : test(std::move(discovered)) {}
  };

  struct test_run_result
  {
    diagnostics::result_state state{diagnostics::result_state::incomplete};
    std::vector<test_case_result> tests;
    std::vector<diagnostics::diagnostic> diagnostics;
  };

  using test_observer = std::function<void(const test_case_result &)>;
  auto run_document_tests(const source::document_snapshot &document,
                          const std::filesystem::path &artifact_root,
                          const std::vector<std::string> &selected_ids = {},
                          diagnostics::cancellation_token cancellation = {},
                          const test_observer &observer = {}) -> test_run_result;

  auto run_project_tests(const std::filesystem::path &entry_or_package,
                         const source::source_provider &source,
                         const std::filesystem::path &artifact_root,
                         const std::vector<std::string> &selected_ids = {},
                         diagnostics::cancellation_token cancellation = {},
                         const test_observer &observer = {}) -> test_run_result;
}
