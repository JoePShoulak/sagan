#pragma once

#include "json.hpp"
#include "../diagnostics/diagnostic.hpp"
#include "../source/provider.hpp"

#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sagan::lsp
{
  inline constexpr std::string_view protocol_schema = "sagan-lsp/1";
  struct request_cancelled : std::runtime_error
  {
    request_cancelled() : std::runtime_error("LSP request cancelled") {}
  };

  struct request_failed : std::runtime_error
  {
    explicit request_failed(std::string message) : std::runtime_error(std::move(message)) {}
  };

  class server
  {
    std::shared_ptr<source::document_store> documents_{std::make_shared<source::document_store>()};
    std::vector<source::document_uri> open_documents_;
    std::vector<std::filesystem::path> workspace_roots_;
    bool initialized_{};
    bool shutdown_{};
    bool exit_{};
    mutable std::mutex cancellation_mutex_;
    std::string active_id_;
    std::optional<diagnostics::cancellation_source> active_cancellation_;
    std::unordered_set<std::string> pending_ids_;
    std::unordered_set<std::string> cancelled_ids_;

    auto query(std::string_view method, const json::value &params,
               diagnostics::cancellation_token cancellation) -> json::value;
    auto publish(const source::document_uri &uri) -> json::value;
    auto synchronize(std::string_view method, const json::value &params) -> std::vector<json::value>;

  public:
    auto handle(const json::value &message) -> std::vector<json::value>;
    auto register_request(const json::value &id) -> void;
    auto begin_request(const json::value &id) -> void;
    auto cancel_request(const json::value &id) -> void;
    auto end_request(const json::value &id) -> void;
    auto active_token() const -> diagnostics::cancellation_token;
    auto should_exit() const -> bool { return exit_; }
    auto documents() const -> const source::document_store & { return *documents_; }
  };

  // Reads Content-Length framed UTF-8 JSON-RPC and writes protocol frames only.
  // Diagnostics and optional logging must never write to protocol stdout.
  auto run(std::istream &input, std::ostream &output, server &service) -> int;
}
