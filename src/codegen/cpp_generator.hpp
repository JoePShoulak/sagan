#pragma once

#include "../parser/ast_node.hpp"
#include "../semantic/type_checker.hpp"

#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <vector>

namespace codegen
{
  inline constexpr const char *source_map_schema_version = "sagan-cpp-source-map-v1";

  struct source_map_entry
  {
    std::size_t generated_begin{};
    std::size_t generated_end{};
    parser::span source{};
    std::optional<std::filesystem::path> source_path;
    std::string generated_function;
    bool breakpoint{};
  };

  struct generated_cpp
  {
    std::string text;
    std::vector<source_map_entry> mappings;
  };

  auto generate_cpp(const parser::program &tree, const semantic::type_model &types) -> std::string;
  auto generated_identifier(std::string_view name) -> std::string;
  auto generate_cpp_mapped(const parser::program &tree, const semantic::type_model &types,
                           std::optional<std::filesystem::path> default_source = {}) -> generated_cpp;
  auto generated_offset(const std::string &text, std::size_t one_based_line,
                        std::size_t one_based_column) -> std::optional<std::size_t>;
  auto source_for_generated_offset(const generated_cpp &output, std::size_t offset)
    -> const source_map_entry *;
}
