#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::source
{
  using document_version = std::int64_t;
  using byte_offset = std::uint32_t;

  struct document_uri
  {
    std::string value;

    auto operator==(const document_uri &) const -> bool = default;
  };

  struct document_id
  {
    std::uint64_t value{};

    auto operator==(const document_id &) const -> bool = default;
  };

  struct utf16_position
  {
    std::uint32_t line{};
    std::uint32_t character{};

    auto operator==(const utf16_position &) const -> bool = default;
  };

  struct byte_range
  {
    byte_offset begin{};
    byte_offset end{};

    auto operator==(const byte_range &) const -> bool = default;
  };

  struct source_range
  {
    document_id document;
    byte_range bytes;

    auto operator==(const source_range &) const -> bool = default;
  };

  struct text_edit
  {
    source_range range;
    std::string replacement_utf8;
  };

  struct document_identity
  {
    document_id id;
    document_uri uri;
    std::optional<std::filesystem::path> canonical_path;
  };

  class line_index
  {
    std::vector<byte_offset> line_starts_{0};

  public:
    explicit line_index(std::string_view utf8_text);

    auto line_count() const -> std::size_t;
    auto to_utf16(std::string_view utf8_text, byte_offset offset) const -> std::optional<utf16_position>;
    auto to_byte(std::string_view utf8_text, utf16_position position) const -> std::optional<byte_offset>;
  };

  class document_snapshot
  {
    document_identity identity_;
    document_version version_;
    std::string text_;
    line_index lines_;

  public:
    document_snapshot(document_identity identity, document_version version, std::string utf8_text);

    auto identity() const -> const document_identity &;
    auto version() const -> document_version;
    auto text() const -> std::string_view;
    auto lines() const -> const line_index &;
    auto to_utf16(byte_offset offset) const -> std::optional<utf16_position>;
    auto to_byte(utf16_position position) const -> std::optional<byte_offset>;
  };

  auto identity_from_path(document_id id, const std::filesystem::path &path) -> document_identity;
}

