#include "../src/codegen/cpp_generator.hpp"
#include "../src/language_service/operations.hpp"
#include "../src/language_service/tests.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

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
  const source::document_snapshot valid(
      {{source::document_id{201}, source::document_uri{"untitled:native-op"}, {}}, 4,
       "fun report(): Int {\n  print(\"Phase 7 says hello\")\n  return 0\n}\nexit(report())\n"});
  const source::document_snapshot source_tests(
      {{source::document_id{215}, source::document_uri{"untitled:native-tests"}, {}}, 1,
       "test \"math/add\" {\n  assert(1 + 1 == 2)\n  print(\"test ran\")\n}\n"
       "test \"math/fail\" { assert(false, \"orbit escaped\") }\n"
       "print(\"root must not run\")\n"});
  const auto known_tests = language_service::discover_document_tests(source_tests);
  require(known_tests.state == diagnostics::result_state::complete && known_tests.tests.size() == 2,
          "native test fixture was not discoverable");
  std::size_t test_events = 0;
  const auto selected = language_service::run_document_tests(source_tests, "build/operations-test",
      {known_tests.tests.front().id}, {}, [&](const auto &) { ++test_events; });
  require(selected.state == diagnostics::result_state::complete && selected.tests.size() == 1 &&
              selected.tests.front().state == language_service::test_case_state::passed &&
              selected.tests.front().standard_output.find("test ran") != std::string::npos &&
              selected.tests.front().standard_output.find("root must not run") == std::string::npos &&
              test_events >= 3,
          "selected Sagan test did not run independently of root script statements");
  const auto all_tests = language_service::run_document_tests(source_tests, "build/operations-test");
  require(all_tests.state == diagnostics::result_state::incomplete && all_tests.tests.size() == 2 &&
              all_tests.tests.front().state == language_service::test_case_state::passed &&
              all_tests.tests.back().state == language_service::test_case_state::failed &&
              all_tests.tests.back().message == "orbit escaped",
          "Sagan test runner did not distinguish pass from assertion failure");
  source::disk_source_provider project_source;
  const auto project_discovery = language_service::discover_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_source);
  require(project_discovery.state == diagnostics::result_state::complete &&
              project_discovery.tests.size() == 2,
          "multi-module project test fixture was not discovered");
  const auto project_all = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_source, "build/operations-test");
  require(project_all.state == diagnostics::result_state::complete && project_all.tests.size() == 2 &&
              project_all.tests.front().state == language_service::test_case_state::passed &&
              project_all.tests.back().state == language_service::test_case_state::passed &&
              project_all.tests.front().standard_output.find('2') != std::string::npos &&
              project_all.tests.back().standard_output.find('1') != std::string::npos,
          "project tests did not execute independently across linked modules");
  const auto package_all = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project", project_source, "build/operations-test");
  require(package_all.state == diagnostics::result_state::complete && package_all.tests.size() == 2 &&
              package_all.tests.front().test.package == "tests-project",
          "package-root test execution lost package identity");
  const auto project_selected = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_source, "build/operations-test",
      {project_discovery.tests.front().id});
  require(project_selected.state == diagnostics::result_state::complete &&
              project_selected.tests.size() == 1 &&
              project_selected.tests.front().test.id == project_discovery.tests.front().id,
          "selected project test did not retain stable discovery identity");
  const auto project_unknown = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_source, "build/operations-test",
      {"sagan-test-v1:missing"});
  require(project_unknown.state == diagnostics::result_state::incomplete &&
              project_unknown.tests.empty() && !project_unknown.diagnostics.empty(),
          "unknown project test ID was not rejected before execution");
  diagnostics::cancellation_source project_cancel;
  project_cancel.cancel();
  const auto cancelled_project = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_source,
      "build/operations-test", {}, project_cancel.token());
  require(cancelled_project.state == diagnostics::result_state::cancelled &&
              cancelled_project.tests.empty(),
          "cancelled project test discovery attempted execution");
  diagnostics::cancellation_source active_project_cancel;
  bool requested_cancel = false;
  const auto cancelled_between_tests = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project", project_source, "build/operations-test", {},
      active_project_cancel.token(), [&](const auto &event)
      {
        if (!requested_cancel && event.state == language_service::test_case_state::queued)
        { requested_cancel = true; active_project_cancel.cancel(); }
      });
  require(cancelled_between_tests.state == diagnostics::result_state::cancelled &&
              cancelled_between_tests.tests.size() == 2 &&
              cancelled_between_tests.tests.front().state == language_service::test_case_state::cancelled &&
              cancelled_between_tests.tests.back().state == language_service::test_case_state::skipped,
          "project runner did not cancel before launching its next test");
  source::document_store project_overlays;
  const auto imported_test_path = std::filesystem::absolute(
      "tests/fixtures/modules/tests_project/tools.sagan").lexically_normal();
  const auto imported_snapshot = project_overlays.read_path(imported_test_path);
  require(imported_snapshot && static_cast<bool>(project_overlays.open(
              imported_snapshot.value->identity().uri, 10,
              "module tools\ntest \"math/add\" { assert(false, \"overlay failed\") }\n")),
          "could not open unsaved imported test overlay");
  const auto overlay_test = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_overlays,
      "build/operations-test", {project_discovery.tests.front().id});
  require(overlay_test.state == diagnostics::result_state::incomplete &&
              overlay_test.tests.size() == 1 &&
              overlay_test.tests.front().state == language_service::test_case_state::failed &&
              overlay_test.tests.front().message == "overlay failed",
          "project runner ignored an unsaved imported-module overlay");
  bool changed_during_discovery = false;
  const auto stale_project_tests = language_service::run_project_tests(
      "tests/fixtures/modules/tests_project/main.sagan", project_overlays,
      "build/operations-test", {}, {}, [&](const auto &event)
      {
        if (!changed_during_discovery && event.state == language_service::test_case_state::queued)
        {
          changed_during_discovery = true;
          const auto changed = project_overlays.replace(imported_snapshot.value->identity().uri, 10, 11,
              "module tools\ntest \"math/add\" { print(99) }\n");
          require(static_cast<bool>(changed), "could not change imported overlay during test run");
        }
      });
  require(stale_project_tests.state == diagnostics::result_state::stale &&
              !stale_project_tests.tests.empty() &&
              stale_project_tests.tests.front().state == language_service::test_case_state::skipped,
          "project runner published stale test results after an overlay edit");
  std::size_t streamed = 0;
  const auto result = language_service::run_document(
      valid, "build/operations-test", language_service::native_build_profile::debug, {},
      [&](const auto &event)
      {
        if (event.kind == language_service::operation_event_kind::standard_output)
          streamed += event.text.size();
      });
  if (result.state != diagnostics::result_state::complete || result.exit_status != 0)
  {
    std::cerr << "Native operation state: " << static_cast<int>(result.state)
              << ", exit: " << (result.exit_status ? std::to_string(*result.exit_status) : "none")
              << ", stderr: " << result.standard_error << '\n';
    for (const auto &issue : result.diagnostics) std::cerr << issue.message << '\n';
  }
  require(result.state == diagnostics::result_state::complete && result.exit_status == 0 &&
              result.generated && result.generated_source && result.executable &&
              result.debug && !result.debug->functions.empty() &&
              std::any_of(result.debug->functions.begin(), result.debug->functions.end(),
                          [](const auto &function)
                          { return function.generated_name == codegen::generated_identifier("report") &&
                                   function.symbol.has_value(); }) &&
              result.debug->breakpoints.size() >= 2 &&
              std::filesystem::is_regular_file(*result.generated_source) &&
              std::filesystem::is_regular_file(*result.executable) &&
              result.standard_output.find("Phase 7 says hello") != std::string::npos &&
              streamed >= result.standard_output.size() && !result.output_truncated,
          "native operation did not build, run, capture output, and return artifacts");
#ifdef _WIN32
  const HMODULE image = LoadLibraryExW(result.executable->c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
  require(image != nullptr, "could not inspect the generated Windows executable");
  const bool has_icon = FindResourceW(image, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(14)) != nullptr;
  FreeLibrary(image);
  require(has_icon, "generated Windows executable is missing the Sagan icon");
#endif
  auto asynchronous = language_service::start_run_document(valid, "build/operations-test");
  const auto asynchronous_result = asynchronous.future.get();
  require(asynchronous_result.operation_id == asynchronous.id &&
              asynchronous_result.lifecycle == language_service::operation_state::completed &&
              asynchronous_result.exit_status == 0 &&
              asynchronous_result.standard_output.find("Phase 7 says hello") != std::string::npos,
          "asynchronous run lost its stable identity or native result");
  const source::document_snapshot dormant_test(
      {{source::document_id{206}, source::document_uri{"untitled:dormant-test"}, {}}, 1,
       "test \"not automatic\" {\n  print(999)\n}\nprint(42)\n"});
  const auto ordinary_run = language_service::run_document(dormant_test, "build/operations-test");
  require(ordinary_run.state == diagnostics::result_state::complete &&
              ordinary_run.exit_status == 0 &&
              ordinary_run.standard_output.find("42") != std::string::npos &&
              ordinary_run.standard_output.find("999") == std::string::npos,
          "ordinary execution invoked a test declaration automatically");
  const auto launch = language_service::plan_debug_launch(result);
  require(launch && launch->executable == result.executable &&
              launch->working_directory == result.executable->parent_path() &&
              launch->profile == language_service::native_build_profile::debug,
          "completed native build did not expose a debug launch plan");
  const auto optimized = language_service::run_document(
      valid, "build/operations-test", language_service::native_build_profile::optimized);
  require(optimized.state == diagnostics::result_state::complete &&
              optimized.exit_status == 0 && optimized.generated && optimized.debug &&
              optimized.standard_output == result.standard_output &&
              !optimized.generated->mappings.empty() &&
              optimized.debug->functions.size() == result.debug->functions.size() &&
              optimized.debug->breakpoints.size() == result.debug->breakpoints.size() &&
              language_service::plan_debug_launch(optimized) &&
              language_service::plan_debug_launch(optimized)->profile ==
                  language_service::native_build_profile::optimized,
          "optimized native operation changed behavior or lost source mapping");
  const source::document_snapshot invalid(
      {{source::document_id{202}, source::document_uri{"untitled:invalid-native-op"}, {}}, 1,
       "fun broken(): Int => missing\n"});
  const auto rejected = language_service::build_document(invalid, "build/operations-test");
  require(rejected.state == diagnostics::result_state::incomplete && rejected.exit_status == 1 &&
              !rejected.diagnostics.empty() && !rejected.generated_source &&
              !language_service::plan_debug_launch(rejected),
          "native build compiled invalid Sagan source");
  const source::document_snapshot module_without_linking(
      {{source::document_id{204}, source::document_uri{"untitled:module-without-linking"}, {}}, 1,
       "module single\nfun report(): Int => 0\n"});
  const auto generated_failure = language_service::build_document(
      module_without_linking, "build/operations-test");
  require(generated_failure.state == diagnostics::result_state::incomplete &&
              !generated_failure.diagnostics.empty() &&
              generated_failure.diagnostics.front().primary.document ==
                  module_without_linking.identity().id,
          "generated-code failure did not retain source identity");
  const std::string failure_text =
      "fun report(): Int {\n  let zero = 0\n  print(10 / zero)\n  return 0\n}\nexit(report())\n";
  const source::document_snapshot runtime_failure(
      {{source::document_id{203}, source::document_uri{"untitled:runtime-failure"}, {}}, 1,
       failure_text});
  for (const auto profile : {language_service::native_build_profile::debug,
                             language_service::native_build_profile::optimized})
  {
    const auto failed_run = language_service::run_document(runtime_failure, "build/operations-test", profile);
    const auto shared_runtime_issue = language_service::map_runtime_failure(
        runtime_failure, failed_run.standard_error);
    require(failed_run.state == diagnostics::result_state::incomplete &&
                failed_run.exit_status && *failed_run.exit_status != 0 &&
                failed_run.diagnostics.size() == 1 &&
                failed_run.diagnostics.front().owner == diagnostics::phase::runtime &&
                failed_run.diagnostics.front().message.find("division by zero") != std::string::npos &&
                failed_run.diagnostics.front().primary.bytes.begin == failure_text.find("print") &&
                shared_runtime_issue && shared_runtime_issue->code == failed_run.diagnostics.front().code &&
                shared_runtime_issue->primary == failed_run.diagnostics.front().primary &&
                failed_run.debug &&
                std::any_of(failed_run.debug->variables.begin(), failed_run.debug->variables.end(),
                            [](const auto &variable)
                            {
                              return variable.name == "zero" &&
                                     variable.representation ==
                                         language_service::debug_value_representation::shared_value &&
                                     !variable.available_in_optimized;
                            }) &&
                std::any_of(failed_run.debug->expression_hooks.begin(),
                            failed_run.debug->expression_hooks.end(),
                            [](const auto &hook)
                            { return hook.generated_expression == "*sagan_7a65726f"; }),
            "runtime failure did not map back to a Sagan statement in both build profiles");
  }
  diagnostics::cancellation_source cancelled;
  cancelled.cancel();
  const auto stopped = language_service::run_document(valid, "build/operations-test",
                                                        language_service::native_build_profile::debug,
                                                        cancelled.token());
  require(stopped.state == diagnostics::result_state::cancelled && !stopped.exit_status &&
              !stopped.generated_source,
          "pre-cancelled native operation wrote an artifact");
  diagnostics::cancellation_source compile_cancel;
  const auto stopped_build = language_service::build_document(
      valid, "build/operations-test", language_service::native_build_profile::debug,
      compile_cancel.token(), [&](const auto &event)
      {
        if (event.kind == language_service::operation_event_kind::progress &&
            event.text == "Compiling native program") compile_cancel.cancel();
      });
  require(stopped_build.state == diagnostics::result_state::cancelled &&
              !stopped_build.exit_status && !stopped_build.generated_source &&
              !stopped_build.executable,
          "native build did not honor cancellation at the compiler boundary");
  const source::disk_source_provider disk;
  const auto runtime_fixture = disk.read_path("tests/fixtures/runtime/runtime_errors.sagan");
  require(static_cast<bool>(runtime_fixture), "runtime operation fixture was not found");
  const auto guarded = language_service::run_document(*runtime_fixture.value,
                                                       "build/operations-test");
  require(guarded.state == diagnostics::result_state::complete &&
              guarded.exit_status == 0 &&
              guarded.standard_output.find("Caught: 4; cleanup: 1") != std::string::npos &&
              guarded.diagnostics.empty(),
          "mapped code generation changed Sagan exception handling or cleanup");
  const auto project = language_service::run_project(
      "tests/fixtures/modules/module_demo/main.sagan", disk, "build/operations-test");
  const auto guidance = disk.read_path("tests/fixtures/modules/module_demo/guidance.sagan");
  require(project.state == diagnostics::result_state::complete &&
              project.exit_status == 0 && project.generated && project.debug &&
              guidance &&
              project.standard_output.find("Cross-module answer: 42") != std::string::npos &&
              std::any_of(project.generated->mappings.begin(), project.generated->mappings.end(),
                          [](const auto &entry)
                          { return entry.source_path &&
                                   entry.source_path->filename() == "guidance.sagan"; }),
          "linked project operation did not build and run modules with provenance");
  require(std::any_of(project.debug->breakpoints.begin(), project.debug->breakpoints.end(),
                      [&](const auto &breakpoint)
                      { return breakpoint.source_path &&
                               breakpoint.source_path->filename() == "guidance.sagan" &&
                               breakpoint.source.document == guidance.value->identity().id; }) &&
              std::any_of(project.debug->variables.begin(), project.debug->variables.end(),
                          [&](const auto &variable)
                          { return variable.name == "answer" &&
                                   variable.lifetime.document == project.document.id; }),
          "linked debug metadata did not retain source identities for breakpoints and locals");
  const auto package = language_service::run_project(
      "examples/package/sagan.toml", disk, "build/operations-test",
      language_service::native_build_profile::optimized);
  require(package.state == diagnostics::result_state::complete &&
              package.exit_status == 0 &&
              package.standard_output.find("Package answer: 42") != std::string::npos,
          "package operation did not use the package entry and linked modules");
  source::document_store overlays;
  std::string altered_guidance(guidance.value->text());
  const auto old_expression = altered_guidance.find("helper(value) + offset()");
  require(old_expression != std::string::npos, "guidance overlay fixture changed");
  altered_guidance.replace(old_expression, std::string("helper(value) + offset()").size(),
                           "helper(value) / (value - value)");
  require(static_cast<bool>(overlays.open(guidance.value->identity().uri, 2,
                                          altered_guidance)),
          "could not open unsaved guidance overlay");
  const auto overlaid_guidance = overlays.read_path(
      "tests/fixtures/modules/module_demo/guidance.sagan");
  const auto overlaid_project = language_service::run_project(
      "tests/fixtures/modules/module_demo/main.sagan", overlays, "build/operations-test");
  require(overlaid_guidance && overlaid_project.state == diagnostics::result_state::incomplete &&
              overlaid_project.exit_status && *overlaid_project.exit_status != 0 &&
              overlaid_project.diagnostics.size() == 1 &&
              overlaid_project.diagnostics.front().owner == diagnostics::phase::runtime &&
              overlaid_project.diagnostics.front().primary.document ==
                  overlaid_guidance.value->identity().id &&
              overlaid_project.diagnostics.front().primary.bytes.begin == old_expression,
          "unsaved module runtime failure did not map to its source document");
  bool changed_during_build = false;
  const auto stale_project = language_service::build_project(
      "tests/fixtures/modules/module_demo/main.sagan", overlays, "build/operations-test",
      language_service::native_build_profile::debug, {}, [&](const auto &event)
      {
        if (!changed_during_build && event.kind == language_service::operation_event_kind::progress &&
            event.text == "Compiling native program")
        {
          changed_during_build = static_cast<bool>(overlays.replace(
              guidance.value->identity().uri, 2, 3, std::string(guidance.value->text())));
        }
      });
  require(changed_during_build && stale_project.state == diagnostics::result_state::stale &&
              !stale_project.exit_status,
          "project build published a result after an imported overlay changed");
  std::cout << "Sagan input:\n" << valid.text()
            << "Debug output:\n" << result.standard_output
            << "Optimized output:\n" << optimized.standard_output
            << "Native exit status: " << *result.exit_status << '\n';
  return 0;
}
