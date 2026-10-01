#include "server.hpp"
#include "conversions.hpp"

#include "../language_service/operations.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace sagan::lsp
{
  namespace
  {
    using language_service::operation_event;
    using language_service::operation_event_kind;
    using language_service::operation_state;

    auto state_text(const operation_state state) -> std::string
    { return std::string(language_service::operation_state_name(state)); }

    auto result_state_text(const diagnostics::result_state state) -> std::string
    {
      switch (state)
      {
        case diagnostics::result_state::complete: return "complete";
        case diagnostics::result_state::recovered: return "recovered";
        case diagnostics::result_state::incomplete: return "incomplete";
        case diagnostics::result_state::cancelled: return "cancelled";
        case diagnostics::result_state::stale: return "stale";
      }
      return "incomplete";
    }

    auto event_kind_text(const operation_event_kind kind) -> std::string
    {
      switch (kind)
      {
        case operation_event_kind::started: return "started";
        case operation_event_kind::progress: return "progress";
        case operation_event_kind::standard_output: return "stdout";
        case operation_event_kind::standard_error: return "stderr";
        case operation_event_kind::diagnostic: return "diagnostic";
        case operation_event_kind::finished: return "finished";
      }
      return "progress";
    }

    auto diagnostics_json(const source::document_snapshot &document,
                          const std::vector<diagnostics::diagnostic> &issues) -> J
    {
      J::array result;
      const auto known_range = [&](const source::source_range range) -> J
      {
        if (range.document != document.identity().id || range.bytes.end > document.text().size())
          return nullptr;
        return lsp_range(document, range.bytes);
      };
      for (const auto &issue : issues)
      {
        J::object entry{{"code", issue.code}, {"phase", std::string(diagnostics::phase_name(issue.owner))},
                        {"severity", std::string(diagnostics::severity_name(issue.level))},
                        {"message", issue.message},
                        {"documentId", std::to_string(issue.primary.document.value)}};
        if (const auto range = known_range(issue.primary);
            !std::holds_alternative<std::nullptr_t>(range.data))
        {
          entry["uri"] = document.identity().uri.value;
          entry["range"] = range;
        }
        J::array notes;
        for (const auto &note : issue.notes) notes.push_back(note);
        entry["notes"] = std::move(notes);
        J::array related;
        for (const auto &place : issue.related)
          related.push_back(J::object{{"documentId", std::to_string(place.range.document.value)},
                                       {"range", known_range(place.range)}, {"message", place.message}});
        entry["related"] = std::move(related);
        J::array fixes;
        for (const auto &fix : issue.fixes)
        {
          J::array edits;
          for (const auto &edit : fix.edits)
            edits.push_back(J::object{{"documentId", std::to_string(edit.range.document.value)},
                                       {"range", known_range(edit.range)},
                                       {"replacement", edit.replacement_utf8}});
          fixes.push_back(J::object{{"title", fix.title}, {"edits", std::move(edits)}});
        }
        entry["fixes"] = std::move(fixes);
        result.push_back(std::move(entry));
      }
      return result;
    }
  }

  auto server::query_operation(const J &params,
                               const diagnostics::cancellation_token cancellation) -> J
  {
    const auto kind = string_field(params, "kind");
    const auto scope = string_field(params, "scope");
    if (kind != "check" && kind != "build" && kind != "run")
      throw std::invalid_argument("Operation kind must be check, build, or run");
    if (scope != "document" && scope != "project")
      throw std::invalid_argument("Operation scope must be document or project");
    const auto uri = source::document_uri{string_field(field(params, "textDocument"), "uri")};
    if (uri.value.empty()) throw std::invalid_argument("Operation requires textDocument.uri");
    const auto loaded = documents_->read(uri);
    if (!loaded) throw std::invalid_argument(loaded.error->message);
    const auto document = *loaded.value;
    if (const auto *version = field(params, "textDocument").get("version"))
      if (!version->integer() || *version->integer() != document.version())
        throw std::invalid_argument("Operation document version is stale");
    const auto profile_name = string_field(params, "profile");
    if (kind != "check" && !profile_name.empty() &&
        profile_name != "debug" && profile_name != "optimized")
      throw std::invalid_argument("Operation profile must be debug or optimized");
    std::filesystem::path path;
    if (scope == "project")
    {
      const auto project_uri = source::document_uri{string_field(params, "projectUri")};
      const auto canonical = documents_->canonicalize(project_uri.value.empty() ? uri : project_uri);
      if (!canonical) throw std::invalid_argument(canonical.error->message);
      path = *canonical.value;
    }
    const auto operation_id = language_service::next_operation_id();
    const auto *progress_token = params.get("workDoneToken");
    if (progress_token)
      notify(J::object{{"jsonrpc", "2.0"}, {"method", "$/progress"},
                       {"params", J::object{{"token", *progress_token},
                                             {"value", J::object{{"kind", "begin"},
                                                                 {"title", "Sagan " + kind}}}}}});
    const auto observer = [&](const operation_event &event)
    {
      // A newer overlay can arrive before this request finishes. Diagnostic events
      // are deliberately published only with the final, version-checked result.
      if (event.kind != operation_event_kind::diagnostic &&
          event.kind != operation_event_kind::finished)
        notify(J::object{{"jsonrpc", "2.0"}, {"method", "sagan/operationEvent"},
                         {"params", J::object{{"schema", std::string(language_service::operations_schema_version)},
                                               {"id", event.operation_id},
                                               {"state", state_text(event.state)},
                                               {"kind", event_kind_text(event.kind)},
                                               {"sequence", static_cast<std::int64_t>(event.sequence)},
                                               {"percent", event.percent}, {"text", event.text}}}});
      if (progress_token && event.kind == operation_event_kind::progress)
        notify(J::object{{"jsonrpc", "2.0"}, {"method", "$/progress"},
                         {"params", J::object{{"token", *progress_token},
                                               {"value", J::object{{"kind", "report"},
                                                                   {"percentage", event.percent},
                                                                   {"message", event.text}}}}}});
    };
    J::object answer{{"schema", std::string(language_service::operations_schema_version)},
                     {"id", operation_id}, {"kind", kind}, {"scope", scope},
                     {"uri", uri.value}, {"version", document.version()}};
    const auto is_stale = [&]
    {
      const auto current = documents_->read(uri);
      return !current || current.value->version() != document.version() ||
             current.value->text() != document.text();
    };
    if (kind == "check")
    {
      auto checked = scope == "document"
          ? language_service::run_check_operation(document, {}, cancellation, observer, operation_id)
          : language_service::run_check_project(path, *documents_, cancellation, observer, operation_id);
      if (is_stale())
      { checked.lifecycle = operation_state::stale; checked.state = diagnostics::result_state::stale; }
      answer["state"] = state_text(checked.lifecycle);
      answer["resultState"] = result_state_text(checked.state);
      answer["exitStatus"] = checked.lifecycle == operation_state::stale ||
                             checked.lifecycle == operation_state::cancelled || !checked.exit_status
                             ? J{nullptr} : J{*checked.exit_status};
      answer["diagnostics"] = checked.lifecycle == operation_state::stale ||
                               checked.lifecycle == operation_state::cancelled
                               ? J::array{} : diagnostics_json(document, checked.diagnostics);
    }
    else
    {
      const auto artifact_root = std::filesystem::temp_directory_path() / "sagan-lsp-operations";
      const auto profile = profile_name == "optimized" ? language_service::native_build_profile::optimized
                                                         : language_service::native_build_profile::debug;
      auto native = scope == "document"
          ? (kind == "build" ? language_service::build_document(document, artifact_root, profile,
                                                                cancellation, observer, operation_id)
                             : language_service::run_document(document, artifact_root, profile,
                                                              cancellation, observer, operation_id))
          : (kind == "build" ? language_service::build_project(path, *documents_, artifact_root, profile,
                                                               cancellation, observer, operation_id)
                             : language_service::run_project(path, *documents_, artifact_root, profile,
                                                             cancellation, observer, operation_id));
      bool dependency_stale = false;
      for (const auto &dependency : native.dependencies)
      {
        const auto current = documents_->read_path(dependency.path);
        if (!current || current.value->identity().id != dependency.document.id ||
            current.value->version() != dependency.version ||
            current.value->text() != dependency.text)
        { dependency_stale = true; break; }
      }
      if (is_stale() || dependency_stale)
      { native.lifecycle = operation_state::stale; native.state = diagnostics::result_state::stale; }
      answer["state"] = state_text(native.lifecycle);
      answer["resultState"] = result_state_text(native.state);
      answer["exitStatus"] = native.lifecycle == operation_state::stale ||
                             native.lifecycle == operation_state::cancelled || !native.exit_status
                             ? J{nullptr} : J{*native.exit_status};
      answer["stdout"] = native.standard_output;
      answer["stderr"] = native.standard_error;
      answer["outputTruncated"] = native.output_truncated;
      answer["executable"] = native.lifecycle == operation_state::stale ||
                             native.lifecycle == operation_state::cancelled || !native.executable
                             ? J{nullptr} : J{native.executable->string()};
      answer["diagnostics"] = native.lifecycle == operation_state::stale ||
                               native.lifecycle == operation_state::cancelled
                               ? J::array{} : diagnostics_json(document, native.diagnostics);
    }
    notify(J::object{{"jsonrpc", "2.0"}, {"method", "sagan/operationEvent"},
                     {"params", J::object{{"schema", std::string(language_service::operations_schema_version)},
                                           {"id", operation_id}, {"state", answer["state"]},
                                           {"kind", "finished"}, {"percent", 100},
                                           {"text", answer["state"]}}}});
    if (progress_token)
      notify(J::object{{"jsonrpc", "2.0"}, {"method", "$/progress"},
                       {"params", J::object{{"token", *progress_token},
                                             {"value", J::object{{"kind", "end"},
                                                                 {"message", answer["state"]}}}}}});
    return answer;
  }
}
