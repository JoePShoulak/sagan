#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../diagnostics/diagnostic.hpp"

namespace modules
{
  struct package_manifest;
  inline constexpr std::string_view package_index_schema = "sagan-package-index-v1";
  inline constexpr std::string_view package_lock_schema = "sagan-package-lock-v1";

  enum class package_index_state { ready, unavailable, invalid };
  enum class package_install_state { installed, available };

  struct indexed_package
  {
    std::string name;
    std::string version;
    std::string compiler_requirement;
    package_install_state install_state{package_install_state::available};
    std::filesystem::path manifest_path;
    bool compiler_compatible{};
  };

  struct package_index_result
  {
    package_index_state state{package_index_state::unavailable};
    bool cancelled{};
    std::string message;
    std::vector<indexed_package> packages;
  };

  auto version_satisfies(std::string_view requirement, std::string_view version) -> bool;
  auto query_package_index(const std::filesystem::path &index_path,
                           std::string_view compiler_version,
                           std::string_view name_prefix = {},
                           sagan::diagnostics::cancellation_token cancellation = {})
    -> package_index_result;

  enum class dependency_state
  {
    ready, index_unavailable, invalid_index, missing, incompatible,
    unavailable, conflict, invalid_lock
  };

  struct dependency_resolution
  {
    dependency_state state{dependency_state::index_unavailable};
    std::string message;
    std::vector<indexed_package> packages;
    std::string lock_text;
  };

  // Resolves exact installed versions, recursively and offline. A supplied
  // lockfile is authoritative; an absent lockfile produces deterministic
  // candidate text but never writes project files during analysis.
  auto resolve_indexed_dependencies(const std::filesystem::path &project_manifest,
                                    const std::filesystem::path &index_path,
                                    std::string_view compiler_version,
                                    const std::filesystem::path &lock_path = {})
    -> dependency_resolution;
  // Editor overlays use the same offline resolver without writing the
  // unsaved manifest to disk.
  auto resolve_indexed_dependencies(const package_manifest &project_manifest,
                                    const std::filesystem::path &index_path,
                                    std::string_view compiler_version,
                                    const std::filesystem::path &lock_path = {})
    -> dependency_resolution;
}
