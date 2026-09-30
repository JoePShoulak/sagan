#pragma once

#include "source.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sagan::source
{
  enum class provider_error_code
  {
    not_found,
    invalid_uri,
    already_open,
    not_open,
    version_mismatch,
    invalid_version,
    invalid_edit,
    io_error,
  };

  struct provider_error
  {
    provider_error_code code;
    std::string message;
  };

  template<typename Value>
  struct provider_result
  {
    std::optional<Value> value;
    std::optional<provider_error> error;

    explicit operator bool() const { return value.has_value(); }
  };

  template<>
  struct provider_result<void>
  {
    bool succeeded{};
    std::optional<provider_error> error;

    explicit operator bool() const { return succeeded; }
  };

  class source_provider
  {
  public:
    virtual ~source_provider() = default;
    virtual auto read(const document_uri &uri) const -> provider_result<document_snapshot> = 0;
    virtual auto read_path(const std::filesystem::path &path) const -> provider_result<document_snapshot> = 0;
    virtual auto exists_path(const std::filesystem::path &path) const -> bool = 0;
    virtual auto canonicalize(const document_uri &uri) const -> provider_result<std::filesystem::path> = 0;
  };

  class disk_source_provider final : public source_provider
  {
    mutable std::unordered_map<std::string, document_id> identities_;
    mutable std::uint64_t next_identity_{1};
    mutable std::mutex mutex_;

    auto identity_for(const std::filesystem::path &path) const -> document_identity;

  public:
    auto read(const document_uri &uri) const -> provider_result<document_snapshot> override;
    auto read_path(const std::filesystem::path &path) const -> provider_result<document_snapshot> override;
    auto exists_path(const std::filesystem::path &path) const -> bool override;
    auto canonicalize(const document_uri &uri) const -> provider_result<std::filesystem::path> override;
  };

  class document_store final : public source_provider
  {
    struct overlay
    {
      document_snapshot snapshot;
      bool saved{};
    };

    std::shared_ptr<source_provider> fallback_;
    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::string, overlay> overlays_;
    std::unordered_map<std::string, std::string> path_to_uri_;
    std::uint64_t next_untitled_identity_{1ULL << 63};

    auto path_key(const std::filesystem::path &path) const -> std::string;
    auto overlay_for(const document_uri &uri) const -> const overlay *;

  public:
    explicit document_store(std::shared_ptr<source_provider> fallback = std::make_shared<disk_source_provider>());

    auto open(document_uri uri, document_version version, std::string utf8_text) -> provider_result<void>;
    auto change(const document_uri &uri, document_version expected_version, document_version new_version,
                std::vector<text_edit> edits) -> provider_result<void>;
    auto replace(const document_uri &uri, document_version expected_version, document_version new_version,
                 std::string utf8_text) -> provider_result<void>;
    auto save(const document_uri &uri, document_version version,
              std::optional<std::string> utf8_text = {}) -> provider_result<void>;
    auto close(const document_uri &uri) -> provider_result<void>;
    auto is_open(const document_uri &uri) const -> bool;
    auto current_version(const document_uri &uri) const -> std::optional<document_version>;

    auto read(const document_uri &uri) const -> provider_result<document_snapshot> override;
    auto read_path(const std::filesystem::path &path) const -> provider_result<document_snapshot> override;
    auto exists_path(const std::filesystem::path &path) const -> bool override;
    auto canonicalize(const document_uri &uri) const -> provider_result<std::filesystem::path> override;
  };
}
