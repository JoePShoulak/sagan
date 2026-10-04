#pragma once

#include "../diagnostics/diagnostic.hpp"
#include "../modules/package_index.hpp"
#include "../modules/resolver.hpp"
#include "../source/source.hpp"
#include "../source/provider.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::language_service
{
  inline constexpr std::string_view package_catalog_schema = "sagan-package-catalog-v1";

  auto analyze_manifest_document(const source::document_snapshot &document,
                                 diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<modules::package_manifest>;

  struct manifest_completion_candidate
  {
    std::string label;
    source::text_edit edit;
  };
  auto complete_manifest_document(const source::document_snapshot &document,
                                  source::byte_offset offset,
                                  diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<std::vector<manifest_completion_candidate>>;

  struct manifest_hover_information
  {
    source::source_range selection;
    std::string markdown;
  };
  auto hover_manifest_document(const source::document_snapshot &document,
                               source::byte_offset offset,
                               diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<manifest_hover_information>;

  struct manifest_document_symbol
  {
    std::string name;
    source::source_range range;
    source::source_range selection;
    std::vector<manifest_document_symbol> children;
  };
  auto manifest_document_symbols(const source::document_snapshot &document,
                                 diagnostics::cancellation_token cancellation = {})
    -> diagnostics::analysis_result<std::vector<manifest_document_symbol>>;

  struct catalog_export
  {
    std::string symbol_id;
    std::string public_name;
    std::string local_name;
    std::string kind;
    std::string signature;
    std::string documentation;
    bool deprecated{};
    source::document_uri source_uri;
    source::utf16_position start;
    source::utf16_position end;
  };

  struct catalog_module
  {
    std::string name;
    source::document_uri source_uri;
    std::vector<catalog_export> exports;
  };

  struct catalog_package
  {
    std::string identity;
    std::string name;
    std::string version;
    std::string compiler_requirement;
    bool compiler_compatible{};
    modules::package_install_state install_state{modules::package_install_state::available};
    source::document_uri manifest_uri;
    std::vector<catalog_module> modules;
    std::string error;
  };

  struct package_catalog_result
  {
    modules::package_index_state state{modules::package_index_state::unavailable};
    bool cancelled{};
    std::string message;
    std::vector<catalog_package> packages;
  };

  struct import_module_candidate
  {
    std::string name;
    source::source_range replacement;
  };

  struct import_module_result
  {
    bool applicable{};
    bool cancelled{};
    std::string error;
    std::vector<import_module_candidate> candidates;
  };

  struct import_export_result
  {
    bool applicable{};
    bool cancelled{};
    bool incomplete{};
    std::string error;
    std::string module;
    source::source_range replacement;
    std::vector<catalog_export> candidates;
  };

  struct import_module_target_result
  {
    bool applicable{};
    bool cancelled{};
    std::string error;
    source::document_uri source_uri;
    source::utf16_position start;
    source::utf16_position end;
  };

  struct import_export_target_result
  {
    bool applicable{};
    bool cancelled{};
    std::string error;
    source::source_range selection;
    std::optional<catalog_export> target;
  };

  // Resolves the exported name in a selective import, including an import
  // whose surrounding document is not yet a complete program.
  auto query_import_export_target(const source::document_snapshot &document,
                                  source::byte_offset offset,
                                  const source::source_provider &provider,
                                  diagnostics::cancellation_token cancellation = {},
                                  const modules::package_resolution_options &options = {})
    -> import_export_target_result;

  auto query_import_module_target(const source::document_snapshot &document,
                                  source::byte_offset offset,
                                  const source::source_provider &provider,
                                  diagnostics::cancellation_token cancellation = {},
                                  const modules::package_resolution_options &options = {})
    -> import_module_target_result;

  auto query_import_exports(const source::document_snapshot &document,
                            source::byte_offset offset,
                            const source::source_provider &provider,
                            diagnostics::cancellation_token cancellation = {},
                            const modules::package_resolution_options &options = {}) -> import_export_result;

  // Completion for a partially typed import must not depend on successfully
  // parsing that same import or on a valid semantic index for the document.
  auto query_import_modules(const source::document_snapshot &document,
                            source::byte_offset offset,
                            diagnostics::cancellation_token cancellation = {},
                            const modules::package_resolution_options &options = {}) -> import_module_result;

  // Enumerates only declarations proven by installed source. Available-only
  // index rows cannot provide exports, signatures, or navigation targets.
  auto query_package_catalog(const std::filesystem::path &index_path,
                             std::string_view compiler_version,
                             std::string_view prefix = {},
                             std::size_t limit = 100,
                             diagnostics::cancellation_token cancellation = {})
    -> package_catalog_result;
}
