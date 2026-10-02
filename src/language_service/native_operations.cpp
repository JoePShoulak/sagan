#include "operations.hpp"

#include "../driver/native_runner.hpp"
#include "../driver/process.hpp"
#include "../modules/resolver.hpp"
#include "../semantic/analyzer.hpp"
#include "../semantic/semantic_error.hpp"
#include "../source/provider.hpp"
#include "../semantic/type_checker.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sagan::language_service
{
  namespace
  {
    auto emit(native_operation_result &result, const operation_observer &observer,
              const operation_event_kind kind, const int percent, std::string text,
              std::optional<diagnostics::diagnostic> issue = {}) -> void
    {
      if (kind == operation_event_kind::started || kind == operation_event_kind::progress)
        result.lifecycle = operation_state::running;
      else if (kind == operation_event_kind::finished)
      {
        switch (result.state)
        {
          case diagnostics::result_state::complete: result.lifecycle = operation_state::completed; break;
          case diagnostics::result_state::cancelled: result.lifecycle = operation_state::cancelled; break;
          case diagnostics::result_state::stale: result.lifecycle = operation_state::stale; break;
          case diagnostics::result_state::incomplete:
          case diagnostics::result_state::recovered: result.lifecycle = operation_state::failed; break;
        }
      }
      result.events.push_back({kind, result.events.size(), percent, std::move(text), std::move(issue),
                               result.operation_id, result.lifecycle});
      if (observer) observer(result.events.back());
    }

    auto build_issue(const source::document_snapshot &document, std::string message,
                     const source::byte_range range = {}) -> diagnostics::diagnostic
    {
      return {std::string(diagnostics::default_code(diagnostics::phase::build)),
              diagnostics::severity::error, diagnostics::phase::build,
              {document.identity().id, range}, std::move(message), {}, {}, {}};
    }

    auto semantic_issue(const source::document_snapshot &document,
                        const semantic::semantic_error &error,
                        const source::source_provider *provider = nullptr) -> diagnostics::diagnostic
    {
      const auto backend = std::string_view(error.what()).starts_with("C++ backend:");
      const auto phase = backend ? diagnostics::phase::build : diagnostics::phase::semantic;
      source::document_id id = document.identity().id;
      std::size_t text_size = document.text().size();
      if (provider && error.origin_path)
      {
        const auto loaded = provider->read_path(*error.origin_path);
        if (loaded)
        {
          id = loaded.value->identity().id;
          text_size = loaded.value->text().size();
        }
      }
      source::byte_range range{};
      if ((!provider || error.origin_path) && error.range.begin >= 0 &&
          error.range.end >= error.range.begin &&
          static_cast<std::size_t>(error.range.end) <= text_size)
        range = {static_cast<source::byte_offset>(error.range.begin),
                 static_cast<source::byte_offset>(error.range.end)};
      diagnostics::diagnostic issue{
          std::string(diagnostics::default_code(phase)), diagnostics::severity::error, phase,
          {id, range}, error.what(), {}, {}, {}};
      if (error.origin_path) issue.notes.push_back("Sagan source: " + error.origin_path->string());
      return issue;
    }

    auto unique_artifact_directory(const std::filesystem::path &root,
                                   const source::document_snapshot &document) -> std::filesystem::path
    {
      if (root.empty()) throw std::runtime_error("Artifact root must be specified");
      std::filesystem::create_directories(root);
      // Editor runs can build the same document/version many times across
      // server restarts. A run-specific suffix avoids exhausting 0..99.
      const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
      for (int attempt = 0; attempt < 100; ++attempt)
      {
        const auto candidate = root / ("sagan-" + std::to_string(document.identity().id.value) +
                                       "-" + std::to_string(document.version()) +
                                       "-" + std::to_string(nonce) + "-" + std::to_string(attempt));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error))
          return std::filesystem::absolute(candidate);
        if (error && error != std::errc::file_exists)
          throw std::runtime_error("Could not create native artifact directory: " + error.message());
      }
      throw std::runtime_error("Could not allocate a native artifact directory");
    }

    auto flag_words(const std::string &flags) -> std::vector<std::string>
    {
      std::vector<std::string> result;
      std::string word;
      char quote = '\0';
      bool started = false;
      for (const char character : flags)
      {
        if ((character == '\'' || character == '"') && (quote == '\0' || quote == character))
        {
          quote = quote == '\0' ? character : '\0';
          started = true;
          continue;
        }
        if (quote == '\0' && std::isspace(static_cast<unsigned char>(character)))
        {
          if (started) { result.push_back(std::move(word)); word.clear(); started = false; }
          continue;
        }
        word += character;
        started = true;
      }
      if (quote != '\0') throw std::runtime_error("Unclosed quote in SAGAN_CXXFLAGS");
      if (started) result.push_back(std::move(word));
      return result;
    }

    auto path_utf8(const std::filesystem::path &path) -> std::string
    {
      const auto encoded = path.u8string();
      return {reinterpret_cast<const char *>(encoded.data()), encoded.size()};
    }

    auto compiler_issues(const source::document_snapshot &document,
                         const codegen::generated_cpp &generated,
                         const std::string &stderr_text,
                         const source::source_provider *provider) -> std::vector<diagnostics::diagnostic>
    {
      const std::regex location(R"(:([0-9]+):([0-9]+): (?:fatal )?error: (.+))");
      std::vector<diagnostics::diagnostic> issues;
      std::istringstream lines(stderr_text);
      std::string line;
      while (issues.size() < 32 && std::getline(lines, line))
      {
        std::smatch match;
        if (!std::regex_search(line, match, location)) continue;
        source::byte_range range{};
        source::document_id source_id = document.identity().id;
        try
        {
          const auto offset = codegen::generated_offset(generated.text,
                                                        std::stoull(match[1].str()),
                                                        std::stoull(match[2].str()));
          if (offset)
            if (const auto *mapped = codegen::source_for_generated_offset(generated, *offset))
            {
              range.begin = static_cast<source::byte_offset>(std::max(0, mapped->source.begin));
              range.end = static_cast<source::byte_offset>(std::max(mapped->source.begin,
                                                                    mapped->source.end));
              std::size_t source_size = document.text().size();
              if (provider && mapped->source_path)
              {
                const auto loaded = provider->read_path(*mapped->source_path);
                if (loaded)
                {
                  source_id = loaded.value->identity().id;
                  source_size = loaded.value->text().size();
                }
              }
              if (range.end > source_size) range = {};
            }
        }
        catch (const std::exception &) { range = {}; }
        auto issue = build_issue(document, match[3].str(), range);
        issue.primary.document = source_id;
        issue.notes.push_back("Generated C++: " + line);
        issues.push_back(std::move(issue));
      }
      if (issues.empty()) issues.push_back(build_issue(document, "Native C++ compilation failed"));
      return issues;
    }

    auto runtime_issue(const source::document_snapshot &document,
                       const std::string &stderr_text,
                       const source::source_provider *provider) -> std::optional<diagnostics::diagnostic>
    {
      constexpr std::string_view marker = "SAGAN_RUNTIME_ERROR\t";
      const auto begin = stderr_text.find(marker);
      if (begin == std::string::npos) return {};
      const auto line_end = stderr_text.find('\n', begin);
      std::string_view line(stderr_text.data() + begin,
                            (line_end == std::string::npos ? stderr_text.size() : line_end) - begin);
      if (line.ends_with('\r')) line.remove_suffix(1);
      const auto first = line.find('\t', marker.size());
      const auto second = first == std::string_view::npos ? first : line.find('\t', first + 1);
      const auto third = second == std::string_view::npos ? second : line.find('\t', second + 1);
      const auto fourth = third == std::string_view::npos ? third : line.find('\t', third + 1);
      if (first == std::string_view::npos || second == std::string_view::npos ||
          third == std::string_view::npos) return {};
      source::byte_range range{};
      source::document_id source_id = document.identity().id;
      const auto path = line.substr(second + 1, third - second - 1);
      std::size_t source_size = document.text().size();
      if (provider && !path.empty())
      {
        const auto loaded = provider->read_path(std::filesystem::path(std::string(path)));
        if (loaded)
        {
          source_id = loaded.value->identity().id;
          source_size = loaded.value->text().size();
        }
      }
      try
      {
        range.begin = static_cast<source::byte_offset>(
            std::stoul(std::string(line.substr(marker.size(), first - marker.size()))));
        range.end = static_cast<source::byte_offset>(
            std::stoul(std::string(line.substr(first + 1, second - first - 1))));
        if (range.begin > range.end || range.end > source_size) range = {};
      }
      catch (const std::exception &) { range = {}; }
      const auto encoded_code = fourth == std::string_view::npos ? std::string_view{} :
                                line.substr(third + 1, fourth - third - 1);
      const auto code = encoded_code.starts_with("SAG-RUN-") && encoded_code.size() == 12
                            ? std::string(encoded_code)
                            : std::string(diagnostics::default_code(diagnostics::phase::runtime));
      diagnostics::diagnostic issue{
          code,
          diagnostics::severity::error, diagnostics::phase::runtime,
          {source_id, range}, std::string(line.substr(fourth == std::string_view::npos ? third + 1 : fourth + 1)),
          {}, {}, {}};
      if (!path.empty() && source_id == document.identity().id &&
          document.identity().canonical_path &&
          document.identity().canonical_path->lexically_normal() !=
              std::filesystem::path(std::string(path)).lexically_normal())
        issue.notes.push_back("Sagan source: " + std::string(path));
      constexpr std::string_view frame_marker = "SAGAN_RUNTIME_FRAME\t";
      std::size_t cursor = line_end == std::string::npos ? stderr_text.size() : line_end + 1;
      for (std::size_t count = 0; count < 64 && cursor < stderr_text.size(); ++count)
      {
        const auto frame_end = stderr_text.find('\n', cursor);
        std::string_view frame(stderr_text.data() + cursor,
            (frame_end == std::string::npos ? stderr_text.size() : frame_end) - cursor);
        if (frame.ends_with('\r')) frame.remove_suffix(1);
        if (!frame.starts_with(frame_marker)) break;
        const auto a = frame.find('\t', frame_marker.size());
        const auto b = a == std::string_view::npos ? a : frame.find('\t', a + 1);
        const auto c = b == std::string_view::npos ? b : frame.find('\t', b + 1);
        if (a == std::string_view::npos || b == std::string_view::npos || c == std::string_view::npos) break;
        const std::string frame_path(frame.substr(frame_marker.size(), a - frame_marker.size()));
        const std::string name(frame.substr(c + 1));
        std::string location = frame_path;
        if (!frame_path.empty())
        {
          std::optional<source::document_snapshot> frame_document;
          if (provider)
          {
            const auto loaded = provider->read_path(frame_path);
            if (loaded) frame_document = std::move(*loaded.value);
          }
          else if (document.identity().canonical_path &&
                   document.identity().canonical_path->lexically_normal() ==
                       std::filesystem::path(frame_path).lexically_normal())
            frame_document = document;
          if (frame_document)
            try
            {
              const auto byte = static_cast<source::byte_offset>(std::stoul(std::string(frame.substr(a + 1, b - a - 1))));
              const auto position = frame_document->to_utf16(byte);
              if (position) location += ':' + std::to_string(position->line + 1);
            }
            catch (const std::exception &) {}
        }
        std::error_code path_error;
        const auto relative = std::filesystem::relative(frame_path, std::filesystem::current_path(), path_error);
        if (!path_error && !relative.empty() && *relative.begin() != "..")
        {
          const auto suffix = location.substr(frame_path.size());
          location = relative.generic_string() + suffix;
        }
        issue.notes.push_back("in " + name + " at " + location);
        cursor = frame_end == std::string::npos ? stderr_text.size() : frame_end + 1;
      }
      return issue;
    }

    auto cancelled(native_operation_result &result, const operation_observer &observer) -> void
    {
      result.state = diagnostics::result_state::cancelled;
      result.exit_status.reset();
      if (result.generated_source && result.generated_source->filename() == "program.cpp")
      {
        const auto directory = result.generated_source->parent_path();
        if (directory.filename().string().starts_with("sagan-") && result.executable &&
            result.executable->parent_path() == directory &&
            (result.executable->filename() == "program.exe" ||
             result.executable->filename() == "program"))
        {
          std::error_code ignored;
          std::filesystem::remove_all(directory, ignored);
          if (!ignored) { result.generated_source.reset(); result.executable.reset(); }
        }
      }
      emit(result, observer, operation_event_kind::finished, 100, "Operation cancelled");
    }

    auto dependencies_current(const native_operation_result &result,
                              const source::source_provider &provider) -> bool
    {
      for (const auto &dependency : result.dependencies)
      {
        const auto latest = provider.read_path(dependency.path);
        if (!latest || latest.value->identity().id != dependency.document.id ||
            latest.value->version() != dependency.version ||
            latest.value->text() != dependency.text) return false;
      }
      return true;
    }

    auto stale(native_operation_result &result, const operation_observer &observer) -> void
    {
      result.state = diagnostics::result_state::stale;
      result.exit_status.reset();
      emit(result, observer, operation_event_kind::finished, 100,
           "A source document changed during the operation");
    }

    auto compile_generated(native_operation_result &result,
                           const source::document_snapshot &document,
                           const std::filesystem::path &artifact_root,
                           const native_build_profile profile,
                           const diagnostics::cancellation_token cancellation,
                           const operation_observer &observer,
                           const source::source_provider *provider = nullptr,
                           const driver::native_compilation_inputs &inputs = {}) -> void
    {
      const auto directory = unique_artifact_directory(artifact_root, document);
      result.generated_source = directory / "program.cpp";
#ifdef _WIN32
      result.executable = directory / "program.exe";
#else
      result.executable = directory / "program";
#endif
      std::ofstream source_file(*result.generated_source, std::ios::binary);
      if (!source_file) throw std::runtime_error("Could not create generated C++ artifact");
      source_file << result.generated->text;
      source_file.close();
      if (!source_file) throw std::runtime_error("Could not write generated C++ artifact");
      emit(result, observer, operation_event_kind::progress, 60, "Compiling native program");
      const auto configuration = driver::configured_compiler();
      auto arguments = flag_words(configuration.flags);
      arguments.push_back(profile == native_build_profile::debug ? "-O0" : "-O2");
      arguments.push_back("-g");
#ifdef _WIN32
      arguments.push_back("-static-libgcc");
      arguments.push_back("-static-libstdc++");
#endif
      if (inputs.header)
      { arguments.push_back("-include"); arguments.push_back(path_utf8(*inputs.header)); }
      arguments.push_back(path_utf8(*result.generated_source));
      if (inputs.source) arguments.push_back(path_utf8(*inputs.source));
      arguments.push_back("-o");
      arguments.push_back(path_utf8(*result.executable));
      arguments.insert(arguments.end(), inputs.libraries.begin(), inputs.libraries.end());
      auto environment = configuration.environment;
      for (const auto *variable : {"TMPDIR", "TMP", "TEMP"})
        environment.emplace_back(variable, path_utf8(directory));
      const auto process = driver::run_process(configuration.executable, arguments, directory,
                                               environment, cancellation,
                                               [&](const bool error, const std::string_view text)
      {
        emit(result, observer,
             error ? operation_event_kind::standard_error : operation_event_kind::standard_output,
             60, std::string(text));
      });
      result.standard_output = process.standard_output;
      result.standard_error = process.standard_error;
      result.output_truncated = process.output_truncated;
      if (process.cancelled) { cancelled(result, observer); return; }
      result.exit_status = process.exit_status;
      result.state = process.exit_status == 0 ? diagnostics::result_state::complete
                                              : diagnostics::result_state::incomplete;
      if (process.exit_status != 0)
      {
        auto issues = compiler_issues(document, *result.generated, process.standard_error, provider);
        for (auto &issue : issues)
        {
          emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
          result.diagnostics.push_back(std::move(issue));
        }
      }
      emit(result, observer, operation_event_kind::finished, 100,
           process.exit_status == 0 ? "Build succeeded" : "Native compilation failed");
    }

    auto execute_generated(native_operation_result &result,
                           const source::document_snapshot &document,
                           const diagnostics::cancellation_token cancellation,
                           const operation_observer &observer,
                           const source::source_provider *provider = nullptr) -> void
    {
      emit(result, observer, operation_event_kind::progress, 0, "Running native program");
      const auto configuration = driver::configured_compiler();
      const auto process = driver::run_process(*result.executable, {},
                                               result.working_directory.value_or(result.executable->parent_path()),
                                               configuration.environment, cancellation,
                                               [&](const bool error, const std::string_view text)
      {
        emit(result, observer,
             error ? operation_event_kind::standard_error : operation_event_kind::standard_output,
             50, std::string(text));
      });
      result.standard_output = process.standard_output;
      result.standard_error = process.standard_error;
      result.output_truncated = result.output_truncated || process.output_truncated;
      if (process.cancelled) { cancelled(result, observer); return; }
      result.exit_status = process.exit_status;
      if (auto issue = runtime_issue(document, process.standard_error, provider))
      {
        result.state = diagnostics::result_state::incomplete;
        emit(result, observer, operation_event_kind::diagnostic, 90, issue->message, *issue);
        result.diagnostics.push_back(std::move(*issue));
      }
      emit(result, observer, operation_event_kind::finished, 100, "Program exited with status " +
           std::to_string(process.exit_status));
    }
  }

  auto map_toolchain_errors(const source::document_snapshot &document,
                            const codegen::generated_cpp &generated,
                            const std::string &compiler_stderr,
                            const source::source_provider *provider) -> std::vector<diagnostics::diagnostic>
  {
    return compiler_issues(document, generated, compiler_stderr, provider);
  }

  auto plan_debug_launch(const native_operation_result &build) -> std::optional<debug_launch_plan>
  {
    if (build.state != diagnostics::result_state::complete || !build.executable ||
        !build.debug || !std::filesystem::is_regular_file(*build.executable)) return {};
    return debug_launch_plan{*build.executable,
                             build.working_directory.value_or(build.executable->parent_path()),
                             driver::configured_compiler().environment, build.profile};
  }

  auto build_document(const source::document_snapshot &document,
                      const std::filesystem::path &artifact_root,
                      const native_build_profile profile,
                      const diagnostics::cancellation_token cancellation,
                      const operation_observer &observer,
                      std::string operation_id,
                      std::optional<std::string> selected_test) -> native_operation_result
  {
    native_operation_result result;
    result.operation_id = operation_id.empty() ? next_operation_id() : std::move(operation_id);
    result.lifecycle = operation_state::running;
    result.profile = profile;
    result.document = document.identity();
    result.analyzed_version = document.version();
    result.entry_source_text = std::string(document.text());
    emit(result, observer, operation_event_kind::started, 0, "Building Sagan document");
    if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
    emit(result, observer, operation_event_kind::progress, 10, "Checking Sagan source");
    const auto checked = check_document(document, {.check_types = true, .check_entry_point = true},
                                        cancellation);
    result.diagnostics = checked.diagnostics;
    if (checked.state == diagnostics::result_state::cancelled)
    { cancelled(result, observer); return result; }
    for (const auto &issue : result.diagnostics)
      emit(result, observer, operation_event_kind::diagnostic, 10, issue.message, issue);
    if (checked.state != diagnostics::result_state::complete)
    {
      result.state = checked.state;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Sagan check failed");
      return result;
    }
    try
    {
      emit(result, observer, operation_event_kind::progress, 30, "Generating C++ and source map");
      const auto parsed = syntax::analyze(document, {.recover = false}, cancellation);
      if (parsed.state == diagnostics::result_state::cancelled)
      { cancelled(result, observer); return result; }
      if (!parsed.value || !parsed.value->strict_ast)
        throw std::runtime_error("Strict syntax tree was unavailable after a successful check");
      const auto model = semantic::analyze(*parsed.value->strict_ast);
      const auto types = semantic::check_types(*parsed.value->strict_ast);
      result.generated = selected_test
          ? codegen::generate_cpp_mapped_test(*parsed.value->strict_ast, types, *selected_test,
                                              document.identity().canonical_path)
          : codegen::generate_cpp_mapped(*parsed.value->strict_ast, types,
                                         document.identity().canonical_path);
      result.debug = derive_debug_metadata(document, model, types, *result.generated);
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      compile_generated(result, document, artifact_root, profile, cancellation, observer);
    }
    catch (const semantic::semantic_error &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      auto issue = semantic_issue(document, error);
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Build failed");
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      auto issue = build_issue(document, error.what());
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Build failed");
    }
    return result;
  }

  auto run_document(const source::document_snapshot &document,
                    const std::filesystem::path &artifact_root,
                    const native_build_profile profile,
                    const diagnostics::cancellation_token cancellation,
                    const operation_observer &observer,
                    std::string operation_id,
                    std::optional<std::string> selected_test) -> native_operation_result
  {
    auto result = build_document(document, artifact_root, profile, cancellation, observer,
                                 std::move(operation_id), std::move(selected_test));
    if (result.state != diagnostics::result_state::complete || !result.executable) return result;
    try
    {
      execute_generated(result, document, cancellation, observer);
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      auto issue = build_issue(document, error.what());
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Run failed");
    }
    return result;
  }

  auto build_project(const std::filesystem::path &entry_or_package,
                     const source::source_provider &source,
                     const std::filesystem::path &artifact_root,
                     const native_build_profile profile,
                     const diagnostics::cancellation_token cancellation,
                     const operation_observer &observer,
                     std::string operation_id,
                     std::optional<project_test_selection> selected_test) -> native_operation_result
  {
    native_operation_result result;
    result.operation_id = operation_id.empty() ? next_operation_id() : std::move(operation_id);
    result.lifecycle = operation_state::running;
    result.profile = profile;
    result.document = source::identity_from_path(source::document_id{}, entry_or_package);
    emit(result, observer, operation_event_kind::started, 0, "Building Sagan project");
    if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
    std::optional<source::document_snapshot> entry;
    try
    {
      const bool package_path = std::filesystem::is_directory(entry_or_package) ||
                                entry_or_package.filename() == "sagan.toml";
      emit(result, observer, operation_event_kind::progress, 10, "Resolving modules and overlays");
      const auto graph = package_path ? modules::resolve_package(entry_or_package, source, cancellation)
                                      : modules::resolve(entry_or_package, source, cancellation);
      auto loaded = source.read_path(graph.entry_path);
      if (!loaded) throw std::runtime_error(loaded.error->message);
      entry = std::move(*loaded.value);
      result.document = entry->identity();
      result.analyzed_version = entry->version();
      result.entry_source_text = std::string(entry->text());
      for (const auto &module : graph.modules)
      {
        const auto loaded_module = source.read_path(module.path);
        if (!loaded_module) throw std::runtime_error(loaded_module.error->message);
        result.dependencies.push_back({module.path, loaded_module.value->identity(),
                                       loaded_module.value->version(),
                                       std::string(loaded_module.value->text())});
      }
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      const auto tree = package_path ? modules::link_package(entry_or_package, source, cancellation)
                                     : modules::link(entry_or_package, source, cancellation);
      const auto model = semantic::analyze(tree);
      const auto types = semantic::check_types(tree);
      semantic::validate_entry_point(tree);
      emit(result, observer, operation_event_kind::progress, 30, "Generating linked C++ and source map");
      if (selected_test)
      {
        std::optional<std::string> linked_name;
        const auto selected_path = std::filesystem::absolute(selected_test->source_path).lexically_normal();
        for (const auto &statement : tree.statements)
        {
          const auto *function = dynamic_cast<const parser::function_declaration *>(statement.get());
          if (!function || !function->test_name || *function->test_name != selected_test->name ||
              !statement->origin_path) continue;
          if (std::filesystem::absolute(*statement->origin_path).lexically_normal() == selected_path)
          { linked_name = function->name; break; }
        }
        if (!linked_name) throw std::runtime_error("Selected test is not present in the linked project");
        result.generated = codegen::generate_cpp_mapped_test(tree, types, *linked_name,
                                                              entry->identity().canonical_path);
      }
      else result.generated = codegen::generate_cpp_mapped(tree, types, entry->identity().canonical_path);
      result.debug = derive_debug_metadata(*entry, model, types, *result.generated, &source);
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      if (!dependencies_current(result, source)) { stale(result, observer); return result; }
      const auto inputs = driver::compilation_inputs_for(graph);
      result.working_directory = inputs.working_directory;
      compile_generated(result, *entry, artifact_root, profile, cancellation, observer, &source, inputs);
      if (result.state == diagnostics::result_state::complete &&
          !dependencies_current(result, source)) stale(result, observer);
    }
    catch (const semantic::semantic_error &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      diagnostics::diagnostic issue = entry
                                          ? semantic_issue(*entry, error, &source)
                                          : diagnostics::diagnostic{
                                                std::string(diagnostics::default_code(
                                                    diagnostics::phase::project)),
                                                diagnostics::severity::error,
                                                diagnostics::phase::project,
                                                {result.document.id, {}}, error.what(), {}, {}, {}};
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Project build failed");
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      diagnostics::diagnostic issue;
      if (entry) issue = build_issue(*entry, error.what());
      else
        issue = {std::string(diagnostics::default_code(diagnostics::phase::project)),
                 diagnostics::severity::error, diagnostics::phase::project,
                 {result.document.id, {}}, error.what(), {}, {}, {}};
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Project build failed");
    }
    return result;
  }

  auto run_project(const std::filesystem::path &entry_or_package,
                   const source::source_provider &source,
                   const std::filesystem::path &artifact_root,
                   const native_build_profile profile,
                   const diagnostics::cancellation_token cancellation,
                   const operation_observer &observer,
                   std::string operation_id,
                   std::optional<project_test_selection> selected_test) -> native_operation_result
  {
    auto result = build_project(entry_or_package, source, artifact_root, profile, cancellation, observer,
                                std::move(operation_id), std::move(selected_test));
    if (result.state != diagnostics::result_state::complete || !result.executable) return result;
    const auto loaded = source.read_path(result.document.canonical_path.value_or(entry_or_package));
    if (!loaded)
    {
      result.state = diagnostics::result_state::stale;
      result.exit_status.reset();
      emit(result, observer, operation_event_kind::finished, 100, "Entry source changed or disappeared");
      return result;
    }
    if (!dependencies_current(result, source) ||
        loaded.value->version() != result.analyzed_version ||
        loaded.value->text() != result.entry_source_text)
    {
      stale(result, observer);
      return result;
    }
    try { execute_generated(result, *loaded.value, cancellation, observer, &source); }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) { cancelled(result, observer); return result; }
      auto issue = build_issue(*loaded.value, error.what());
      emit(result, observer, operation_event_kind::diagnostic, 90, issue.message, issue);
      result.diagnostics.push_back(std::move(issue));
      result.state = diagnostics::result_state::incomplete;
      result.exit_status = 1;
      emit(result, observer, operation_event_kind::finished, 100, "Project run failed");
    }
    return result;
  }
}
