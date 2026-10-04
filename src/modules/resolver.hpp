#pragma once

#include <filesystem>
#include <array>
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../parser/ast_node.hpp"
#include "../diagnostics/diagnostic.hpp"
#include "../source/provider.hpp"

namespace modules
{
  inline constexpr std::array<std::string_view, 3> manifest_sections{
      "package", "application", "dependencies"};
  inline constexpr std::array<std::string_view, 4> manifest_package_keys{
      "name", "version", "source", "entry"};
  inline constexpr std::array<std::string_view, 1> manifest_application_keys{"mode"};

  class manifest_error : public std::runtime_error
  {
    std::size_t line_{};

  public:
    manifest_error(std::string message, std::size_t line)
        : std::runtime_error(std::move(message)), line_(line) {}
    auto line() const -> std::size_t { return line_; }
  };

  enum class application_mode
  {
    console,
    windowed,
  };

  struct package_dependency
  {
    std::string alias;
    std::string name;
    std::string requirement;
  };

  struct package_manifest
  {
    std::string name;
    std::string version;
    std::filesystem::path manifest_path;
    std::filesystem::path package_root;
    std::filesystem::path source_root;
    std::string entry_module;
    application_mode mode{application_mode::console};
    std::vector<package_dependency> dependencies;
  };

  struct package_resolution_options
  {
    std::filesystem::path index_path;
    std::string compiler_version;
    std::filesystem::path lock_path;
  };

  auto application_mode_name(application_mode mode) -> std::string_view;

  struct export_symbol
  {
    std::string local_name;
    std::string public_name;
    parser::span declaration;
  };

  struct import_edge
  {
    std::string module_name;
    std::string imported_name;
    std::string binding_name;
    bool whole_module;
    parser::span declaration;
  };

  struct module_info
  {
    std::string name;
    std::filesystem::path path;
    std::vector<export_symbol> exports;
    std::vector<import_edge> imports;
  };

  struct module_graph
  {
    std::filesystem::path source_root;
    std::filesystem::path entry_path;
    std::optional<package_manifest> package;
    std::vector<module_info> modules;

    auto print(std::ostream &stream) const -> void;
  };

  auto resolve(const std::filesystem::path &entry_path) -> module_graph;
  auto resolve(const std::filesystem::path &entry_path, const sagan::source::source_provider &source,
               sagan::diagnostics::cancellation_token cancellation = {}) -> module_graph;
  auto link(const std::filesystem::path &entry_path) -> parser::program;
  auto link(const std::filesystem::path &entry_path, const sagan::source::source_provider &source,
            sagan::diagnostics::cancellation_token cancellation = {}) -> parser::program;
  auto load_package(const std::filesystem::path &package_path) -> package_manifest;
  // Validate an editor overlay with the same parser used for on-disk packages.
  auto parse_package_manifest(const std::filesystem::path &manifest_path,
                              std::string_view text) -> package_manifest;
  auto discover_package(const std::filesystem::path &entry_path) -> std::optional<package_manifest>;
  struct importable_module_source
  {
    std::string name;
    std::filesystem::path path;
    bool external{};
  };

  auto importable_module_sources(const std::filesystem::path &entry_path,
                                 sagan::diagnostics::cancellation_token cancellation = {},
                                 const package_resolution_options &options = {})
    -> std::vector<importable_module_source>;
  // Lists import spellings from a package's source tree and its locked,
  // installed dependencies. This does not require the active editor buffer to
  // parse, so it remains usable while an import statement is incomplete.
  auto importable_modules(const std::filesystem::path &entry_path,
                          sagan::diagnostics::cancellation_token cancellation = {},
                          const package_resolution_options &options = {}) -> std::vector<std::string>;
  auto resolve_package(const std::filesystem::path &package_path) -> module_graph;
  auto resolve_package(const std::filesystem::path &package_path,
                       const sagan::source::source_provider &source,
                       sagan::diagnostics::cancellation_token cancellation = {}) -> module_graph;
  auto resolve_package(const std::filesystem::path &package_path,
                       const sagan::source::source_provider &source,
                       sagan::diagnostics::cancellation_token cancellation,
                       const package_resolution_options &options) -> module_graph;
  auto link_package(const std::filesystem::path &package_path) -> parser::program;
  auto link_package(const std::filesystem::path &package_path,
                    const sagan::source::source_provider &source,
                    sagan::diagnostics::cancellation_token cancellation = {}) -> parser::program;
  auto link_package(const std::filesystem::path &package_path,
                    const sagan::source::source_provider &source,
                    sagan::diagnostics::cancellation_token cancellation,
                    const package_resolution_options &options) -> parser::program;
}
