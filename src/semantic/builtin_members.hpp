#pragma once

#include <optional>
#include <string_view>

namespace semantic
{
  struct builtin_member
  {
    std::string_view name;
    std::string_view result_type;
    std::string_view documentation;
    bool callable{};
  };

  inline auto integer_builtin_member(const std::string_view receiver_type,
                                     const std::string_view member_name)
    -> std::optional<builtin_member>
  {
    if (receiver_type != "Int" && receiver_type != "Int8" && receiver_type != "Int16" &&
        receiver_type != "Int32" && receiver_type != "Int64") return {};
    if (member_name == "times")
      return builtin_member{"times", "Array<Int64>",
                            "Eagerly constructs the array [0, 1, ..., count - 1]. "
                            "A negative count raises RuntimeError.invalid_range."};
    return {};
  }

  inline auto integer_builtin_static_method(const std::string_view receiver_type,
                                            const std::string_view member_name)
    -> std::optional<builtin_member>
  {
    if (receiver_type == "Int" && member_name == "round")
      return builtin_member{"round", "Int64",
                            "Rounds a Float to the nearest Int; exact halves go away from zero. "
                            "NaN, infinity, and out-of-range results raise RuntimeError.invalid_conversion.", true};
    return {};
  }

  inline auto vector_builtin_method(const std::string_view receiver_type,
                                    const std::string_view member_name)
    -> std::optional<builtin_member>
  {
    if (!receiver_type.starts_with("Vector")) return {};
    if (member_name == "squared_length")
      return builtin_member{"squared_length", "component unit squared",
                            "Returns the sum of squared components with squared units.", true};
    if (member_name == "length")
      return builtin_member{"length", "component unit",
                            "Returns the Euclidean length of a finite Vector.", true};
    if (member_name == "normalized")
      return builtin_member{"normalized", "unitless Vector",
                            "Returns a new unitless Vector without changing the receiver.", true};
    if (member_name == "normalized!")
      return builtin_member{"normalized!", "unitless Vector",
                            "Normalizes a mutable unitless Vector in place and returns it.", true};
    return {};
  }
}
