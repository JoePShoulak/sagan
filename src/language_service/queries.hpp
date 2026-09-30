#pragma once

#include "language_service.hpp"
#include "documentation.hpp"
#include "../semantic/workspace_index.hpp"
#include "../syntax/syntax.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sagan::language_service
{
  struct symbol_occurrence
  {
    semantic::symbol_id id;
    semantic::symbol_kind kind;
    semantic::symbol_origin origin;
    source::source_range selection;
    source::source_range declaration;
    std::string name;
  };

  struct hover_information
  {
    symbol_occurrence symbol;
    std::optional<std::string> type;
    std::vector<std::string> documentation;
  };

  struct document_symbol
  {
    symbol_occurrence symbol;
    std::vector<document_symbol> children;
  };

  struct semantic_classification
  {
    semantic::symbol_id id;
    semantic::symbol_kind kind;
    source::source_range range;
    bool declaration{};
    bool read_only{};
    bool private_access{};
    bool builtin{};
    bool deprecated{};
    bool unresolved{};
    bool generated{};
  };

  struct workspace_symbol
  {
    semantic::symbol_id id;
    std::string name;
    semantic::symbol_kind kind;
    source::source_range declaration;
    std::string module;
    semantic::symbol_visibility visibility;
  };

  enum class folding_kind { block, comment, string_literal };

  struct folding_region
  {
    source::source_range range;
    folding_kind kind;
  };

  struct document_link
  {
    source::source_range range;
    source::document_uri target;
  };

  struct completion_item
  {
    std::string label;
    semantic::symbol_id id;
    semantic::symbol_kind kind;
    source::source_range replacement;
    std::string insertion_text;
    std::string detail;
    std::vector<std::string> documentation;
    std::string source_module;
    std::string filter_text;
    std::string sort_text;
    bool deprecated{};
    std::vector<source::text_edit> additional_import_edits;
  };

  struct signature_variant
  {
    semantic::symbol_id callable;
    std::string label;
    std::vector<std::string> parameter_names;
    std::vector<std::string> parameter_types;
    std::string result_type;
    std::vector<std::string> documentation;
  };

  struct signature_information
  {
    source::source_range call;
    std::string label;
    std::vector<std::string> parameter_types;
    std::vector<std::string> parameter_names;
    std::vector<std::string> generic_names;
    std::vector<std::string> documentation;
    std::string result_type;
    std::size_t active_parameter{};
    std::vector<signature_variant> alternatives;
    std::size_t active_signature{};
  };

  struct type_hierarchy_information
  {
    symbol_occurrence symbol;
    std::vector<symbol_occurrence> supertypes;
    std::vector<symbol_occurrence> subtypes;
  };

  struct inlay_hint
  {
    source::byte_offset position{};
    std::string label;
    semantic::symbol_id symbol;
  };

  struct call_hierarchy_edge
  {
    symbol_occurrence caller;
    symbol_occurrence callee;
    std::vector<source::source_range> call_sites;
  };

  struct call_hierarchy_information
  {
    symbol_occurrence symbol;
    std::vector<call_hierarchy_edge> incoming;
    std::vector<call_hierarchy_edge> outgoing;
  };

  struct scope_information
  {
    std::size_t id{};
    std::string label;
    source::source_range range;
  };

  struct position_context
  {
    std::optional<source::source_range> containing_declaration;
    std::vector<scope_information> scopes;
    std::optional<source::source_range> expression;
    std::optional<std::string> expression_type;
  };

  // These queries consume one immutable analysis result. A caller must never
  // combine an index with a newer editor buffer: a mismatch returns stale.
  class document_queries
  {
    const source::document_snapshot &document_;
    const semantic::semantic_index &index_;
    const semantic::workspace_semantic_index *workspace_;
    const semantic::semantic_model *model_;
    const semantic::type_model *types_;
    std::vector<syntax::lossless_token> tokens_;
    std::vector<syntax::trivia> trailing_trivia_;
    std::unique_ptr<parser::program> tree_;

    auto occurrence_at(source::byte_offset offset) const -> std::optional<symbol_occurrence>;

  public:
    document_queries(const source::document_snapshot &document, const semantic::semantic_index &index,
                     const semantic::workspace_semantic_index *workspace = nullptr,
                     const semantic::semantic_model *model = nullptr,
                     const semantic::type_model *types = nullptr);

    auto symbol_at(source::byte_offset offset) const -> diagnostics::analysis_result<symbol_occurrence>;
    auto symbol_at(source::utf16_position position) const -> diagnostics::analysis_result<symbol_occurrence>;
    auto definitions(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto implementations(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto references(source::byte_offset offset, bool include_declaration = false) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto document_highlights(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto resolved_type(source::byte_offset offset) const -> diagnostics::analysis_result<std::string>;
    auto hover(source::byte_offset offset) const -> diagnostics::analysis_result<hover_information>;
    auto document_symbols() const -> diagnostics::analysis_result<std::vector<document_symbol>>;
    auto semantic_classifications() const
      -> diagnostics::analysis_result<std::vector<semantic_classification>>;
    auto folding_regions() const -> diagnostics::analysis_result<std::vector<folding_region>>;
    auto import_links() const -> diagnostics::analysis_result<std::vector<document_link>>;
    auto completions(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<completion_item>>;
    auto signature_help(source::byte_offset offset) const
      -> diagnostics::analysis_result<signature_information>;
    // Innermost-to-outermost, strictly nested source ranges for editor selection expansion.
    auto selection_ranges(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto type_definitions(source::byte_offset offset) const
      -> diagnostics::analysis_result<std::vector<source::source_range>>;
    auto type_hierarchy(source::byte_offset offset) const
      -> diagnostics::analysis_result<type_hierarchy_information>;
    auto inlay_hints(source::byte_range range) const
      -> diagnostics::analysis_result<std::vector<inlay_hint>>;
    auto call_hierarchy(source::byte_offset offset) const
      -> diagnostics::analysis_result<call_hierarchy_information>;
    auto documentation_at(source::byte_offset offset) const
      -> diagnostics::analysis_result<documentation_entry>;
    auto context_at(source::byte_offset offset) const
      -> diagnostics::analysis_result<position_context>;
  };

  auto search_workspace_symbols(const semantic::workspace_semantic_index &workspace,
                                std::string_view query, std::size_t limit = 100)
    -> std::vector<workspace_symbol>;
}
