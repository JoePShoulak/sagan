#pragma once

#include "language_service.hpp"
#include "debug_metadata.hpp"
#include "../codegen/cpp_generator.hpp"
#include "../source/provider.hpp"

#include <cstddef>
#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view operations_schema_version = "sagan-operations-v2";

  enum class operation_state { queued, running, completed, failed, cancelled, stale };
  auto operation_state_name(operation_state state) -> std::string_view;
  auto next_operation_id() -> std::string;

  enum class operation_event_kind { started, progress, standard_output, standard_error, diagnostic, finished };

  struct operation_event
  {
    operation_event_kind kind{};
    std::size_t sequence{};
    int percent{};
    std::string text;
    std::optional<diagnostics::diagnostic> issue;
    std::string operation_id;
    operation_state state{operation_state::running};
  };

  struct check_operation_result
  {
    std::string operation_id;
    operation_state lifecycle{operation_state::queued};
    diagnostics::result_state state{diagnostics::result_state::incomplete};
    source::document_identity document;
    source::document_version analyzed_version{};
    std::optional<check_summary> summary;
    std::vector<diagnostics::diagnostic> diagnostics;
    std::vector<operation_event> events;
    std::optional<int> exit_status;
  };

  using operation_observer = std::function<void(const operation_event &)>;

  template <typename Result> struct asynchronous_operation
  {
    std::string id;
    diagnostics::cancellation_source cancellation;
    std::shared_ptr<std::atomic<operation_state>> state;
    std::future<Result> future;

    auto cancel() const -> void { cancellation.cancel(); }
    auto current_state() const -> operation_state { return state->load(); }
  };

  // Synchronous first slice of the operations API. The caller owns the source
  // snapshot and cancellation token; no file or editor buffer is mutated.
  auto run_check_operation(const source::document_snapshot &document, check_options options = {},
                           diagnostics::cancellation_token cancellation = {},
                           const operation_observer &observer = {},
                           std::string operation_id = {}) -> check_operation_result;

  auto start_check_document(source::document_snapshot document, check_options options = {},
                            operation_observer observer = {}) -> asynchronous_operation<check_operation_result>;

  auto run_check_project(const std::filesystem::path &entry_or_package,
                         const source::source_provider &source,
                         diagnostics::cancellation_token cancellation = {},
                         const operation_observer &observer = {},
                         std::string operation_id = {}) -> check_operation_result;
  auto start_check_project(std::filesystem::path entry_or_package,
                           std::shared_ptr<const source::source_provider> source,
                           operation_observer observer = {}) -> asynchronous_operation<check_operation_result>;

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
    std::string operation_id;
    operation_state lifecycle{operation_state::queued};
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
    std::optional<std::filesystem::path> working_directory;
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

  struct project_test_selection
  {
    std::filesystem::path source_path;
    std::string name;
  };

  auto build_document(const source::document_snapshot &document,
                      const std::filesystem::path &artifact_root,
                      native_build_profile profile = native_build_profile::debug,
                      diagnostics::cancellation_token cancellation = {},
                      const operation_observer &observer = {},
                      std::string operation_id = {},
                      std::optional<std::string> selected_test = {}) -> native_operation_result;
  auto run_document(const source::document_snapshot &document,
                    const std::filesystem::path &artifact_root,
                    native_build_profile profile = native_build_profile::debug,
                    diagnostics::cancellation_token cancellation = {},
                    const operation_observer &observer = {},
                    std::string operation_id = {},
                    std::optional<std::string> selected_test = {}) -> native_operation_result;

  auto build_project(const std::filesystem::path &entry_or_package,
                     const source::source_provider &source,
                     const std::filesystem::path &artifact_root,
                     native_build_profile profile = native_build_profile::debug,
                     diagnostics::cancellation_token cancellation = {},
                     const operation_observer &observer = {},
                     std::string operation_id = {},
                     std::optional<project_test_selection> selected_test = {}) -> native_operation_result;
  auto run_project(const std::filesystem::path &entry_or_package,
                   const source::source_provider &source,
                   const std::filesystem::path &artifact_root,
                   native_build_profile profile = native_build_profile::debug,
                   diagnostics::cancellation_token cancellation = {},
                   const operation_observer &observer = {},
                   std::string operation_id = {},
                   std::optional<project_test_selection> selected_test = {}) -> native_operation_result;

  auto start_build_document(source::document_snapshot document, std::filesystem::path artifact_root,
                            native_build_profile profile = native_build_profile::debug,
                            operation_observer observer = {}) -> asynchronous_operation<native_operation_result>;
  auto start_run_document(source::document_snapshot document, std::filesystem::path artifact_root,
                          native_build_profile profile = native_build_profile::debug,
                          operation_observer observer = {}) -> asynchronous_operation<native_operation_result>;
  auto start_build_project(std::filesystem::path entry_or_package,
                           std::shared_ptr<const source::source_provider> source,
                           std::filesystem::path artifact_root,
                           native_build_profile profile = native_build_profile::debug,
                           operation_observer observer = {}) -> asynchronous_operation<native_operation_result>;
  auto start_run_project(std::filesystem::path entry_or_package,
                         std::shared_ptr<const source::source_provider> source,
                         std::filesystem::path artifact_root,
                         native_build_profile profile = native_build_profile::debug,
                         operation_observer observer = {}) -> asynchronous_operation<native_operation_result>;

  auto map_toolchain_errors(const source::document_snapshot &document,
                            const codegen::generated_cpp &generated,
                            const std::string &compiler_stderr,
                            const source::source_provider *provider = nullptr)
    -> std::vector<diagnostics::diagnostic>;
  auto map_runtime_failure(const source::document_snapshot &document,
                           const std::string &runtime_stderr,
                           const source::source_provider *provider = nullptr)
    -> std::optional<diagnostics::diagnostic>;
}
