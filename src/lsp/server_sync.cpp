#include "server.hpp"
#include "conversions.hpp"

#include "../language_service/language_service.hpp"

#include <algorithm>
#include <stdexcept>

namespace sagan::lsp
{
  namespace
  {
    auto lsp_diagnostic(const source::document_snapshot &document,
                        const diagnostics::diagnostic &issue) -> J
    {
      const int severity = issue.level == diagnostics::severity::error ? 1 :
                           issue.level == diagnostics::severity::warning ? 2 :
                           issue.level == diagnostics::severity::information ? 3 : 4;
      J::object converted{{"range", lsp_range(document, issue.primary.bytes)},
                          {"severity", severity}, {"code", issue.code},
                          {"source", "sagan"}, {"message", issue.message}};
      if (!issue.related.empty())
      {
        J::array related;
        for (const auto &location : issue.related)
          if (location.range.document == document.identity().id)
            related.push_back(J::object{{"location", J::object{{"uri", document.identity().uri.value},
                                                                 {"range", lsp_range(document, location.range.bytes)}}},
                                        {"message", location.message}});
        if (!related.empty()) converted["relatedInformation"] = std::move(related);
      }
      return converted;
    }
  }

  auto server::publish(const source::document_uri &uri) -> J
  {
    const auto loaded = documents_->read(uri);
    if (!loaded) throw std::invalid_argument(loaded.error->message);
    const auto &document = *loaded.value;
    const auto analysis = language_service::analyze_project_document(document, *documents_);
    J::array issues;
    if (documents_->current_version(uri) == document.version() &&
        analysis.state != diagnostics::result_state::cancelled &&
        analysis.state != diagnostics::result_state::stale)
      for (const auto &issue : analysis.diagnostics)
        if (issue.primary.document == document.identity().id)
          issues.push_back(lsp_diagnostic(document, issue));
    return J::object{{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"},
                     {"params", J::object{{"uri", uri.value}, {"version", document.version()},
                                          {"diagnostics", std::move(issues)}}}};
  }

  auto server::synchronize(const std::string_view method, const J &params) -> std::vector<J>
  {
    const auto &text_document = field(params, "textDocument");
    const source::document_uri uri{string_field(text_document, "uri")};
    if (uri.value.empty()) throw std::invalid_argument("Missing document URI");
    if (method == "textDocument/didOpen")
    {
      const auto opened = documents_->open(uri, integer_field(text_document, "version"),
                                           string_field(text_document, "text"));
      if (!opened) throw std::invalid_argument(opened.error->message);
      open_documents_.push_back(uri);
      std::vector<J> updates;
      for (const auto &open : open_documents_) updates.push_back(publish(open));
      return updates;
    }
    if (method == "textDocument/didChange")
    {
      const auto loaded = documents_->read(uri);
      if (!loaded || !documents_->is_open(uri)) throw std::invalid_argument("Document is not open");
      const auto next_version = integer_field(text_document, "version");
      const auto changes = field(params, "contentChanges").elements();
      if (!changes || changes->empty()) throw std::invalid_argument("Missing document changes");
      std::string updated(loaded.value->text());
      for (const auto &change : *changes)
      {
        if (const auto *range = change.get("range"))
        {
          const source::document_snapshot intermediate(loaded.value->identity(),
                                                       loaded.value->version(), updated);
          const auto bytes = byte_range(intermediate, *range);
          updated.replace(bytes.begin, bytes.end - bytes.begin, string_field(change, "text"));
        }
        else updated = string_field(change, "text");
      }
      const auto replaced = documents_->replace(uri, loaded.value->version(), next_version, updated);
      if (!replaced) throw std::invalid_argument(replaced.error->message);
      std::vector<J> updates;
      for (const auto &open : open_documents_) updates.push_back(publish(open));
      return updates;
    }
    if (method == "textDocument/didSave")
    {
      const auto current = documents_->current_version(uri);
      if (!current) throw std::invalid_argument("Document is not open");
      const auto *text = params.get("text");
      const auto saved = documents_->save(uri, *current,
                                           text ? std::optional<std::string>(string_field(params, "text"))
                                                : std::optional<std::string>{});
      if (!saved) throw std::invalid_argument(saved.error->message);
      std::vector<J> updates;
      for (const auto &open : open_documents_) updates.push_back(publish(open));
      return updates;
    }
    if (method == "textDocument/didClose")
    {
      const auto closed = documents_->close(uri);
      if (!closed) throw std::invalid_argument(closed.error->message);
      std::erase_if(open_documents_, [&](const auto &open) { return open == uri; });
      std::vector<J> updates{J::object{{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"},
                                       {"params", J::object{{"uri", uri.value}, {"diagnostics", J::array{}}}}}};
      for (const auto &open : open_documents_) updates.push_back(publish(open));
      return updates;
    }
    return {};
  }
}
