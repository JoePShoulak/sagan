#pragma once

#include "language_service.hpp"
#include "../source/provider.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sagan::language_service
{
  struct analysis_request
  {
    source::document_snapshot document;
    std::uint64_t generation{};
    std::uint64_t request_id{};
    diagnostics::cancellation_token cancellation;
  };

  class workspace
  {
    struct cached_analysis
    {
      source::document_version version{};
      std::uint64_t generation{};
      diagnostics::analysis_result<check_summary> result;
    };

    struct active_analysis
    {
      std::uint64_t request_id{};
      diagnostics::cancellation_source cancellation;
    };

    std::shared_ptr<source::document_store> documents_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::uint64_t> generations_;
    std::unordered_map<std::string, active_analysis> active_requests_;
    std::uint64_t next_request_id_{1};
    std::unordered_map<std::string, cached_analysis> cache_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependents_;

    auto invalidate_locked(const std::string &uri) -> std::size_t;

  public:
    explicit workspace(std::shared_ptr<source::document_store> documents =
                           std::make_shared<source::document_store>());

    auto documents() const -> const source::document_store &;
    auto open(source::document_uri uri, source::document_version version, std::string text)
      -> source::provider_result<void>;
    auto change(const source::document_uri &uri, source::document_version expected_version,
                source::document_version new_version, std::vector<source::text_edit> edits)
      -> source::provider_result<void>;
    auto replace(const source::document_uri &uri, source::document_version expected_version,
                 source::document_version new_version, std::string text) -> source::provider_result<void>;
    auto save(const source::document_uri &uri, source::document_version version,
              std::optional<std::string> text = {}) -> source::provider_result<void>;
    auto close(const source::document_uri &uri) -> source::provider_result<void>;

    auto set_dependencies(const source::document_uri &document,
                          const std::vector<source::document_uri> &dependencies) -> void;
    auto invalidate(const source::document_uri &uri) -> std::size_t;
    auto begin_analysis(const source::document_uri &uri) -> source::provider_result<analysis_request>;
    auto finish_analysis(const analysis_request &request, diagnostics::analysis_result<check_summary> result)
      -> diagnostics::analysis_result<check_summary>;
    auto analyze(const source::document_uri &uri, check_options options = {})
      -> diagnostics::analysis_result<check_summary>;
  };
}
