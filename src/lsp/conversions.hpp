#pragma once

#include "json.hpp"
#include "../source/source.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace sagan::lsp
{
  using J = json::value;

  inline auto field(const J &item, const std::string_view key) -> const J &
  {
    static const J empty;
    if (const auto *found = item.get(key)) return *found;
    return empty;
  }
  inline auto string_field(const J &item, const std::string_view key) -> std::string
  {
    return std::string(field(item, key).string().value_or(""));
  }
  inline auto integer_field(const J &item, const std::string_view key) -> std::int64_t
  {
    const auto parsed = field(item, key).integer();
    if (!parsed) throw std::invalid_argument("Expected integer field: " + std::string(key));
    return *parsed;
  }
  inline auto position(const J &item) -> source::utf16_position
  {
    const auto line = integer_field(item, "line");
    const auto character = integer_field(item, "character");
    if (line < 0 || character < 0 || line > UINT32_MAX || character > UINT32_MAX)
      throw std::invalid_argument("Invalid UTF-16 position");
    return {static_cast<std::uint32_t>(line), static_cast<std::uint32_t>(character)};
  }
  inline auto offset(const source::document_snapshot &document, const J &item) -> source::byte_offset
  {
    const auto converted = document.to_byte(position(item));
    if (!converted) throw std::invalid_argument("Position is outside the document or a surrogate pair");
    return *converted;
  }
  inline auto byte_range(const source::document_snapshot &document, const J &item) -> source::byte_range
  {
    const auto begin = offset(document, field(item, "start"));
    const auto end = offset(document, field(item, "end"));
    if (begin > end) throw std::invalid_argument("Range end precedes its start");
    return {begin, end};
  }
  inline auto lsp_position(const source::utf16_position point) -> J
  {
    return J::object{{"line", static_cast<std::int64_t>(point.line)},
                     {"character", static_cast<std::int64_t>(point.character)}};
  }
  inline auto lsp_range(const source::document_snapshot &document, const source::byte_range bytes) -> J
  {
    const auto start = document.to_utf16(bytes.begin);
    const auto end = document.to_utf16(bytes.end);
    if (!start || !end) throw std::invalid_argument("Compiler range is outside its source document");
    return J::object{{"start", lsp_position(*start)}, {"end", lsp_position(*end)}};
  }
  inline auto lsp_edit(const source::document_snapshot &document, const source::text_edit &edit) -> J
  {
    return J::object{{"range", lsp_range(document, edit.range.bytes)},
                     {"newText", edit.replacement_utf8}};
  }
}
