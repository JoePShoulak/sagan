#include "provider.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace sagan::source
{
  namespace
  {
    auto failure(const provider_error_code code, std::string message) -> provider_result<void>
    {
      return {false, provider_error{code, std::move(message)}};
    }

    template<typename Value>
    auto failure(const provider_error_code code, std::string message) -> provider_result<Value>
    {
      return {{}, provider_error{code, std::move(message)}};
    }

    auto canonical_path(const std::filesystem::path &path) -> std::filesystem::path
    {
      return std::filesystem::absolute(path).lexically_normal();
    }

    auto uri_path(const document_uri &uri) -> provider_result<std::filesystem::path>
    {
      constexpr std::string_view prefix = "file://";
      if (!uri.value.starts_with(prefix))
        return failure<std::filesystem::path>(provider_error_code::invalid_uri,
                                              "Document URI is not a file URI: " + uri.value);
      std::string encoded = uri.value.substr(prefix.size());
      std::string decoded;
      decoded.reserve(encoded.size());
      const auto hex = [](const char value) -> int
      {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
      };
      for (std::size_t index = 0; index < encoded.size(); ++index)
      {
        if (encoded[index] != '%') { decoded.push_back(encoded[index]); continue; }
        if (index + 2 >= encoded.size() || hex(encoded[index + 1]) < 0 || hex(encoded[index + 2]) < 0)
          return failure<std::filesystem::path>(provider_error_code::invalid_uri,
                                                "Document URI contains invalid percent encoding: " + uri.value);
        decoded.push_back(static_cast<char>((hex(encoded[index + 1]) << 4) | hex(encoded[index + 2])));
        index += 2;
      }
#ifdef _WIN32
      if (decoded.size() >= 3 && decoded[0] == '/' && std::isalpha(static_cast<unsigned char>(decoded[1])) != 0 &&
          decoded[2] == ':') decoded.erase(decoded.begin());
#endif
      return {canonical_path(std::filesystem::path(decoded)), {}};
    }

    auto read_text(const std::filesystem::path &path) -> provider_result<std::string>
    {
      std::ifstream input(path, std::ios::binary);
      if (!input)
        return failure<std::string>(provider_error_code::not_found, "Could not open source '" + path.string() + "'");
      std::ostringstream contents;
      contents << input.rdbuf();
      if (input.bad())
        return failure<std::string>(provider_error_code::io_error, "Could not read source '" + path.string() + "'");
      return {contents.str(), {}};
    }
  }

  auto disk_source_provider::identity_for(const std::filesystem::path &path) const -> document_identity
  {
    std::scoped_lock lock(mutex_);
    const auto absolute = canonical_path(path);
    const auto key = absolute.generic_string();
    const auto [found, inserted] = identities_.try_emplace(key, document_id{next_identity_});
    if (inserted) ++next_identity_;
    return identity_from_path(found->second, absolute);
  }

  auto disk_source_provider::read(const document_uri &uri) const -> provider_result<document_snapshot>
  {
    const auto path = canonicalize(uri);
    if (!path) return failure<document_snapshot>(path.error->code, path.error->message);
    return read_path(*path.value);
  }

  auto disk_source_provider::read_path(const std::filesystem::path &path) const
    -> provider_result<document_snapshot>
  {
    const auto absolute = canonical_path(path);
    auto text = read_text(absolute);
    if (!text) return failure<document_snapshot>(text.error->code, text.error->message);
    return {document_snapshot(identity_for(absolute), 0, std::move(*text.value)), {}};
  }

  auto disk_source_provider::exists_path(const std::filesystem::path &path) const -> bool
  {
    return std::filesystem::is_regular_file(canonical_path(path));
  }

  auto disk_source_provider::canonicalize(const document_uri &uri) const
    -> provider_result<std::filesystem::path>
  {
    return uri_path(uri);
  }

  document_store::document_store(std::shared_ptr<source_provider> fallback) : fallback_(std::move(fallback)) {}

  auto document_store::path_key(const std::filesystem::path &path) const -> std::string
  {
    auto key = canonical_path(path).generic_string();
#ifdef _WIN32
    std::ranges::transform(key, key.begin(), [](const unsigned char value) { return std::tolower(value); });
#endif
    return key;
  }

  auto document_store::overlay_for(const document_uri &uri) const -> const overlay *
  {
    const auto found = overlays_.find(uri.value);
    return found == overlays_.end() ? nullptr : &found->second;
  }

  auto document_store::open(document_uri uri, const document_version version, std::string utf8_text)
    -> provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    if (version < 0) return failure(provider_error_code::invalid_version, "Document version cannot be negative");
    if (overlays_.contains(uri.value))
      return failure(provider_error_code::already_open, "Document is already open: " + uri.value);
    document_identity identity;
    const auto path = fallback_->canonicalize(uri);
    if (path)
    {
      if (path_to_uri_.contains(path_key(*path.value)))
        return failure(provider_error_code::already_open,
                       "A document overlay is already open for path: " + path.value->string());
      identity = identity_from_path(document_id{next_untitled_identity_++}, *path.value);
        // Keep the editor's URI spelling while retaining the canonical path
        // for overlay-first imports. LSP replies must address the opened URI.
        identity.uri = uri;
      path_to_uri_[path_key(*path.value)] = uri.value;
    }
    else identity = document_identity{document_id{next_untitled_identity_++}, uri, {}};
    overlays_.emplace(uri.value, overlay{document_snapshot(std::move(identity), version, std::move(utf8_text)), false});
    return {true, {}};
  }

  auto document_store::replace(const document_uri &uri, const document_version expected_version,
                               const document_version new_version, std::string utf8_text)
    -> provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto found = overlays_.find(uri.value);
    if (found == overlays_.end()) return failure(provider_error_code::not_open, "Document is not open: " + uri.value);
    if (found->second.snapshot.version() != expected_version)
      return failure(provider_error_code::version_mismatch, "Document version does not match the open snapshot");
    if (new_version <= expected_version)
      return failure(provider_error_code::invalid_version, "Document versions must increase monotonically");
    const auto identity = found->second.snapshot.identity();
    found->second = overlay{document_snapshot(identity, new_version, std::move(utf8_text)), false};
    return {true, {}};
  }

  auto document_store::change(const document_uri &uri, const document_version expected_version,
                              const document_version new_version, std::vector<text_edit> edits)
    -> provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    const auto found = overlays_.find(uri.value);
    if (found == overlays_.end()) return failure(provider_error_code::not_open, "Document is not open: " + uri.value);
    if (found->second.snapshot.version() != expected_version)
      return failure(provider_error_code::version_mismatch, "Document version does not match the open snapshot");
    if (new_version <= expected_version)
      return failure(provider_error_code::invalid_version, "Document versions must increase monotonically");
    const auto &snapshot = found->second.snapshot;
    std::ranges::sort(edits, {}, [](const text_edit &edit) { return edit.range.bytes.begin; });
    byte_offset previous_end = 0;
    for (const auto &edit : edits)
    {
      if (edit.range.document != snapshot.identity().id || edit.range.bytes.begin < previous_end ||
          edit.range.bytes.begin > edit.range.bytes.end || edit.range.bytes.end > snapshot.text().size() ||
          !snapshot.to_utf16(edit.range.bytes.begin) || !snapshot.to_utf16(edit.range.bytes.end))
        return failure(provider_error_code::invalid_edit, "Document edits must be ordered, non-overlapping UTF-8 ranges");
      previous_end = edit.range.bytes.end;
    }
    std::string updated(snapshot.text());
    for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit)
      updated.replace(edit->range.bytes.begin, edit->range.bytes.end - edit->range.bytes.begin,
                      edit->replacement_utf8);
    return replace(uri, expected_version, new_version, std::move(updated));
  }

  auto document_store::save(const document_uri &uri, const document_version version,
                            std::optional<std::string> utf8_text) -> provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    auto found = overlays_.find(uri.value);
    if (found == overlays_.end()) return failure(provider_error_code::not_open, "Document is not open: " + uri.value);
    if (found->second.snapshot.version() != version)
      return failure(provider_error_code::version_mismatch, "Saved document version does not match the open snapshot");
    if (utf8_text)
    {
      const auto identity = found->second.snapshot.identity();
      found->second.snapshot = document_snapshot(identity, version, std::move(*utf8_text));
    }
    found->second.saved = true;
    return {true, {}};
  }

  auto document_store::close(const document_uri &uri) -> provider_result<void>
  {
    std::scoped_lock lock(mutex_);
    const auto found = overlays_.find(uri.value);
    if (found == overlays_.end()) return failure(provider_error_code::not_open, "Document is not open: " + uri.value);
    if (found->second.snapshot.identity().canonical_path)
      path_to_uri_.erase(path_key(*found->second.snapshot.identity().canonical_path));
    overlays_.erase(found);
    return {true, {}};
  }

  auto document_store::is_open(const document_uri &uri) const -> bool
  {
    std::scoped_lock lock(mutex_);
    return overlays_.contains(uri.value);
  }

  auto document_store::current_version(const document_uri &uri) const -> std::optional<document_version>
  {
    std::scoped_lock lock(mutex_);
    const auto value = overlay_for(uri);
    return value ? std::optional<document_version>{value->snapshot.version()} : std::nullopt;
  }

  auto document_store::read(const document_uri &uri) const -> provider_result<document_snapshot>
  {
    std::scoped_lock lock(mutex_);
    if (const auto value = overlay_for(uri)) return {value->snapshot, {}};
    return fallback_->read(uri);
  }

  auto document_store::read_path(const std::filesystem::path &path) const -> provider_result<document_snapshot>
  {
    std::scoped_lock lock(mutex_);
    if (const auto found = path_to_uri_.find(path_key(path)); found != path_to_uri_.end())
      return read(document_uri{found->second});
    return fallback_->read_path(path);
  }

  auto document_store::exists_path(const std::filesystem::path &path) const -> bool
  {
    std::scoped_lock lock(mutex_);
    return path_to_uri_.contains(path_key(path)) || fallback_->exists_path(path);
  }

  auto document_store::canonicalize(const document_uri &uri) const -> provider_result<std::filesystem::path>
  {
    std::scoped_lock lock(mutex_);
    if (const auto value = overlay_for(uri); value && value->snapshot.identity().canonical_path)
      return {*value->snapshot.identity().canonical_path, {}};
    return fallback_->canonicalize(uri);
  }
}
