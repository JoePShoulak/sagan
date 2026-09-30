#pragma once

#include "../parser/ast_node.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace semantic::units
{
  struct rational
  {
    std::int64_t numerator{1};
    std::int64_t denominator{1};
    int decimal_exponent{};
    int pi_exponent{};

    rational normalized() const;
    auto operator==(const rational &) const -> bool = default;
  };

  auto multiply(rational left, rational right) -> rational;
  auto divide(rational left, rational right) -> rational;
  auto add(rational left, rational right) -> rational;

  using dimensions = std::map<std::string, int>;

  enum class category { linear, affine_point, affine_difference };

  struct descriptor
  {
    std::string name;
    dimensions dimension;
    std::optional<std::string> quantity;
    rational scale;
    rational offset{0, 1};
    category kind{category::linear};
    std::string symbol;
  };

  struct measured_type
  {
    std::string numeric;
    descriptor unit;
  };

  class registry
  {
    std::unordered_map<std::string, dimensions> dimensions_;
    std::unordered_map<std::string, descriptor> units_;
    std::unordered_map<std::string, dimensions> quantities_;
    std::unordered_map<std::string, rational> prefixes_;
    std::unordered_set<std::string> prefixable_;

  public:
    registry();
    auto add_program(const parser::program &program) -> void;
    auto resolve(std::string_view expression, parser::span range) const -> descriptor;
    auto find(std::string_view name) const -> const descriptor *;
    auto prefixed(std::string_view name) const -> std::optional<descriptor>;
    auto dimension_dimensions(std::string_view name) const -> const dimensions *;
    auto quantity_dimensions(std::string_view name) const -> const dimensions *;
    auto infer_quantity(const dimensions &value) const -> std::optional<std::string>;
  };

  auto parse_measured_type(std::string_view type, const registry &units,
                           parser::span range) -> std::optional<measured_type>;
  auto format_type(std::string_view numeric, const descriptor &unit) -> std::string;
  auto compatible(const descriptor &expected, const descriptor &actual) -> bool;
  auto difference_of(const descriptor &point) -> descriptor;
  auto combine(const descriptor &left, const descriptor &right, char operation,
               const registry &units) -> descriptor;
}
