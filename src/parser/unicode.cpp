#include "unicode.hpp"

#include "unicode_tables.hpp"
#include <uni_algo/norm.h>

#include <algorithm>
#include <array>

namespace
{
  using sagan::unicode::tables::range;

  template <std::size_t Size>
  auto in_ranges(const char32_t value, const std::array<range, Size> &ranges) -> bool
  {
    const auto found = std::lower_bound(
        ranges.begin(), ranges.end(), value,
        [](const range &entry, const char32_t candidate) { return entry.end < candidate; });
    return found != ranges.end() && found->begin <= value;
  }

  auto is_variation_selector(const char32_t value) -> bool
  {
    return value == 0xfe0e || value == 0xfe0f;
  }

  auto is_regional_indicator(const char32_t value) -> bool
  {
    return value >= 0x1f1e6 && value <= 0x1f1ff;
  }

  auto is_tag_character(const char32_t value) -> bool
  {
    return value >= 0xe0020 && value <= 0xe007e;
  }
}

namespace sagan::unicode
{
  auto decode(const std::string_view text, const std::size_t offset) -> std::optional<code_point>
  {
    if (offset >= text.size())
    {
      return {};
    }

    const auto first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7f)
    {
      return code_point{first, 1};
    }

    std::size_t width = 0;
    char32_t value = 0;
    char32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf)
    {
      width = 2;
      value = first & 0x1f;
      minimum = 0x80;
    }
    else if (first >= 0xe0 && first <= 0xef)
    {
      width = 3;
      value = first & 0x0f;
      minimum = 0x800;
    }
    else if (first >= 0xf0 && first <= 0xf4)
    {
      width = 4;
      value = first & 0x07;
      minimum = 0x10000;
    }
    else
    {
      return {};
    }

    if (offset + width > text.size())
    {
      return {};
    }
    for (std::size_t i = 1; i < width; i++)
    {
      const auto next = static_cast<unsigned char>(text[offset + i]);
      if ((next & 0xc0) != 0x80)
      {
        return {};
      }
      value = static_cast<char32_t>((value << 6) | (next & 0x3f));
    }

    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
    {
      return {};
    }
    return code_point{value, width};
  }

  auto first_invalid_utf8(const std::string_view text) -> std::optional<std::size_t>
  {
    std::size_t offset = 0;
    while (offset < text.size())
    {
      const auto current = decode(text, offset);
      if (!current)
      {
        return offset;
      }
      offset += current->width;
    }
    return {};
  }

  auto is_xid_start(const char32_t value) -> bool
  {
    return in_ranges(value, tables::xid_start);
  }

  auto is_xid_continue(const char32_t value) -> bool
  {
    return in_ranges(value, tables::xid_continue);
  }

  auto emoji_sequence_length(const std::string_view text, const std::size_t offset) -> std::size_t
  {
    auto current = decode(text, offset);
    if (!current)
    {
      return 0;
    }

    if (is_regional_indicator(current->value))
    {
      std::size_t end = offset + current->width;
      const auto second = decode(text, end);
      if (second && is_regional_indicator(second->value))
      {
        end += second->width;
      }
      return end - offset;
    }

    if (!in_ranges(current->value, tables::extended_pictographic))
    {
      return 0;
    }

    std::size_t end = offset + current->width;
    const auto consume_suffix = [&text](std::size_t position) {
      auto suffix = decode(text, position);
      if (suffix && is_variation_selector(suffix->value))
      {
        position += suffix->width;
        suffix = decode(text, position);
      }
      if (suffix && in_ranges(suffix->value, sagan::unicode::tables::emoji_modifier))
      {
        position += suffix->width;
      }
      return position;
    };
    end = consume_suffix(end);

    if (current->value == 0x1f3f4)
    {
      std::size_t tag_end = end;
      auto tag = decode(text, tag_end);
      while (tag && is_tag_character(tag->value))
      {
        tag_end += tag->width;
        tag = decode(text, tag_end);
      }
      if (tag_end > end && tag && tag->value == 0xe007f)
      {
        end = tag_end + tag->width;
      }
    }

    while (true)
    {
      const auto joiner = decode(text, end);
      if (!joiner || joiner->value != 0x200d)
      {
        break;
      }
      const std::size_t joined_begin = end + joiner->width;
      const auto joined = decode(text, joined_begin);
      if (!joined || !in_ranges(joined->value, tables::extended_pictographic))
      {
        break;
      }
      end = consume_suffix(joined_begin + joined->width);
    }
    return end - offset;
  }

  auto normalize_nfc(const std::string_view text) -> std::string
  {
    return una::norm::to_nfc_utf8(text);
  }
}
