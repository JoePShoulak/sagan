#include "tests.hpp"

#include "../modules/resolver.hpp"
#include "../parser/ast_node.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace sagan::language_service
{
  namespace
  {
    auto stable_id(const source::document_uri &uri, const std::string_view name) -> std::string
    {
      constexpr char hex[] = "0123456789abcdef";
      std::string result{"sagan-test-v1:"};
      const auto append = [&](const std::string_view input)
      {
        for (const unsigned char byte : input)
        { result += hex[byte >> 4]; result += hex[byte & 15]; }
      };
      append(uri.value);
      result += ':';
      append(name);
      return result;
    }

    auto suites_for(const std::string_view name) -> std::vector<std::string>
    {
      std::vector<std::string> result;
      std::size_t begin = 0;
      for (auto slash = name.find('/', begin); slash != std::string_view::npos;
           slash = name.find('/', begin))
      {
        result.emplace_back(name.substr(begin, slash - begin));
        begin = slash + 1;
      }
      return result;
    }

    auto capture_project_sources(const std::filesystem::path &entry_or_package,
                                 const source::source_provider &source,
                                 const diagnostics::cancellation_token cancellation)
      -> std::vector<std::pair<std::filesystem::path, std::string>>
    {
      const bool package_path = std::filesystem::is_directory(entry_or_package) ||
                                entry_or_package.filename() == "sagan.toml";
      const auto graph = package_path ? modules::resolve_package(entry_or_package, source, cancellation)
                                      : modules::resolve(entry_or_package, source, cancellation);
      std::vector<std::pair<std::filesystem::path, std::string>> snapshots;
      const auto append = [&](const std::filesystem::path &path)
      {
        const auto loaded = source.read_path(path);
        if (!loaded) throw std::runtime_error(loaded.error->message);
        snapshots.emplace_back(path, std::string(loaded.value->text()));
      };
      if (graph.package) append(graph.package->manifest_path);
      for (const auto &module : graph.modules) append(module.path);
      return snapshots;
    }
  }

  auto discover_document_tests(const source::document_snapshot &document,
                               std::string package, std::string module,
                               const diagnostics::cancellation_token cancellation)
    -> test_discovery_result
  {
    test_discovery_result result;
    const auto analyzed = syntax::analyze(document, {.recover = true}, cancellation);
    result.state = analyzed.state;
    result.diagnostics = analyzed.diagnostics;
    if (!analyzed.value || cancellation.is_cancelled())
    { result.state = diagnostics::result_state::cancelled; return result; }
    const auto *tree = analyzed.value->strict_ast ? analyzed.value->strict_ast.get() :
                       analyzed.value->recovered_ast.get();
    if (!tree) return result;
    if (module.empty())
    {
      for (const auto &statement : tree->statements)
        if (const auto *declared = dynamic_cast<const parser::module_declaration *>(statement.get()))
        { module = declared->name; break; }
      if (module.empty() && document.identity().canonical_path)
        module = document.identity().canonical_path->stem().string();
      if (module.empty()) module = "untitled";
    }
    std::set<std::string> names;
    for (const auto &statement : tree->statements)
    {
      if (cancellation.is_cancelled())
      { result.state = diagnostics::result_state::cancelled; result.tests.clear(); return result; }
      const auto *function = dynamic_cast<const parser::function_declaration *>(statement.get());
      if (!function || !function->test_name || !function->test_name_range) continue;
      if (!names.insert(*function->test_name).second) continue;
      const auto declaration = source::source_range{document.identity().id,
          {static_cast<source::byte_offset>(function->range.begin),
           static_cast<source::byte_offset>(function->range.end)}};
      const auto name_range = source::source_range{document.identity().id,
          {static_cast<source::byte_offset>(function->test_name_range->begin),
           static_cast<source::byte_offset>(function->test_name_range->end)}};
      const auto begin = document.to_utf16(name_range.bytes.begin);
      const auto end = document.to_utf16(name_range.bytes.end);
      if (!begin || !end) continue;
      result.tests.push_back({stable_id(document.identity().uri, *function->test_name),
                              *function->test_name, suites_for(*function->test_name), package, module,
                              document.identity().uri, declaration, name_range,
                              *begin, *end, document.version()});
    }
    return result;
  }

  auto discover_project_tests(const std::filesystem::path &entry_or_package,
                              const source::source_provider &source,
                              const diagnostics::cancellation_token cancellation)
    -> test_discovery_result
  {
    test_discovery_result result;
    if (cancellation.is_cancelled())
    { result.state = diagnostics::result_state::cancelled; return result; }
    try
    {
      const bool package_path = std::filesystem::is_directory(entry_or_package) ||
                                entry_or_package.filename() == "sagan.toml";
      const auto graph = package_path ? modules::resolve_package(entry_or_package, source, cancellation)
                                      : modules::resolve(entry_or_package, source, cancellation);
      result.state = diagnostics::result_state::complete;
      for (const auto &module : graph.modules)
      {
        if (cancellation.is_cancelled())
        { result.state = diagnostics::result_state::cancelled; result.tests.clear(); return result; }
        const auto document = source.read_path(module.path);
        if (!document) throw std::runtime_error(document.error->message);
        auto discovered = discover_document_tests(*document.value,
            graph.package ? graph.package->name : "local", module.name, cancellation);
        if (discovered.state != diagnostics::result_state::complete)
          result.state = discovered.state;
        result.tests.insert(result.tests.end(),
            std::make_move_iterator(discovered.tests.begin()),
            std::make_move_iterator(discovered.tests.end()));
        result.diagnostics.insert(result.diagnostics.end(),
            std::make_move_iterator(discovered.diagnostics.begin()),
            std::make_move_iterator(discovered.diagnostics.end()));
      }
      if (cancellation.is_cancelled())
      { result.state = diagnostics::result_state::cancelled; result.tests.clear(); }
    }
    catch (const std::exception &failure)
    {
      if (cancellation.is_cancelled())
      { result.state = diagnostics::result_state::cancelled; result.tests.clear(); return result; }
      result.state = diagnostics::result_state::incomplete;
      result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
          diagnostics::severity::error, diagnostics::phase::project, {}, failure.what(), {}, {}, {}});
    }
    return result;
  }

  auto test_case_state_name(const test_case_state state) -> std::string_view
  {
    switch (state)
    {
      case test_case_state::queued: return "queued";
      case test_case_state::running: return "running";
      case test_case_state::passed: return "passed";
      case test_case_state::failed: return "failed";
      case test_case_state::errored: return "errored";
      case test_case_state::skipped: return "skipped";
      case test_case_state::cancelled: return "cancelled";
    }
    return "errored";
  }

  auto run_document_tests(const source::document_snapshot &document,
                          const std::filesystem::path &artifact_root,
                          const std::vector<std::string> &selected_ids,
                          const diagnostics::cancellation_token cancellation,
                          const test_observer &observer) -> test_run_result
  {
    test_run_result result;
    auto discovered = discover_document_tests(document, "local", {}, cancellation);
    result.state = discovered.state;
    result.diagnostics = std::move(discovered.diagnostics);
    if (result.state != diagnostics::result_state::complete) return result;
    const bool all_tests = selected_ids.empty();
    std::unordered_set<std::string> requested(selected_ids.begin(), selected_ids.end());
    for (const auto &test : discovered.tests)
    {
      if (all_tests || requested.contains(test.id))
      {
        requested.erase(test.id);
        result.tests.emplace_back(test);
      }
    }
    if (!requested.empty())
    {
      result.state = diagnostics::result_state::incomplete;
      result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
          diagnostics::severity::error, diagnostics::phase::project, {},
          "A selected test ID is not present in this document version", {}, {}, {}});
      result.tests.clear();
      return result;
    }
    if (observer)
      for (const auto &test : result.tests) observer(test);
    const auto parsed = syntax::analyze(document, {.recover = false}, cancellation);
    if (parsed.state != diagnostics::result_state::complete || !parsed.value || !parsed.value->strict_ast)
    {
      result.state = parsed.state;
      result.diagnostics.insert(result.diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
      result.tests.clear();
      return result;
    }
    std::unordered_map<std::string, std::string> internal_names;
    for (const auto &statement : parsed.value->strict_ast->statements)
      if (const auto *function = dynamic_cast<const parser::function_declaration *>(statement.get());
          function && function->test_name)
        internal_names.emplace(*function->test_name, function->name);

    for (std::size_t index = 0; index < result.tests.size(); ++index)
    {
      auto &test = result.tests[index];
      if (cancellation.is_cancelled())
      {
        result.state = diagnostics::result_state::cancelled;
        test.state = test_case_state::cancelled;
        if (observer) observer(test);
        for (++index; index < result.tests.size(); ++index)
        {
          result.tests[index].state = test_case_state::skipped;
          if (observer) observer(result.tests[index]);
        }
        break;
      }
      test.state = test_case_state::running;
      if (observer) observer(test);
      const auto started = std::chrono::steady_clock::now();
      const auto native_observer = [&](const operation_event &event)
      {
        if (event.kind == operation_event_kind::standard_output)
          test.standard_output += event.text;
        else if (event.kind == operation_event_kind::standard_error)
          test.standard_error += event.text;
        else return;
        if (observer) observer(test);
      };
      const auto native = run_document(document, artifact_root, native_build_profile::debug,
          cancellation, native_observer, {}, internal_names.at(test.test.name));
      test.duration_milliseconds = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started).count());
      test.exit_status = native.exit_status;
      test.standard_output = native.standard_output;
      test.standard_error = native.standard_error;
      test.output_truncated = native.output_truncated;
      result.diagnostics.insert(result.diagnostics.end(), native.diagnostics.begin(),
                                native.diagnostics.end());
      if (cancellation.is_cancelled() || native.state == diagnostics::result_state::cancelled)
      { test.state = test_case_state::cancelled; result.state = diagnostics::result_state::cancelled; }
      else if (native.exit_status == 0 && native.state == diagnostics::result_state::complete)
        test.state = test_case_state::passed;
      else if (const auto failure = std::find_if(native.diagnostics.begin(), native.diagnostics.end(),
                   [](const auto &issue) { return issue.code == "SAG-RUN-0200"; });
               failure != native.diagnostics.end())
      {
        test.state = test_case_state::failed;
        test.message = failure->message;
        result.state = diagnostics::result_state::incomplete;
      }
      else
      {
        test.state = test_case_state::errored;
        test.message = !native.diagnostics.empty() ? native.diagnostics.front().message :
                       test.standard_error.empty() ? "Test process failed" : test.standard_error;
        result.state = diagnostics::result_state::incomplete;
      }
      if (observer) observer(test);
    }
    return result;
  }

  auto run_project_tests(const std::filesystem::path &entry_or_package,
                         const source::source_provider &source,
                         const std::filesystem::path &artifact_root,
                         const std::vector<std::string> &selected_ids,
                         const diagnostics::cancellation_token cancellation,
                         const test_observer &observer) -> test_run_result
  {
    test_run_result result;
    std::vector<std::pair<std::filesystem::path, std::string>> baseline;
    try { baseline = capture_project_sources(entry_or_package, source, cancellation); }
    catch (const std::exception &failure)
    {
      result.state = cancellation.is_cancelled() ? diagnostics::result_state::cancelled :
                                                     diagnostics::result_state::incomplete;
      if (!cancellation.is_cancelled())
        result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
            diagnostics::severity::error, diagnostics::phase::project, {}, failure.what(), {}, {}, {}});
      return result;
    }
    auto discovered = discover_project_tests(entry_or_package, source, cancellation);
    result.state = discovered.state;
    result.diagnostics = std::move(discovered.diagnostics);
    if (result.state != diagnostics::result_state::complete) return result;
    std::unordered_set<std::string> requested(selected_ids.begin(), selected_ids.end());
    for (const auto &test : discovered.tests)
      if (selected_ids.empty() || requested.erase(test.id) != 0) result.tests.emplace_back(test);
    if (!requested.empty())
    {
      result.state = diagnostics::result_state::incomplete;
      result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
          diagnostics::severity::error, diagnostics::phase::project, {},
          "A selected test ID is not present in this project version", {}, {}, {}});
      result.tests.clear();
      return result;
    }

    // The discovery snapshot is authoritative for the entire run. Recheck every
    // resolved module, including helper modules with no tests, before each case.
    std::vector<std::pair<std::filesystem::path, std::string>> snapshots;
    try { snapshots = capture_project_sources(entry_or_package, source, cancellation); }
    catch (const std::exception &failure)
    {
      result.state = cancellation.is_cancelled() ? diagnostics::result_state::cancelled :
                                                     diagnostics::result_state::incomplete;
      if (!cancellation.is_cancelled())
        result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
            diagnostics::severity::error, diagnostics::phase::project, {}, failure.what(), {}, {}, {}});
      result.tests.clear();
      return result;
    }
    if (snapshots != baseline)
    { result.state = diagnostics::result_state::stale; result.tests.clear(); return result; }
    const auto current = [&]()
    {
      for (const auto &[path, text] : snapshots)
      {
        const auto loaded = source.read_path(path);
        if (!loaded || loaded.value->text() != text) return false;
      }
      return true;
    };
    if (observer)
      for (const auto &test : result.tests) observer(test);
    for (std::size_t index = 0; index < result.tests.size(); ++index)
    {
      auto &test = result.tests[index];
      if (cancellation.is_cancelled() || !current())
      {
        result.state = cancellation.is_cancelled() ? diagnostics::result_state::cancelled :
                                                       diagnostics::result_state::stale;
        test.state = cancellation.is_cancelled() ? test_case_state::cancelled : test_case_state::skipped;
        if (observer) observer(test);
        for (++index; index < result.tests.size(); ++index)
        { result.tests[index].state = test_case_state::skipped; if (observer) observer(result.tests[index]); }
        break;
      }
      test.state = test_case_state::running;
      if (observer) observer(test);
      const auto started = std::chrono::steady_clock::now();
      const auto path = source.canonicalize(test.test.uri);
      if (!path)
      {
        test.state = test_case_state::errored;
        test.message = path.error->message;
        result.state = diagnostics::result_state::incomplete;
        if (observer) observer(test);
        continue;
      }
      const auto native_observer = [&](const operation_event &event)
      {
        if (event.kind == operation_event_kind::standard_output) test.standard_output += event.text;
        else if (event.kind == operation_event_kind::standard_error) test.standard_error += event.text;
        else return;
        if (observer) observer(test);
      };
      const auto native = run_project(entry_or_package, source, artifact_root, native_build_profile::debug,
          cancellation, native_observer, {}, project_test_selection{*path.value, test.test.name});
      test.duration_milliseconds = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started).count());
      test.exit_status = native.exit_status;
      test.standard_output = native.standard_output;
      test.standard_error = native.standard_error;
      test.output_truncated = native.output_truncated;
      result.diagnostics.insert(result.diagnostics.end(), native.diagnostics.begin(), native.diagnostics.end());
      if (cancellation.is_cancelled() || native.state == diagnostics::result_state::cancelled)
      { test.state = test_case_state::cancelled; result.state = diagnostics::result_state::cancelled; }
      else if (native.state == diagnostics::result_state::stale || !current())
      { test.state = test_case_state::skipped; result.state = diagnostics::result_state::stale; }
      else if (native.exit_status == 0 && native.state == diagnostics::result_state::complete)
        test.state = test_case_state::passed;
      else if (const auto failure = std::find_if(native.diagnostics.begin(), native.diagnostics.end(),
                   [](const auto &issue) { return issue.code == "SAG-RUN-0200"; });
               failure != native.diagnostics.end())
      {
        test.state = test_case_state::failed;
        test.message = failure->message;
        result.state = diagnostics::result_state::incomplete;
      }
      else
      {
        test.state = test_case_state::errored;
        test.message = !native.diagnostics.empty() ? native.diagnostics.front().message :
                       test.standard_error.empty() ? "Test process failed" : test.standard_error;
        result.state = diagnostics::result_state::incomplete;
      }
      if (observer) observer(test);
      if (result.state == diagnostics::result_state::cancelled ||
          result.state == diagnostics::result_state::stale)
      {
        for (++index; index < result.tests.size(); ++index)
        { result.tests[index].state = test_case_state::skipped; if (observer) observer(result.tests[index]); }
        break;
      }
    }
    if (!cancellation.is_cancelled() && result.state != diagnostics::result_state::stale && !current())
    { result.state = diagnostics::result_state::stale; result.tests.clear(); }
    return result;
  }
}
