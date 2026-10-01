#include "operations.hpp"
#include "../modules/resolver.hpp"

#include <atomic>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace sagan::language_service
{
  namespace
  {
    std::atomic<std::uint64_t> operation_sequence{1};

    auto lifecycle_for(const diagnostics::result_state state) -> operation_state
    {
      switch (state)
      {
        case diagnostics::result_state::complete: return operation_state::completed;
        case diagnostics::result_state::cancelled: return operation_state::cancelled;
        case diagnostics::result_state::stale: return operation_state::stale;
        case diagnostics::result_state::recovered:
        case diagnostics::result_state::incomplete: return operation_state::failed;
      }
      return operation_state::failed;
    }

    template <typename Result, typename Work>
    auto start(std::string id, const operation_observer &observer, Work work)
      -> asynchronous_operation<Result>
    {
      asynchronous_operation<Result> task;
      task.id = std::move(id);
      task.state = std::make_shared<std::atomic<operation_state>>(operation_state::queued);
      const auto task_id = task.id;
      const auto state = task.state;
      const auto token = task.cancellation.token();
      if (observer) observer({operation_event_kind::started, 0, 0, "Queued", {}, task_id,
                              operation_state::queued});
      task.future = std::async(std::launch::async, [work = std::move(work), observer, task_id,
                                                    state, token]() mutable -> Result
      {
        state->store(operation_state::running);
        auto result = work(token, observer, task_id);
        state->store(result.lifecycle);
        return result;
      });
      return task;
    }
  }

  auto next_operation_id() -> std::string
  {
    return "sagan-operation-" + std::to_string(operation_sequence.fetch_add(1));
  }

  auto operation_state_name(const operation_state state) -> std::string_view
  {
    switch (state)
    {
      case operation_state::queued: return "queued";
      case operation_state::running: return "running";
      case operation_state::completed: return "completed";
      case operation_state::failed: return "failed";
      case operation_state::cancelled: return "cancelled";
      case operation_state::stale: return "stale";
    }
    return "failed";
  }

  auto run_check_operation(const source::document_snapshot &document, const check_options options,
                           const diagnostics::cancellation_token cancellation,
                           const operation_observer &observer,
                           std::string operation_id) -> check_operation_result
  {
    check_operation_result result;
    result.operation_id = operation_id.empty() ? next_operation_id() : std::move(operation_id);
    result.lifecycle = operation_state::running;
    result.document = document.identity();
    result.analyzed_version = document.version();
    const auto emit = [&](const operation_event_kind kind, const int percent,
                          std::string text = {},
                          std::optional<diagnostics::diagnostic> issue = {})
    {
      result.events.push_back({kind, result.events.size(), percent, std::move(text), std::move(issue),
                               result.operation_id, result.lifecycle});
      if (observer) observer(result.events.back());
    };
    emit(operation_event_kind::started, 0, "Checking document");
    if (cancellation.is_cancelled())
    {
      result.state = diagnostics::result_state::cancelled;
      result.lifecycle = operation_state::cancelled;
      emit(operation_event_kind::finished, 100, "Check cancelled");
      return result;
    }
    emit(operation_event_kind::progress, 10, "Analyzing source");
    auto analysis = check_document(document, options, cancellation);
    result.state = analysis.state;
    result.lifecycle = lifecycle_for(result.state);
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

  auto start_check_document(source::document_snapshot document, const check_options options,
                            operation_observer observer) -> asynchronous_operation<check_operation_result>
  {
    return start<check_operation_result>(next_operation_id(), observer,
        [document = std::move(document), options](const auto token, const auto &events,
                                                   const std::string &id)
        { return run_check_operation(document, options, token, events, id); });
  }

  auto run_check_project(const std::filesystem::path &entry_or_package,
                         const source::source_provider &source,
                         const diagnostics::cancellation_token cancellation,
                         const operation_observer &observer,
                         std::string operation_id) -> check_operation_result
  {
    check_operation_result result;
    result.operation_id = operation_id.empty() ? next_operation_id() : std::move(operation_id);
    result.lifecycle = operation_state::running;
    result.document = source::identity_from_path(source::document_id{}, entry_or_package);
    const auto emit = [&](const operation_event_kind kind, const int percent, std::string text)
    {
      result.events.push_back({kind, result.events.size(), percent, std::move(text), {},
                               result.operation_id, result.lifecycle});
      if (observer) observer(result.events.back());
    };
    emit(operation_event_kind::started, 0, "Checking Sagan project");
    if (cancellation.is_cancelled())
    {
      result.state = diagnostics::result_state::cancelled;
      result.lifecycle = operation_state::cancelled;
      emit(operation_event_kind::finished, 100, "Check cancelled");
      return result;
    }
    try
    {
      emit(operation_event_kind::progress, 10, "Resolving project modules");
      const bool package_path = std::filesystem::is_directory(entry_or_package) ||
                                entry_or_package.filename() == "sagan.toml";
      const auto graph = package_path ? modules::resolve_package(entry_or_package, source, cancellation)
                                      : modules::resolve(entry_or_package, source, cancellation);
      std::vector<std::tuple<std::filesystem::path, source::document_version, std::string>> dependencies;
      for (const auto &module : graph.modules)
      {
        if (cancellation.is_cancelled()) throw std::runtime_error("Project check cancelled");
        const auto snapshot = source.read_path(module.path);
        if (!snapshot) throw std::runtime_error(snapshot.error->message);
        dependencies.emplace_back(module.path, snapshot.value->version(), snapshot.value->text());
      }
      if (cancellation.is_cancelled())
      {
        result.state = diagnostics::result_state::cancelled;
        result.lifecycle = operation_state::cancelled;
        emit(operation_event_kind::finished, 100, "Check cancelled");
        return result;
      }
      const auto entry = source.read_path(graph.entry_path);
      if (!entry) throw std::runtime_error(entry.error->message);
      result.document = entry.value->identity();
      result.analyzed_version = entry.value->version();
      emit(operation_event_kind::progress, 30, "Analyzing project source");
      auto analyzed = analyze_project_document(*entry.value, source, cancellation);
      result.state = analyzed.state;
      result.summary = std::move(analyzed.value);
      result.diagnostics = std::move(analyzed.diagnostics);
      if (cancellation.is_cancelled()) result.state = diagnostics::result_state::cancelled;
      if (result.state != diagnostics::result_state::cancelled)
      {
        for (const auto &[path, version, text] : dependencies)
        {
          const auto current = source.read_path(path);
          if (!current || current.value->version() != version || current.value->text() != text)
          { result.state = diagnostics::result_state::stale; result.diagnostics.clear(); break; }
        }
      }
    }
    catch (const std::exception &failure)
    {
      result.state = cancellation.is_cancelled() ? diagnostics::result_state::cancelled
                                                 : diagnostics::result_state::incomplete;
      if (result.state != diagnostics::result_state::cancelled)
        result.diagnostics.push_back({std::string(diagnostics::default_code(diagnostics::phase::project)),
                                      diagnostics::severity::error, diagnostics::phase::project,
                                      {result.document.id, {}}, failure.what(), {}, {}, {}});
    }
    result.lifecycle = lifecycle_for(result.state);
    if (result.lifecycle != operation_state::cancelled && result.lifecycle != operation_state::stale)
      for (const auto &issue : result.diagnostics)
      {
        result.events.push_back({operation_event_kind::diagnostic, result.events.size(), 90,
                                 issue.message, issue, result.operation_id, result.lifecycle});
        if (observer) observer(result.events.back());
      }
    if (result.lifecycle != operation_state::cancelled && result.lifecycle != operation_state::stale)
      result.exit_status = result.lifecycle == operation_state::completed ? 0 : 1;
    emit(operation_event_kind::finished, 100, std::string(operation_state_name(result.lifecycle)));
    return result;
  }

  auto start_check_project(std::filesystem::path entry_or_package,
                           std::shared_ptr<const source::source_provider> source,
                           operation_observer observer) -> asynchronous_operation<check_operation_result>
  {
    if (!source) throw std::invalid_argument("Project source provider is required");
    return start<check_operation_result>(next_operation_id(), observer,
        [entry_or_package = std::move(entry_or_package), source = std::move(source)]
        (const auto token, const auto &events, const std::string &id)
        { return run_check_project(entry_or_package, *source, token, events, id); });
  }

  auto start_build_document(source::document_snapshot document, std::filesystem::path artifact_root,
                            const native_build_profile profile, operation_observer observer)
    -> asynchronous_operation<native_operation_result>
  {
    return start<native_operation_result>(next_operation_id(), observer,
        [document = std::move(document), artifact_root = std::move(artifact_root), profile]
        (const auto token, const auto &events, const std::string &id)
        { return build_document(document, artifact_root, profile, token, events, id); });
  }

  auto start_run_document(source::document_snapshot document, std::filesystem::path artifact_root,
                          const native_build_profile profile, operation_observer observer)
    -> asynchronous_operation<native_operation_result>
  {
    return start<native_operation_result>(next_operation_id(), observer,
        [document = std::move(document), artifact_root = std::move(artifact_root), profile]
        (const auto token, const auto &events, const std::string &id)
        { return run_document(document, artifact_root, profile, token, events, id); });
  }

  auto start_build_project(std::filesystem::path entry_or_package,
                           std::shared_ptr<const source::source_provider> source,
                           std::filesystem::path artifact_root, const native_build_profile profile,
                           operation_observer observer) -> asynchronous_operation<native_operation_result>
  {
    if (!source) throw std::invalid_argument("Project source provider is required");
    return start<native_operation_result>(next_operation_id(), observer,
        [entry_or_package = std::move(entry_or_package), source = std::move(source),
         artifact_root = std::move(artifact_root), profile]
        (const auto token, const auto &events, const std::string &id)
        { return build_project(entry_or_package, *source, artifact_root, profile, token, events, id); });
  }

  auto start_run_project(std::filesystem::path entry_or_package,
                         std::shared_ptr<const source::source_provider> source,
                         std::filesystem::path artifact_root, const native_build_profile profile,
                         operation_observer observer) -> asynchronous_operation<native_operation_result>
  {
    if (!source) throw std::invalid_argument("Project source provider is required");
    return start<native_operation_result>(next_operation_id(), observer,
        [entry_or_package = std::move(entry_or_package), source = std::move(source),
         artifact_root = std::move(artifact_root), profile]
        (const auto token, const auto &events, const std::string &id)
        { return run_project(entry_or_package, *source, artifact_root, profile, token, events, id); });
  }
}
