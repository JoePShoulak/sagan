#pragma once

#include "edits.hpp"
#include "language_service.hpp"
#include "../semantic/index.hpp"
#include "../semantic/workspace_index.hpp"

#include <string_view>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view source_edits_schema_version = "sagan-source-edits-v1";

  struct source_edit_capability
  {
    std::string_view action;
    bool available{};
    std::string_view limitation;
  };

  auto source_edit_capabilities() -> std::vector<source_edit_capability>;

  // Rename a proven local binding, private member, or a non-exported,
  // non-overloaded function in one document. Exported, entry-point, public
  // member, and cross-file rename remain unavailable until their workspace
  // proofs exist.
  auto rename_local(const source::document_snapshot &document, const semantic::semantic_index &index,
                    source::byte_offset position, std::string_view new_name) -> edit_plan;

  // Rename an exported source symbol, its public export/import spelling, and
  // every identity-resolved workspace reference. Explicit export and import
  // aliases remain independent bindings.
  auto rename_workspace(const source::document_snapshot &document,
                        const semantic::semantic_index &index,
                        const semantic::workspace_semantic_index &workspace,
                        const source::source_provider &source,
                        source::byte_offset position, std::string_view new_name) -> edit_plan;

  // Sort one uninterrupted, comment-free import block at the top of a module.
  // Ambiguous trivia or interleaved imports cause a structured refusal.
  auto organize_imports(const source::document_snapshot &document) -> edit_plan;

  // Require one exact public workspace export and a module declaration.
  auto add_missing_import(const source::document_snapshot &document,
                          const semantic::workspace_semantic_index &workspace,
                          const semantic::symbol_id &target) -> edit_plan;

  // Only fixes attached to this exact analyzed document/version are accepted.
  auto plan_diagnostic_fix(const source::document_snapshot &document,
                           const diagnostics::analysis_result<check_summary> &analysis,
                           std::size_t diagnostic_index, std::size_t fix_index) -> edit_plan;
}
