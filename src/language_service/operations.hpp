#pragma once

#include "language_service.hpp"

#include <cstddef>
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
}
