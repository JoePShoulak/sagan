#include "json.hpp"
#include "../parser/unicode.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace sagan::lsp::json
{
  auto value::get(const std::string_view key) const -> const value *
  {
    const auto *object = fields();
    if (!object) return nullptr;
    const auto found = object->find(std::string(key));
    return found == object->end() ? nullptr : &found->second;
  }
  auto value::string() const -> std::optional<std::string_view>
  {
    if (const auto *item = std::get_if<std::string>(&data)) return *item;
    return {};
  }
  auto value::integer() const -> std::optional<std::int64_t>
  {
    if (const auto *item = std::get_if<std::int64_t>(&data)) return *item;
    return {};
  }
  auto value::boolean() const -> std::optional<bool>
  {
    if (const auto *item = std::get_if<bool>(&data)) return *item;
    return {};
  }
  auto value::elements() const -> const array * { return std::get_if<array>(&data); }
  auto value::fields() const -> const object * { return std::get_if<object>(&data); }

  namespace
  {
    auto append_utf8(std::string &output, const std::uint32_t point) -> void
    {
      if (point <= 0x7f) output += static_cast<char>(point);
      else if (point <= 0x7ff)
      {
        output += static_cast<char>(0xc0 | (point >> 6));
        output += static_cast<char>(0x80 | (point & 0x3f));
      }
      else if (point <= 0xffff)
      {
        output += static_cast<char>(0xe0 | (point >> 12));
        output += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (point & 0x3f));
      }
      else
      {
        output += static_cast<char>(0xf0 | (point >> 18));
        output += static_cast<char>(0x80 | ((point >> 12) & 0x3f));
        output += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (point & 0x3f));
      }
    }

    class reader
    {
      std::string_view input_;
      std::size_t cursor_{};
      std::size_t depth_{};

      auto whitespace() -> void
      {
        while (cursor_ < input_.size() && (input_[cursor_] == ' ' || input_[cursor_] == '\n' ||
               input_[cursor_] == '\r' || input_[cursor_] == '\t')) ++cursor_;
      }
      auto expect(const char symbol) -> void
      {
        if (cursor_ >= input_.size() || input_[cursor_++] != symbol)
          throw std::runtime_error("Invalid JSON delimiter");
      }
      auto hex_quad() -> std::uint32_t
      {
        std::uint32_t result = 0;
        for (int index = 0; index < 4; ++index)
        {
          if (cursor_ == input_.size()) throw std::runtime_error("Incomplete JSON Unicode escape");
          const char digit = input_[cursor_++];
          result <<= 4;
          if (digit >= '0' && digit <= '9') result |= static_cast<std::uint32_t>(digit - '0');
          else if (digit >= 'a' && digit <= 'f') result |= static_cast<std::uint32_t>(digit - 'a' + 10);
          else if (digit >= 'A' && digit <= 'F') result |= static_cast<std::uint32_t>(digit - 'A' + 10);
          else throw std::runtime_error("Invalid JSON Unicode escape");
        }
        return result;
      }
      auto quoted() -> std::string
      {
        expect('"');
        std::string result;
        while (cursor_ < input_.size())
        {
          const unsigned char character = static_cast<unsigned char>(input_[cursor_++]);
          if (character == '"') return result;
          if (character < 0x20) throw std::runtime_error("Unescaped JSON control character");
          if (character != '\\') { result += static_cast<char>(character); continue; }
          if (cursor_ == input_.size()) throw std::runtime_error("Incomplete JSON escape");
          const char escape = input_[cursor_++];
          switch (escape)
          {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u':
            {
              auto point = hex_quad();
              if (point >= 0xd800 && point <= 0xdbff)
              {
                expect('\\'); expect('u');
                const auto low = hex_quad();
                if (low < 0xdc00 || low > 0xdfff)
                  throw std::runtime_error("Invalid JSON surrogate pair");
                point = 0x10000 + ((point - 0xd800) << 10) + (low - 0xdc00);
              }
              else if (point >= 0xdc00 && point <= 0xdfff)
                throw std::runtime_error("Unpaired JSON surrogate");
              append_utf8(result, point);
              break;
            }
            default: throw std::runtime_error("Unknown JSON escape");
          }
        }
        throw std::runtime_error("Unterminated JSON string");
      }
      auto item() -> value
      {
        whitespace();
        if (++depth_ > 64) throw std::runtime_error("JSON nesting limit exceeded");
        if (cursor_ == input_.size()) throw std::runtime_error("Incomplete JSON value");
        const char first = input_[cursor_];
        value result;
        if (first == '"') result = quoted();
        else if (first == '{')
        {
          ++cursor_; whitespace();
          value::object object;
          if (cursor_ < input_.size() && input_[cursor_] == '}') ++cursor_;
          else for (;;)
          {
            whitespace();
            const auto key = quoted(); whitespace(); expect(':');
            object.insert_or_assign(key, item()); whitespace();
            if (cursor_ < input_.size() && input_[cursor_] == '}') { ++cursor_; break; }
            expect(',');
          }
          result = std::move(object);
        }
        else if (first == '[')
        {
          ++cursor_; whitespace();
          value::array array;
          if (cursor_ < input_.size() && input_[cursor_] == ']') ++cursor_;
          else for (;;)
          {
            array.push_back(item()); whitespace();
            if (cursor_ < input_.size() && input_[cursor_] == ']') { ++cursor_; break; }
            expect(',');
          }
          result = std::move(array);
        }
        else if (input_.substr(cursor_, 4) == "true") { cursor_ += 4; result = true; }
        else if (input_.substr(cursor_, 5) == "false") { cursor_ += 5; result = false; }
        else if (input_.substr(cursor_, 4) == "null") { cursor_ += 4; result = nullptr; }
        else
        {
          const auto begin = cursor_;
          if (input_[cursor_] == '-') ++cursor_;
          if (cursor_ == input_.size()) throw std::runtime_error("Invalid JSON number");
          if (input_[cursor_] == '0') ++cursor_;
          else
          {
            if (input_[cursor_] < '1' || input_[cursor_] > '9')
              throw std::runtime_error("Invalid JSON number");
            while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_;
          }
          bool fractional = false;
          if (cursor_ < input_.size() && input_[cursor_] == '.')
          {
            fractional = true; ++cursor_;
            if (cursor_ == input_.size() || input_[cursor_] < '0' || input_[cursor_] > '9')
              throw std::runtime_error("Invalid JSON fraction");
            while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_;
          }
          if (cursor_ < input_.size() && (input_[cursor_] == 'e' || input_[cursor_] == 'E'))
          {
            fractional = true; ++cursor_;
            if (cursor_ < input_.size() && (input_[cursor_] == '+' || input_[cursor_] == '-')) ++cursor_;
            if (cursor_ == input_.size() || input_[cursor_] < '0' || input_[cursor_] > '9')
              throw std::runtime_error("Invalid JSON exponent");
            while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_;
          }
          const auto number = input_.substr(begin, cursor_ - begin);
          if (fractional)
          {
            double parsed{};
            const auto converted = std::from_chars(number.data(), number.data() + number.size(), parsed);
            if (converted.ec != std::errc{} || !std::isfinite(parsed))
              throw std::runtime_error("Invalid JSON number");
            result = parsed;
          }
          else
          {
            std::int64_t parsed{};
            const auto converted = std::from_chars(number.data(), number.data() + number.size(), parsed);
            if (converted.ec != std::errc{}) throw std::runtime_error("JSON integer outside supported range");
            result = parsed;
          }
        }
        --depth_;
        return result;
      }
    public:
      explicit reader(const std::string_view input) : input_(input) {}
      auto all() -> value
      {
        auto result = item(); whitespace();
        if (cursor_ != input_.size()) throw std::runtime_error("Trailing JSON content");
        return result;
      }
    };

    auto quote_string(const std::string_view text) -> std::string
    {
      std::string output = "\"";
      constexpr char hex[] = "0123456789abcdef";
      for (const unsigned char character : text)
      {
        if (character == '"' || character == '\\') { output += '\\'; output += static_cast<char>(character); }
        else if (character < 0x20)
        {
          output += "\\u00";
          output += hex[character >> 4]; output += hex[character & 15];
        }
        else output += static_cast<char>(character);
      }
      return output + '"';
    }
  }

  auto parse(const std::string_view text) -> value
  {
    if (unicode::first_invalid_utf8(text)) throw std::runtime_error("Invalid UTF-8 JSON message");
    return reader(text).all();
  }

  auto serialize(const value &item) -> std::string
  {
    if (std::holds_alternative<std::nullptr_t>(item.data)) return "null";
    if (const auto *boolean = std::get_if<bool>(&item.data)) return *boolean ? "true" : "false";
    if (const auto *integer = std::get_if<std::int64_t>(&item.data)) return std::to_string(*integer);
    if (const auto *number = std::get_if<double>(&item.data))
    {
      char buffer[64]{};
      const auto result = std::to_chars(buffer, buffer + sizeof(buffer), *number);
      if (result.ec != std::errc{}) throw std::runtime_error("Could not serialize JSON number");
      return {buffer, result.ptr};
    }
    if (const auto *string = std::get_if<std::string>(&item.data)) return quote_string(*string);
    if (const auto *array = item.elements())
    {
      std::string output = "[";
      for (const auto &element : *array)
      { if (output.size() != 1) output += ','; output += serialize(element); }
      return output + ']';
    }
    std::string output = "{";
    for (const auto &[key, element] : *item.fields())
    { if (output.size() != 1) output += ','; output += quote_string(key) + ':' + serialize(element); }
    return output + '}';
  }
}
