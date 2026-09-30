#include "operations.hpp"

#include <utility>

namespace sagan::language_service
{
  auto run_check_operation(const source::document_snapshot &document, const check_options options,
                           const diagnostics::cancellation_token cancellation,
                           const operation_observer &observer) -> check_operation_result
  {
    check_operation_result result;
    result.document = document.identity();
    result.analyzed_version = document.version();
    const auto emit = [&](const operation_event_kind kind, const int percent,
                          std::string text = {},
                          std::optional<diagnostics::diagnostic> issue = {})
    {
      result.events.push_back({kind, result.events.size(), percent, std::move(text), std::move(issue)});
      if (observer) observer(result.events.back());
    };
    emit(operation_event_kind::started, 0, "Checking document");
    if (cancellation.is_cancelled())
    {
      result.state = diagnostics::result_state::cancelled;
      emit(operation_event_kind::finished, 100, "Check cancelled");
      return result;
    }
    emit(operation_event_kind::progress, 10, "Analyzing source");
    auto analysis = check_document(document, options, cancellation);
    result.state = analysis.state;
    result.analyzed_version = analysis.analyzed_version;
    result.summary = std::move(analysis.value);
    result.diagnostics = std::move(analysis.diagnostics);
    if (result.state != diagnostics::result_state::cancelled)
      for (const auto &issue : result.diagnostics)
        emit(operation_event_kind::diagnostic, 90, issue.message, issue);
    if (result.state == diagnostics::result_state::cancelled)
      emit(operation_event_kind::finished, 100, "Check cancelled");
    else
    {
      result.exit_status = result.state == diagnostics::result_state::complete ? 0 : 1;
      emit(operation_event_kind::finished, 100,
           *result.exit_status == 0 ? "Check passed" : "Check failed");
    }
    return result;
  }
}
