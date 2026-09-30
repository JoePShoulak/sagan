#include "source.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sagan::source
{
  namespace
  {
    struct decoded_code_point
    {
      std::uint32_t value;
      std::size_t width;
    };

    auto decode(const std::string_view text, const std::size_t index) -> std::optional<decoded_code_point>
    {
      if (index >= text.size()) return {};
      const auto first = static_cast<unsigned char>(text[index]);
      if (first < 0x80) return decoded_code_point{first, 1};

      std::size_t width = 0;
      std::uint32_t value = 0;
      std::uint32_t minimum = 0;
      if ((first & 0xe0) == 0xc0) { width = 2; value = first & 0x1f; minimum = 0x80; }
      else if ((first & 0xf0) == 0xe0) { width = 3; value = first & 0x0f; minimum = 0x800; }
      else if ((first & 0xf8) == 0xf0) { width = 4; value = first & 0x07; minimum = 0x10000; }
      else return {};
      if (index + width > text.size()) return {};
      for (std::size_t offset = 1; offset < width; ++offset)
      {
        const auto next = static_cast<unsigned char>(text[index + offset]);
        if ((next & 0xc0) != 0x80) return {};
        value = (value << 6) | (next & 0x3f);
      }
      if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return {};
      return decoded_code_point{value, width};
    }

    auto encoded_uri_path(const std::filesystem::path &path) -> std::string
    {
      static constexpr char hex[] = "0123456789ABCDEF";
      const std::string input = path.generic_string();
      std::string result;
      for (const unsigned char value : input)
      {
        const bool safe = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                          (value >= '0' && value <= '9') || value == '-' || value == '_' ||
                          value == '.' || value == '~' || value == '/' || value == ':';
        if (safe) result.push_back(static_cast<char>(value));
        else
        {
          result.push_back('%');
          result.push_back(hex[value >> 4]);
          result.push_back(hex[value & 0x0f]);
        }
      }
      return result;
    }
  }

  line_index::line_index(const std::string_view utf8_text)
  {
    if (utf8_text.size() > std::numeric_limits<byte_offset>::max())
      throw std::length_error("Sagan source document exceeds the supported byte-offset range");
    for (std::size_t index = 0; index < utf8_text.size(); ++index)
    {
      if (utf8_text[index] == '\r')
      {
        if (index + 1 < utf8_text.size() && utf8_text[index + 1] == '\n') ++index;
        line_starts_.push_back(static_cast<byte_offset>(index + 1));
      }
      else if (utf8_text[index] == '\n') line_starts_.push_back(static_cast<byte_offset>(index + 1));
    }
  }

  auto line_index::line_count() const -> std::size_t { return line_starts_.size(); }

  auto line_index::to_utf16(const std::string_view text, const byte_offset offset) const
    -> std::optional<utf16_position>
  {
    if (offset > text.size()) return {};
    const auto upper = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    const std::size_t line = static_cast<std::size_t>(upper - line_starts_.begin() - 1);
    const std::size_t start = line_starts_[line];
    std::uint32_t character = 0;
    std::size_t index = start;
    while (index < offset)
    {
      const auto current = decode(text, index);
      if (!current || index + current->width > offset || current->value == '\n' || current->value == '\r') return {};
      character += current->value > 0xffff ? 2U : 1U;
      index += current->width;
    }
    return utf16_position{static_cast<std::uint32_t>(line), character};
  }

  auto line_index::to_byte(const std::string_view text, const utf16_position position) const
    -> std::optional<byte_offset>
  {
    if (position.line >= line_starts_.size()) return {};
    std::size_t index = line_starts_[position.line];
    std::uint32_t character = 0;
    while (character < position.character)
    {
      if (index >= text.size() || text[index] == '\n' || text[index] == '\r') return {};
      const auto current = decode(text, index);
      if (!current) return {};
      const std::uint32_t width = current->value > 0xffff ? 2U : 1U;
      if (character + width > position.character) return {};
      character += width;
      index += current->width;
    }
    return static_cast<byte_offset>(index);
  }

  document_snapshot::document_snapshot(document_identity identity, const document_version version,
                                       std::string utf8_text)
      : identity_(std::move(identity)), version_(version), text_(std::move(utf8_text)), lines_(text_)
  {
  }

  auto document_snapshot::identity() const -> const document_identity & { return identity_; }
  auto document_snapshot::version() const -> document_version { return version_; }
  auto document_snapshot::text() const -> std::string_view { return text_; }
  auto document_snapshot::lines() const -> const line_index & { return lines_; }
  auto document_snapshot::to_utf16(const byte_offset offset) const -> std::optional<utf16_position>
  {
    return lines_.to_utf16(text_, offset);
  }
  auto document_snapshot::to_byte(const utf16_position position) const -> std::optional<byte_offset>
  {
    return lines_.to_byte(text_, position);
  }

  auto identity_from_path(const document_id id, const std::filesystem::path &path) -> document_identity
  {
    const auto canonical = std::filesystem::absolute(path).lexically_normal();
    std::string encoded = encoded_uri_path(canonical);
    if (!encoded.starts_with('/')) encoded.insert(encoded.begin(), '/');
    return document_identity{id, document_uri{"file://" + encoded}, canonical};
  }
}

