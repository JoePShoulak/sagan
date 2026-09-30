#pragma once

#include "../codegen/cpp_generator.hpp"
#include "../semantic/analyzer.hpp"
#include "../source/source.hpp"
#include "../source/provider.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view debug_metadata_schema_version = "sagan-debug-metadata-v1";

  struct debug_breakpoint
  {
    source::source_range source;
    std::optional<std::filesystem::path> source_path;
    std::size_t generated_begin{};
    std::size_t generated_end{};
    std::string generated_function;
  };

  struct debug_function
  {
    std::optional<semantic::symbol_id> symbol;
    std::string generated_name;
    source::source_range source;
    std::optional<std::filesystem::path> source_path;
    std::size_t generated_begin{};
    std::size_t generated_end{};
  };

  struct debug_scope
  {
    std::size_t id{};
    std::size_t parent{};
    std::string label;
    source::source_range lifetime;
    std::optional<std::filesystem::path> source_path;
  };

  enum class debug_value_representation { shared_value, direct_value, unavailable };

  struct debug_variable
  {
    semantic::symbol_id symbol;
    std::string name;
    std::string generated_name;
    std::string type;
    std::size_t scope_id{};
    source::source_range lifetime;
    std::optional<std::filesystem::path> source_path;
    debug_value_representation representation{debug_value_representation::unavailable};
    bool available_in_optimized{};
  };

  struct debug_expression_hook
  {
    semantic::symbol_id symbol;
    source::source_range source;
    std::string generated_expression;
    bool available_in_optimized{};
  };

  struct debug_metadata
  {
    source::document_identity document;
    source::document_version version{};
    std::vector<debug_function> functions;
    std::vector<debug_breakpoint> breakpoints;
    std::vector<debug_scope> scopes;
    std::vector<debug_variable> variables;
    std::vector<debug_expression_hook> expression_hooks;
  };

  auto derive_debug_metadata(const source::document_snapshot &document,
                             const semantic::semantic_model &model,
                             const semantic::type_model &types,
                             const codegen::generated_cpp &generated,
                             const source::source_provider *provider = nullptr) -> debug_metadata;

  auto source_for_stack_frame(const debug_metadata &metadata,
                              const codegen::generated_cpp &generated,
                              std::string_view generated_function,
                              std::size_t one_based_line,
                              std::size_t one_based_column)
    -> std::optional<source::source_range>;
}
