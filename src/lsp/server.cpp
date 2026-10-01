#include "server.hpp"

#include "../language_service/language_service.hpp"
#include "../version.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace sagan::lsp
{
  namespace
  {
    using J = json::value;
    auto field(const J &item, const std::string_view key) -> const J &
    {
      static const J empty;
      if (const auto *found = item.get(key)) return *found;
      return empty;
    }
    auto string_field(const J &item, const std::string_view key) -> std::string
    {
      return std::string(field(item, key).string().value_or(""));
    }
    auto response(const J &id, J result) -> J
    {
      return J::object{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
    }
    auto error(const J &id, const int code, std::string message) -> J
    {
      return J::object{{"jsonrpc", "2.0"}, {"id", id},
                       {"error", J::object{{"code", code}, {"message", std::move(message)}}}};
    }
    auto capabilities() -> J
    {
      const J::array token_types{"namespace", "type", "class", "enum", "interface", "typeParameter",
                                 "parameter", "variable", "property", "enumMember", "function", "method",
                                 "keyword"};
      const J::array modifiers{"declaration", "readonly", "static", "deprecated", "defaultLibrary"};
      return J::object{
          {"positionEncoding", "utf-16"},
          {"workspace", J::object{{"workspaceFolders", J::object{{"supported", true},
                                                                 {"changeNotifications", true}}}}},
          {"textDocumentSync", J::object{{"openClose", true}, {"change", 2}, {"save", J::object{{"includeText", true}}}}},
          {"hoverProvider", true}, {"definitionProvider", true}, {"typeDefinitionProvider", true},
          {"implementationProvider", true}, {"referencesProvider", true},
          {"documentHighlightProvider", true}, {"documentSymbolProvider", true},
          {"workspaceSymbolProvider", true}, {"foldingRangeProvider", true},
          {"selectionRangeProvider", true}, {"documentLinkProvider", true},
          {"inlayHintProvider", true},
          {"renameProvider", J::object{{"prepareProvider", true}}},
          {"documentFormattingProvider", true}, {"documentRangeFormattingProvider", true},
          {"documentOnTypeFormattingProvider", J::object{{"firstTriggerCharacter", "}"},
                                                         {"moreTriggerCharacter", J::array{"\n"}}}},
          {"completionProvider", J::object{{"triggerCharacters", J::array{".", ":"}}}},
          {"signatureHelpProvider", J::object{{"triggerCharacters", J::array{"(", ","}}}},
          {"semanticTokensProvider", J::object{{"legend", J::object{{"tokenTypes", token_types},
                                                                   {"tokenModifiers", modifiers}}},
                                                      {"full", true}}},
          {"typeHierarchyProvider", true}, {"callHierarchyProvider", true},
          {"codeActionProvider", J::object{{"codeActionKinds", J::array{"quickfix", "source.organizeImports"}}}}};
    }
  }

  auto server::register_request(const json::value &id) -> void
  {
    std::scoped_lock lock(cancellation_mutex_);
    pending_ids_.insert(json::serialize(id));
  }
  auto server::begin_request(const json::value &id) -> void
  {
    std::scoped_lock lock(cancellation_mutex_);
    active_id_ = json::serialize(id);
    pending_ids_.insert(active_id_);
    active_cancellation_.emplace();
    if (cancelled_ids_.contains(active_id_)) active_cancellation_->cancel();
  }
  auto server::cancel_request(const json::value &id) -> void
  {
    std::scoped_lock lock(cancellation_mutex_);
    const auto key = json::serialize(id);
    if (!pending_ids_.contains(key)) return;
    cancelled_ids_.insert(key);
    if (key == active_id_ && active_cancellation_) active_cancellation_->cancel();
  }
  auto server::end_request(const json::value &id) -> void
  {
    std::scoped_lock lock(cancellation_mutex_);
    const auto key = json::serialize(id);
    pending_ids_.erase(key);
    cancelled_ids_.erase(key);
    if (active_id_ == key) { active_id_.clear(); active_cancellation_.reset(); }
  }
  auto server::active_token() const -> diagnostics::cancellation_token
  {
    std::scoped_lock lock(cancellation_mutex_);
    return active_cancellation_ ? active_cancellation_->token() : diagnostics::cancellation_token{};
  }
  auto server::set_notification_sink(std::function<void(const json::value &)> sink) -> void
  {
    notification_sink_ = std::move(sink);
  }
  auto server::notify(const json::value &message) const -> void
  {
    if (notification_sink_) notification_sink_(message);
  }

  auto server::handle(const json::value &message) -> std::vector<json::value>
  {
    const auto method = string_field(message, "method");
    const auto &params = field(message, "params");
    const auto *id = message.get("id");
    if (method.empty()) return {};
    try
    {
      if (method == "exit") { exit_ = true; return {}; }
      if (method == "initialize")
      {
        if (!id) return {};
        workspace_roots_.clear();
        if (const auto *folders = field(params, "workspaceFolders").elements())
          for (const auto &folder : *folders)
            if (const auto path = documents_->canonicalize(source::document_uri{string_field(folder, "uri")}))
              workspace_roots_.push_back(*path.value);
        if (workspace_roots_.empty())
          if (const auto path = documents_->canonicalize(
                  source::document_uri{string_field(params, "rootUri")}))
            workspace_roots_.push_back(*path.value);
        initialized_ = true;
        return {response(*id, J::object{{"capabilities", capabilities()},
                                         {"serverInfo", J::object{{"name", "Sagan"}, {"version", SAGAN_VERSION}}},
                                         {"experimental", J::object{{"schema", std::string(protocol_schema)},
                                                                    {"compiler", json::parse(language_service::capabilities_json())}}}})};
      }
      if (method == "shutdown")
      { shutdown_ = true; return id ? std::vector<J>{response(*id, nullptr)} : std::vector<J>{}; }
      if (!initialized_ || shutdown_)
        return id ? std::vector<J>{error(*id, -32002, "Sagan language server is not initialized")}
                  : std::vector<J>{};
      if (method == "workspace/didChangeWorkspaceFolders")
      {
        const auto &event = field(params, "event");
        if (const auto *removed = field(event, "removed").elements())
          for (const auto &folder : *removed)
            if (const auto path = documents_->canonicalize(source::document_uri{string_field(folder, "uri")}))
              std::erase(workspace_roots_, *path.value);
        if (const auto *added = field(event, "added").elements())
          for (const auto &folder : *added)
            if (const auto path = documents_->canonicalize(source::document_uri{string_field(folder, "uri")}))
              if (std::find(workspace_roots_.begin(), workspace_roots_.end(), *path.value) == workspace_roots_.end())
                workspace_roots_.push_back(*path.value);
        return {};
      }
      if (method == "workspace/didChangeWatchedFiles" || method == "workspace/didCreateFiles" ||
          method == "workspace/didRenameFiles" || method == "workspace/didDeleteFiles")
      {
        std::vector<J> updates;
        for (const auto &open : open_documents_) updates.push_back(publish(open));
        return updates;
      }
      if (method == "$/cancelRequest")
      { if (const auto *target = params.get("id")) cancel_request(*target); return {}; }
      if (method == "initialized" ||
          method == "workspace/didChangeConfiguration" || method == "workspace/willCreateFiles")
        return id ? std::vector<J>{response(*id, nullptr)} : std::vector<J>{};
      if (method.starts_with("textDocument/did")) return synchronize(method, params);
      if (!id) return {};
      auto answer = query(method, params, active_token());
      if (active_token().is_cancelled() && method != "sagan/operation" &&
          method != "sagan/tests/run") throw request_cancelled{};
      return {response(*id, std::move(answer))};
    }
    catch (const request_cancelled &failure)
    { return id ? std::vector<J>{error(*id, -32800, failure.what())} : std::vector<J>{}; }
    catch (const request_failed &failure)
    { return id ? std::vector<J>{error(*id, -32803, failure.what())} : std::vector<J>{}; }
    catch (const std::invalid_argument &failure)
    { return id ? std::vector<J>{error(*id, -32602, failure.what())} : std::vector<J>{}; }
    catch (const std::out_of_range &failure)
    { return id ? std::vector<J>{error(*id, -32601, failure.what())} : std::vector<J>{}; }
    catch (const std::exception &failure)
    { return id ? std::vector<J>{error(*id, -32603, failure.what())} : std::vector<J>{}; }
  }

  auto run(std::istream &input, std::ostream &output, server &service) -> int
  {
    constexpr std::size_t max_message = 16 * 1024 * 1024;
    const auto *log_setting = std::getenv("SAGAN_LSP_LOG");
    const bool log_enabled = log_setting && std::string_view(log_setting) == "stderr";
    std::mutex queue_mutex;
    std::mutex output_mutex;
    std::mutex log_mutex;
    std::condition_variable ready;
    std::deque<J> pending;
    bool input_done = false;
    int read_status = 0;
    const auto log = [&](const std::string_view direction, const J &message)
    {
      if (!log_enabled) return;
      const auto method = string_field(message, "method");
      std::string safe_method;
      for (const unsigned char ch : method)
      {
        if (safe_method.size() == 80) break;
        if (std::isalnum(ch) || ch == '/' || ch == '$' || ch == '.' || ch == '_' || ch == '-')
          safe_method.push_back(static_cast<char>(ch));
        else safe_method.push_back('?');
      }
      std::scoped_lock lock(log_mutex);
      std::cerr << "[sagan-lsp] " << direction << ' '
                << (safe_method.empty() ? "response" : safe_method) << '\n';
    };
    const auto send = [&](const J &message)
    {
      log("send", message);
      const auto encoded = json::serialize(message);
      std::scoped_lock lock(output_mutex);
      output << "Content-Length: " << encoded.size() << "\r\n\r\n" << encoded;
      output.flush();
    };
    service.set_notification_sink(send);
    std::jthread worker([&]
    {
      for (;;)
      {
        J message;
        {
          std::unique_lock lock(queue_mutex);
          ready.wait(lock, [&] { return input_done || !pending.empty(); });
          if (pending.empty()) break;
          message = std::move(pending.front());
          pending.pop_front();
        }
        const auto *id = message.get("id");
        if (id) service.begin_request(*id);
        try
        {
          for (const auto &answer : service.handle(message)) send(answer);
        }
        catch (const std::exception &failure)
        { if (id) send(error(*id, -32603, failure.what())); }
        if (id) service.end_request(*id);
      }
    });
    while (read_status == 0)
    {
      std::string header;
      std::optional<std::size_t> length;
      while (std::getline(input, header))
      {
        if (!header.empty() && header.back() == '\r') header.pop_back();
        if (header.empty()) break;
        constexpr std::string_view prefix = "content-length:";
        std::string normalized = header;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (!normalized.starts_with(prefix)) continue;
        const auto raw = std::string_view(normalized).substr(prefix.size());
        auto begin = raw.find_first_not_of(' ');
        if (begin == std::string_view::npos) { read_status = 1; break; }
        std::size_t parsed{};
        const auto result = std::from_chars(raw.data() + begin, raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size() || parsed > max_message)
        { read_status = 1; break; }
        length = parsed;
      }
      if (read_status != 0) break;
      if (!input && !length) break;
      if (!length) { read_status = 1; break; }
      std::string body(*length, '\0');
      input.read(body.data(), static_cast<std::streamsize>(*length));
      if (input.gcount() != static_cast<std::streamsize>(*length)) { read_status = 1; break; }
      try
      {
        auto message = json::parse(body);
        log("receive", message);
        if (string_field(message, "method") == "$/cancelRequest")
        {
          if (const auto *id = field(message, "params").get("id")) service.cancel_request(*id);
          continue;
        }
        const bool exit_requested = string_field(message, "method") == "exit";
        if (const auto *id = message.get("id")) service.register_request(*id);
        {
          std::scoped_lock lock(queue_mutex);
          if (pending.size() >= 256) { read_status = 1; break; }
          pending.push_back(std::move(message));
        }
        ready.notify_one();
        if (exit_requested) break;
      }
      catch (const std::exception &failure)
      { send(error(nullptr, -32700, failure.what())); }
    }
    {
      std::scoped_lock lock(queue_mutex);
      input_done = true;
    }
    ready.notify_one();
    worker.join();
    service.set_notification_sink({});
    return read_status;
  }
}
