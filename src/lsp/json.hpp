#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sagan::lsp::json
{
  struct value
  {
    using array = std::vector<value>;
    using object = std::map<std::string, value>;
    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, array, object> data;

    value() : data(nullptr) {}
    value(std::nullptr_t) : data(nullptr) {}
    value(bool item) : data(item) {}
    value(int item) : data(static_cast<std::int64_t>(item)) {}
    value(std::int64_t item) : data(item) {}
    value(double item) : data(item) {}
    value(const char *item) : data(std::string(item)) {}
    value(std::string item) : data(std::move(item)) {}
    value(array item) : data(std::move(item)) {}
    value(object item) : data(std::move(item)) {}

    auto get(std::string_view key) const -> const value *;
    auto string() const -> std::optional<std::string_view>;
    auto integer() const -> std::optional<std::int64_t>;
    auto boolean() const -> std::optional<bool>;
    auto elements() const -> const array *;
    auto fields() const -> const object *;
  };

  auto parse(std::string_view text) -> value;
  auto serialize(const value &item) -> std::string;
}
