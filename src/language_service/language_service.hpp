#pragma once

#include "../diagnostics/diagnostic.hpp"
#include "../semantic/index.hpp"
#include "../source/source.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace sagan::language_service
{
  struct check_options
  {
    bool check_types{true};
    bool check_entry_point{false};
  };

  struct check_summary
  {
    std::size_t token_count{};
    std::size_t statement_count{};
    std::size_t scope_count{};
    std::size_t typed_expression_count{};
  };

  struct semantic_snapshot
  {
    semantic::semantic_model model;
    semantic::semantic_index index;
  };

  struct capabilities
  {
    std::string_view schema;
    bool strict_document_check;
    bool structured_diagnostics;
    bool utf16_positions;
    bool cancellation;
    bool recovery;
    bool document_overlays;
    bool semantic_index;
    bool language_server;
  };

  auto supported_capabilities() -> capabilities;
  auto capabilities_json() -> std::string;

  auto check_document(const source::document_snapshot &document, check_options options = {},
                      diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<check_summary>;
  auto analyze_document(const source::document_snapshot &document, check_options options = {},
                        diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<check_summary>;
  auto index_document(const source::document_snapshot &document,
                      diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<semantic_snapshot>;
}

