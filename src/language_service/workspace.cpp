#include "workspace.hpp"

#include <queue>
#include <utility>

namespace sagan::language_service
{
  namespace
  {
    auto unavailable() -> diagnostics::analysis_result<check_summary>
    {
      return {diagnostics::result_state::incomplete, {}, {}, 0};
    }
  }

  workspace::workspace(std::shared_ptr<source::document_store> documents) : documents_(std::move(documents)) {}

  auto workspace::documents() const -> const source::document_store & { return *documents_; }

  auto workspace::invalidate_locked(const std::string &uri) -> std::size_t
  {
    std::queue<std::string> pending;
    std::unordered_set<std::string> visited;
    pending.push(uri);
    while (!pending.empty())
    {
      auto current = std::move(pending.front());
      pending.pop();
      if (!visited.insert(current).second) continue;
      ++generations_[current];
      cache_.erase(current);
      if (const auto active = active_requests_.find(current); active != active_requests_.end())
      {
        active->second.cancellation.cancel();
        active_requests_.erase(active);
      }
      if (const auto next = dependents_.find(current); next != dependents_.end())
        for (const auto &dependent : next->second) pending.push(dependent);
    }
    return visited.size();
  }

  auto workspace::open(source::document_uri uri, const source::document_version version, std::string text)
    -> source::provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto result = documents_->open(uri, version, std::move(text));
    if (result) invalidate_locked(uri.value);
    return result;
  }

  auto workspace::change(const source::document_uri &uri, const source::document_version expected_version,
                         const source::document_version new_version, std::vector<source::text_edit> edits)
    -> source::provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto result = documents_->change(uri, expected_version, new_version, std::move(edits));
    if (result) invalidate_locked(uri.value);
    return result;
  }

  auto workspace::replace(const source::document_uri &uri, const source::document_version expected_version,
                          const source::document_version new_version, std::string text)
    -> source::provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto result = documents_->replace(uri, expected_version, new_version, std::move(text));
    if (result) invalidate_locked(uri.value);
    return result;
  }

  auto workspace::save(const source::document_uri &uri, const source::document_version version,
                       std::optional<std::string> text) -> source::provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto result = documents_->save(uri, version, std::move(text));
    if (result) invalidate_locked(uri.value);
    return result;
  }

  auto workspace::close(const source::document_uri &uri) -> source::provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto result = documents_->close(uri);
    if (!result) return result;
    invalidate_locked(uri.value);
    if (const auto old = dependencies_.find(uri.value); old != dependencies_.end())
    {
      for (const auto &dependency : old->second) dependents_[dependency].erase(uri.value);
      dependencies_.erase(old);
    }
    if (const auto reverse = dependents_.find(uri.value); reverse != dependents_.end())
    {
      for (const auto &dependent : reverse->second) dependencies_[dependent].erase(uri.value);
      dependents_.erase(reverse);
    }
    return result;
  }

  auto workspace::set_dependencies(const source::document_uri &document,
                                   const std::vector<source::document_uri> &dependencies) -> void
  {
    std::scoped_lock lock(mutex_);
    for (const auto &old : dependencies_[document.value]) dependents_[old].erase(document.value);
    auto &current = dependencies_[document.value];
    current.clear();
    for (const auto &dependency : dependencies)
    {
      if (dependency == document) continue;
      current.insert(dependency.value);
      dependents_[dependency.value].insert(document.value);
    }
    invalidate_locked(document.value);
  }

  auto workspace::invalidate(const source::document_uri &uri) -> std::size_t
  {
    std::scoped_lock lock(mutex_);
    return invalidate_locked(uri.value);
  }

  auto workspace::begin_analysis(const source::document_uri &uri) -> source::provider_result<analysis_request>
  {
    std::scoped_lock lock(mutex_);
    auto document = documents_->read(uri);
    if (!document) return {{}, document.error};
    if (const auto active = active_requests_.find(uri.value); active != active_requests_.end())
      active->second.cancellation.cancel();
    diagnostics::cancellation_source cancellation;
    const auto request_id = next_request_id_++;
    active_requests_.insert_or_assign(uri.value, active_analysis{request_id, cancellation});
    return {analysis_request{std::move(*document.value), generations_[uri.value], request_id, cancellation.token()}, {}};
  }

  auto workspace::finish_analysis(const analysis_request &request,
                                  diagnostics::analysis_result<check_summary> result)
    -> diagnostics::analysis_result<check_summary>
  {
    std::scoped_lock lock(mutex_);
    const auto &uri = request.document.identity().uri.value;
    const auto current_version = documents_->current_version(request.document.identity().uri);
    const auto active = active_requests_.find(uri);
    if (!current_version || *current_version != request.document.version() || generations_[uri] != request.generation ||
        active == active_requests_.end() || active->second.request_id != request.request_id)
    {
      result.state = diagnostics::result_state::stale;
      result.value.reset();
      result.diagnostics.clear();
      return result;
    }
    active_requests_.erase(uri);
    if (result.state != diagnostics::result_state::cancelled)
      cache_.insert_or_assign(uri, cached_analysis{request.document.version(), request.generation, result});
    return result;
  }

  auto workspace::analyze(const source::document_uri &uri, const check_options options)
    -> diagnostics::analysis_result<check_summary>
  {
    {
      std::scoped_lock lock(mutex_);
      const auto version = documents_->current_version(uri);
      const auto cached = cache_.find(uri.value);
      if (version && cached != cache_.end() && cached->second.version == *version &&
          cached->second.generation == generations_[uri.value]) return cached->second.result;
    }
    auto request = begin_analysis(uri);
    if (!request) return unavailable();
    auto result = analyze_document(request.value->document, options, request.value->cancellation);
    return finish_analysis(*request.value, std::move(result));
  }
}
