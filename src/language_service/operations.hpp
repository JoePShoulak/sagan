#pragma once

#include "language_service.hpp"
#include "debug_metadata.hpp"
#include "../codegen/cpp_generator.hpp"
#include "../source/provider.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view operations_schema_version = "sagan-operations-v1";

  enum class operation_event_kind { started, progress, standard_output, standard_error, diagnostic, finished };

  struct operation_event
  {
    operation_event_kind kind{};
    std::size_t sequence{};
    int percent{};
    std::string text;
    std::optional<diagnostics::diagnostic> issue;
  };

  struct check_operation_result
  {
    diagnostics::result_state state{diagnostics::result_state::incomplete};
    source::document_identity document;
    source::document_version analyzed_version{};
    std::optional<check_summary> summary;
    std::vector<diagnostics::diagnostic> diagnostics;
    std::vector<operation_event> events;
    std::optional<int> exit_status;
  };

  using operation_observer = std::function<void(const operation_event &)>;

  // Synchronous first slice of the operations API. The caller owns the source
  // snapshot and cancellation token; no file or editor buffer is mutated.
  auto run_check_operation(const source::document_snapshot &document, check_options options = {},
                           diagnostics::cancellation_token cancellation = {},
                           const operation_observer &observer = {}) -> check_operation_result;

  enum class native_build_profile { debug, optimized };

  struct source_dependency_snapshot
  {
    std::filesystem::path path;
    source::document_identity document;
    source::document_version version{};
    std::string text;
  };

  struct native_operation_result
  {
    diagnostics::result_state state{diagnostics::result_state::incomplete};
    native_build_profile profile{native_build_profile::debug};
    source::document_identity document;
    source::document_version analyzed_version{};
    std::string entry_source_text;
    std::vector<source_dependency_snapshot> dependencies;
    std::vector<diagnostics::diagnostic> diagnostics;
    std::vector<operation_event> events;
    std::optional<int> exit_status;
    std::optional<codegen::generated_cpp> generated;
    std::optional<debug_metadata> debug;
    std::optional<std::filesystem::path> generated_source;
    std::optional<std::filesystem::path> executable;
    std::string standard_output;
    std::string standard_error;
    bool output_truncated{};
  };

  struct debug_launch_plan
  {
    std::filesystem::path executable;
    std::filesystem::path working_directory;
    std::vector<std::pair<std::string, std::string>> environment;
    native_build_profile profile{native_build_profile::debug};
  };

  auto plan_debug_launch(const native_operation_result &build)
    -> std::optional<debug_launch_plan>;

  auto build_document(const source::document_snapshot &document,
                      const std::filesystem::path &artifact_root,
                      native_build_profile profile = native_build_profile::debug,
                      diagnostics::cancellation_token cancellation = {},
                      const operation_observer &observer = {}) -> native_operation_result;
  auto run_document(const source::document_snapshot &document,
                    const std::filesystem::path &artifact_root,
                    native_build_profile profile = native_build_profile::debug,
                    diagnostics::cancellation_token cancellation = {},
                    const operation_observer &observer = {}) -> native_operation_result;

  auto build_project(const std::filesystem::path &entry_or_package,
                     const source::source_provider &source,
                     const std::filesystem::path &artifact_root,
                     native_build_profile profile = native_build_profile::debug,
                     diagnostics::cancellation_token cancellation = {},
                     const operation_observer &observer = {}) -> native_operation_result;
  auto run_project(const std::filesystem::path &entry_or_package,
                   const source::source_provider &source,
                   const std::filesystem::path &artifact_root,
                   native_build_profile profile = native_build_profile::debug,
                   diagnostics::cancellation_token cancellation = {},
                   const operation_observer &observer = {}) -> native_operation_result;

  auto map_toolchain_errors(const source::document_snapshot &document,
                            const codegen::generated_cpp &generated,
                            const std::string &compiler_stderr,
                            const source::source_provider *provider = nullptr)
    -> std::vector<diagnostics::diagnostic>;
}
