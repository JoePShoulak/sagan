#include "../src/language_service/operations.hpp"
#include "../src/language_service/tests.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/modules/package_index.hpp"
#include "../src/syntax/syntax.hpp"

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
  require(language_service::operations_schema_version == "sagan-operations-v2" &&
              success.state == diagnostics::result_state::complete &&
              success.lifecycle == language_service::operation_state::completed &&
              !success.operation_id.empty() &&
              success.exit_status == 0 && success.summary.has_value() &&
              success.diagnostics.empty() && success.analyzed_version == 7 &&
              success.document.id == valid.identity().id &&
              success.document.uri == valid.identity().uri &&
              success.events.size() == 3 && observed.size() == success.events.size() &&
              success.events.front().kind == operation_event_kind::started &&
              success.events.back().kind == operation_event_kind::finished,
          "successful check operation did not expose versioned result and ordered events");
  for (std::size_t i = 0; i < observed.size(); ++i)
    require(observed[i].sequence == i && observed[i].kind == success.events[i].kind &&
                observed[i].operation_id == success.operation_id,
            "operation observer received events out of order");

  auto asynchronous = language_service::start_check_document(valid);
  const auto async_id = asynchronous.id;
  const auto async_result = asynchronous.future.get();
  require(async_result.operation_id == async_id &&
              async_result.lifecycle == language_service::operation_state::completed &&
              asynchronous.current_state() == language_service::operation_state::completed &&
              async_id != success.operation_id,
          "asynchronous check lost operation identity or completion state");
  const auto project_source = std::make_shared<source::disk_source_provider>();
  auto project_check = language_service::start_check_project(
      "tests/fixtures/modules/module_demo/main.sagan", project_source);
  const auto checked_project = project_check.future.get();
  require(checked_project.operation_id == project_check.id &&
              checked_project.lifecycle == language_service::operation_state::completed &&
              checked_project.exit_status == 0 && checked_project.summary,
          "asynchronous project check failed to resolve and analyze modules");
  source::document_store project_overlays;
  const auto imported_path = std::filesystem::absolute(
      "tests/fixtures/modules/module_demo/guidance.sagan").lexically_normal();
  const auto imported_snapshot = project_source->read_path(imported_path);
  require(imported_snapshot &&
              project_overlays.open(imported_snapshot.value->identity().uri, 10,
                                    std::string(imported_snapshot.value->text())),
          "could not open imported module overlay for stale project test");
  const auto changed_dependency = language_service::run_check_project(
      "tests/fixtures/modules/module_demo/main.sagan", project_overlays, {},
      [&](const auto &event)
      {
        if (event.kind == operation_event_kind::progress &&
            event.text == "Analyzing project source")
        {
          const auto updated = project_overlays.replace(imported_snapshot.value->identity().uri,
              10, 11, std::string(imported_snapshot.value->text()) + "\n");
          require(static_cast<bool>(updated), "could not change imported module during project check");
        }
      });
  require(changed_dependency.lifecycle == language_service::operation_state::stale &&
              !changed_dependency.exit_status && changed_dependency.diagnostics.empty(),
          "project check published a result after an imported overlay changed");

  const source::document_snapshot invalid(
      {{source::document_id{2}, source::document_uri{"untitled:check-invalid"}, {}}, 8,
       "fun main(): Int => missing\n"});
  const source::document_snapshot declared_test(
      {{source::document_id{14}, source::document_uri{"untitled:test-declaration"}, {}}, 1,
       "test \"orbit/🚀\" {\n  let value = 42\n  print(value)\n}\n"});
  const auto test_syntax = syntax::analyze(declared_test);
  const auto test_check = language_service::run_check_operation(declared_test);
  require(test_syntax.value && test_syntax.value->strict_ast &&
              test_syntax.value->strict_ast->statements.size() == 1 &&
              dynamic_cast<const parser::function_declaration *>(
                  test_syntax.value->strict_ast->statements.front().get())->test_name == "orbit/🚀" &&
              test_check.lifecycle == language_service::operation_state::completed,
          "explicit Unicode test declaration did not parse and type-check");
  const auto discovered = language_service::discover_document_tests(declared_test);
  const source::document_snapshot moved_test(
      {declared_test.identity(), 2, "\n" + std::string(declared_test.text())});
  const auto rediscovered = language_service::discover_document_tests(moved_test);
  require(language_service::test_schema_version == "sagan-tests-v1" &&
              discovered.state == diagnostics::result_state::complete &&
              discovered.tests.size() == 1 && rediscovered.tests.size() == 1 &&
              discovered.tests.front().id == rediscovered.tests.front().id &&
              discovered.tests.front().name == "orbit/🚀" &&
              discovered.tests.front().suites == std::vector<std::string>{"orbit"} &&
              discovered.tests.front().start.line == 0 &&
              rediscovered.tests.front().start.line == 1 &&
              rediscovered.tests.front().version == 2,
          "test discovery lost Unicode range or stable identity across edits");
  const auto project_tests = language_service::discover_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", *project_source);
  require(project_tests.state == diagnostics::result_state::complete &&
              project_tests.tests.size() == 2 &&
              project_tests.tests.front().module == "tools" &&
              project_tests.tests.back().module == "main",
          "project test discovery missed an imported module");
  const auto dependency_manifest = modules::load_package(
      "tests/fixtures/modules/package_dependency");
  require(dependency_manifest.dependencies.size() == 1 &&
              dependency_manifest.dependencies.front().alias == "physics" &&
              dependency_manifest.dependencies.front().name == "physics" &&
              dependency_manifest.dependencies.front().requirement == "^1.2.3",
          "approved dependency manifest syntax was not parsed");
  bool unresolved_dependency_rejected = false;
  try
  {
    modules::package_resolution_options missing_index;
    missing_index.index_path = "tests/fixtures/missing-index.tsv";
    static_cast<void>(modules::resolve_package("tests/fixtures/modules/package_dependency",
                                               *project_source, {}, missing_index));
  }
  catch (const std::runtime_error &error)
  { unresolved_dependency_rejected = std::string(error.what()).find("missing-index.tsv") != std::string::npos; }
  require(unresolved_dependency_rejected,
          "unresolved external dependency was silently ignored");
  const auto package_index = modules::query_package_index(
      "tests/fixtures/package_index/index.tsv", "2.1.0", "phys");
  require(package_index.state == modules::package_index_state::ready &&
              package_index.packages.size() == 2 &&
              package_index.packages.front().compiler_compatible &&
              !package_index.packages.back().compiler_compatible &&
              modules::version_satisfies("^0.2.3", "0.2.9") &&
              !modules::version_satisfies("^0.2.3", "0.3.0"),
          "versioned local package index lost prefix or compatibility filtering");
  const auto installed_index = modules::query_package_index(
      "tests/fixtures/package_index/index.tsv", "2.1.0", "orbit");
  require(installed_index.state == modules::package_index_state::ready &&
              installed_index.packages.size() == 1 &&
              installed_index.packages.front().install_state == modules::package_install_state::installed,
          "local package index did not validate an installed manifest");
  sagan::diagnostics::cancellation_source stopped_index;
  stopped_index.cancel();
  const auto cancelled_index = modules::query_package_index(
      "tests/fixtures/package_index/index.tsv", "2.1.0", {}, stopped_index.token());
  require(cancelled_index.cancelled && cancelled_index.packages.empty(),
          "Cancelled package-index analysis exposed partial candidates");
  require(modules::query_package_index("tests/fixtures/package_index/absent.tsv", "2.1.0").state ==
              modules::package_index_state::unavailable,
          "missing local package index was not represented explicitly");
  require(modules::query_package_index("tests/fixtures/package_index/invalid.tsv", "2.1.0").state ==
              modules::package_index_state::invalid,
          "package index accepted a manifest path outside its root");
  const source::document_snapshot incomplete_tests(
      {{source::document_id{17}, source::document_uri{"untitled:recover-tests"}, {}}, 1,
       "test \"safe\" {}\nfun unfinished(\n"});
  const auto recovered_tests = language_service::discover_document_tests(incomplete_tests);
  require(recovered_tests.state == diagnostics::result_state::recovered &&
              recovered_tests.tests.size() == 1 &&
              recovered_tests.tests.front().name == "safe",
          "test discovery lost a complete declaration beside incomplete source");
  diagnostics::cancellation_source cancelled_discovery;
  cancelled_discovery.cancel();
  const auto stopped_discovery = language_service::discover_document_tests(
      declared_test, "local", {}, cancelled_discovery.token());
  require(stopped_discovery.state == diagnostics::result_state::cancelled &&
              stopped_discovery.tests.empty(),
          "test discovery ignored cancellation");
  const source::document_snapshot duplicate_test(
      {{source::document_id{15}, source::document_uri{"untitled:duplicate-tests"}, {}}, 1,
       "test \"same\" {}\ntest \"same\" {}\n"});
  require(language_service::run_check_operation(duplicate_test).lifecycle ==
              language_service::operation_state::failed,
          "duplicate test names were accepted");
  const source::document_snapshot empty_suite(
      {{source::document_id{19}, source::document_uri{"untitled:empty-suite"}, {}}, 1,
       "test \"orbit//escape\" {}\n"});
  require(language_service::run_check_operation(empty_suite).lifecycle ==
              language_service::operation_state::failed,
          "empty slash-separated suite component was accepted");
  const source::document_snapshot interpolated_test(
      {{source::document_id{16}, source::document_uri{"untitled:interpolated-test"}, {}}, 1,
       "test \"value ${2}\" {}\n"});
  require(language_service::run_check_operation(interpolated_test).lifecycle ==
              language_service::operation_state::failed,
          "interpolated test name was accepted");
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
  bool traversal_stopped = false;
  try
  {
    static_cast<void>(modules::resolve("tests/fixtures/modules/module_demo/main.sagan",
                                       *project_source, cancelled.token()));
  }
  catch (const std::runtime_error &failure)
  {
    traversal_stopped = std::string(failure.what()) == "Module traversal cancelled";
  }
  require(traversal_stopped, "module resolver ignored a cancellation token");
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
