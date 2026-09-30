#include "../src/language_service/operations.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

namespace
{
  auto require(const bool condition, const char *message) -> void
  {
    if (!condition) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  using namespace sagan;
  using language_service::operation_event_kind;
  const source::document_snapshot valid(
      {{source::document_id{1}, source::document_uri{"untitled:check-valid"}, {}}, 7,
       "fun main(): Int => 0\n"});
  std::vector<language_service::operation_event> observed;
  const auto success = language_service::run_check_operation(
      valid, {.check_types = true, .check_entry_point = true}, {},
      [&](const auto &event) { observed.push_back(event); });
  require(language_service::operations_schema_version == "sagan-operations-v1" &&
              success.state == diagnostics::result_state::complete &&
              success.exit_status == 0 && success.summary.has_value() &&
              success.diagnostics.empty() && success.analyzed_version == 7 &&
              success.document.id == valid.identity().id &&
              success.document.uri == valid.identity().uri &&
              success.events.size() == 3 && observed.size() == success.events.size() &&
              success.events.front().kind == operation_event_kind::started &&
              success.events.back().kind == operation_event_kind::finished,
          "successful check operation did not expose versioned result and ordered events");
  for (std::size_t i = 0; i < observed.size(); ++i)
    require(observed[i].sequence == i && observed[i].kind == success.events[i].kind,
            "operation observer received events out of order");

  const source::document_snapshot invalid(
      {{source::document_id{2}, source::document_uri{"untitled:check-invalid"}, {}}, 8,
       "fun main(): Int => missing\n"});
  const auto failure = language_service::run_check_operation(invalid);
  require(failure.state == diagnostics::result_state::incomplete &&
              failure.exit_status == 1 && !failure.diagnostics.empty() &&
              failure.events.size() == failure.diagnostics.size() + 3 &&
              failure.events[2].kind == operation_event_kind::diagnostic &&
              failure.events[2].issue.has_value() &&
              failure.events[2].issue->code == failure.diagnostics.front().code,
          "failed check operation lost structured diagnostics");

  diagnostics::cancellation_source cancelled;
  cancelled.cancel();
  const auto stopped = language_service::run_check_operation(valid, {}, cancelled.token());
  require(stopped.state == diagnostics::result_state::cancelled &&
              !stopped.exit_status && stopped.diagnostics.empty() &&
              stopped.events.size() == 2 &&
              stopped.events.back().kind == operation_event_kind::finished,
          "cancelled check operation reported a process exit code or diagnostics");
  diagnostics::cancellation_source during_progress;
  const auto interrupted = language_service::run_check_operation(
      valid, {}, during_progress.token(), [&](const auto &event)
      {
        if (event.kind == operation_event_kind::progress) during_progress.cancel();
      });
  require(interrupted.state == diagnostics::result_state::cancelled &&
              !interrupted.exit_status && interrupted.events.size() == 3,
          "check operation ignored cancellation between progress and analysis");
  std::cout << "Sagan input:\n" << valid.text()
            << "Check result: passed (exit " << *success.exit_status << ")\n"
            << "Invalid input:\n" << invalid.text()
            << "Check result: failed (exit " << *failure.exit_status << ")\n"
            << "Diagnostic: " << failure.diagnostics.front().code << " — "
            << failure.diagnostics.front().message << "\n"
            << "Cancelled result: no exit status\n";
  return 0;
}
