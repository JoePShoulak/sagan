#include "../src/language_service/operations.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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
       "fun main(): Int {\n  print(\"Phase 7 says hello\")\n  return 0\n}\n"});
  std::size_t streamed = 0;
  const auto result = language_service::run_document(
      valid, "build/operations-test", language_service::native_build_profile::debug, {},
      [&](const auto &event)
      {
        if (event.kind == language_service::operation_event_kind::standard_output)
          streamed += event.text.size();
      });
  require(result.state == diagnostics::result_state::complete && result.exit_status == 0 &&
              result.generated && result.generated_source && result.executable &&
              result.debug && !result.debug->functions.empty() &&
              std::any_of(result.debug->functions.begin(), result.debug->functions.end(),
                          [](const auto &function)
                          { return function.generated_name == "main" && function.symbol.has_value(); }) &&
              result.debug->breakpoints.size() >= 2 &&
              std::filesystem::is_regular_file(*result.generated_source) &&
              std::filesystem::is_regular_file(*result.executable) &&
              result.standard_output.find("Phase 7 says hello") != std::string::npos &&
              streamed >= result.standard_output.size() && !result.output_truncated,
          "native operation did not build, run, capture output, and return artifacts");
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
       "fun main(): Int => missing\n"});
  const auto rejected = language_service::build_document(invalid, "build/operations-test");
  require(rejected.state == diagnostics::result_state::incomplete && rejected.exit_status == 1 &&
              !rejected.diagnostics.empty() && !rejected.generated_source &&
              !language_service::plan_debug_launch(rejected),
          "native build compiled invalid Sagan source");
  const source::document_snapshot module_without_linking(
      {{source::document_id{204}, source::document_uri{"untitled:module-without-linking"}, {}}, 1,
       "module single\nfun main(): Int => 0\n"});
  const auto generated_failure = language_service::build_document(
      module_without_linking, "build/operations-test");
  require(generated_failure.state == diagnostics::result_state::incomplete &&
              !generated_failure.diagnostics.empty() &&
              generated_failure.diagnostics.front().primary.document ==
                  module_without_linking.identity().id,
          "generated-code failure did not retain source identity");
  const std::string failure_text =
      "fun main(): Int {\n  let zero = 0\n  print(10 / zero)\n  return 0\n}\n";
  const source::document_snapshot runtime_failure(
      {{source::document_id{203}, source::document_uri{"untitled:runtime-failure"}, {}}, 1,
       failure_text});
  for (const auto profile : {language_service::native_build_profile::debug,
                             language_service::native_build_profile::optimized})
  {
    const auto failed_run = language_service::run_document(runtime_failure, "build/operations-test", profile);
    require(failed_run.state == diagnostics::result_state::incomplete &&
                failed_run.exit_status && *failed_run.exit_status != 0 &&
                failed_run.diagnostics.size() == 1 &&
                failed_run.diagnostics.front().owner == diagnostics::phase::runtime &&
                failed_run.diagnostics.front().message.find("division by zero") != std::string::npos &&
                failed_run.diagnostics.front().primary.bytes.begin == failure_text.find("print") &&
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
              !stopped_build.exit_status,
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
