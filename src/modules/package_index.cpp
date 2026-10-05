#include "package_index.hpp"
#include "resolver.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace modules
{
  namespace
  {
    auto numeric_version(const std::string_view text) -> std::optional<std::array<std::uint64_t, 3>>
    {
      std::array<std::uint64_t, 3> parts{};
      std::size_t begin = 0;
      for (std::size_t index = 0; index < parts.size(); ++index)
      {
        const auto end = index == 2 ? text.size() : text.find('.', begin);
        if (end == std::string_view::npos || end == begin) return {};
        const auto [last, error] = std::from_chars(text.data() + begin, text.data() + end, parts[index]);
        if (error != std::errc{} || last != text.data() + end) return {};
        begin = end + 1;
      }
      return parts;
    }

    auto split_tabs(const std::string &line) -> std::vector<std::string>
    {
      std::vector<std::string> fields;
      std::size_t begin = 0;
      while (true)
      {
        const auto tab = line.find('\t', begin);
        fields.push_back(line.substr(begin, tab == std::string::npos ? tab : tab - begin));
        if (tab == std::string::npos) return fields;
        begin = tab + 1;
      }
    }
  }

  auto version_satisfies(const std::string_view requirement, const std::string_view version) -> bool
  {
    const bool caret = requirement.starts_with('^');
    const auto required = numeric_version(caret ? requirement.substr(1) : requirement);
    const auto actual = numeric_version(version);
    if (!required || !actual) return false;
    if (!caret) return *actual == *required;
    if (*actual < *required) return false;
    if ((*required)[0] != 0) return (*actual)[0] == (*required)[0];
    if ((*required)[1] != 0)
      return (*actual)[0] == 0 && (*actual)[1] == (*required)[1];
    return (*actual)[0] == 0 && (*actual)[1] == 0 && (*actual)[2] == (*required)[2];
  }

  auto query_package_index(const std::filesystem::path &index_path,
                           const std::string_view compiler_version,
                           const std::string_view name_prefix,
                           const sagan::diagnostics::cancellation_token cancellation)
    -> package_index_result
  {
    package_index_result result;
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    std::ifstream input(index_path, std::ios::binary);
    if (!input)
    { result.message = "Local package index is unavailable"; return result; }
    std::string line;
    if (!std::getline(input, line))
    { result.state = package_index_state::invalid; result.message = "Unsupported package index schema"; return result; }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != package_index_schema)
    { result.state = package_index_state::invalid; result.message = "Unsupported package index schema"; return result; }
    result.state = package_index_state::ready;
    std::set<std::pair<std::string, std::string>> seen;
    std::size_t line_number = 1;
    while (std::getline(input, line))
    {
      if (cancellation.is_cancelled())
      { result.cancelled = true; result.packages.clear(); return result; }
      ++line_number;
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line.front() == '#') continue;
      const auto fields = split_tabs(line);
      if (fields.size() != 5 ||
          !std::regex_match(fields[0], std::regex{"[A-Za-z][A-Za-z0-9_-]*"}) ||
          !numeric_version(fields[1]) ||
          !numeric_version(fields[2].starts_with('^') ? std::string_view(fields[2]).substr(1) :
                                                  std::string_view(fields[2])) ||
          (fields[3] != "installed" && fields[3] != "available") ||
          !seen.emplace(fields[0], fields[1]).second)
      {
        result.state = package_index_state::invalid;
        result.message = "Invalid package index row " + std::to_string(line_number);
        result.packages.clear();
        return result;
      }
      const auto path = std::filesystem::path(fields[4]);
      if ((fields[3] == "available" && fields[4] != "-") ||
          (fields[3] == "installed" &&
           (fields[4] == "-" || path.is_absolute() ||
            path.lexically_normal().string().starts_with(".."))))
      {
        result.state = package_index_state::invalid;
        result.message = "Package index row has an invalid manifest path";
        result.packages.clear();
        return result;
      }
      if (!std::string_view(fields[0]).starts_with(name_prefix)) continue;
      indexed_package package{fields[0], fields[1], fields[2],
          fields[3] == "installed" ? package_install_state::installed : package_install_state::available,
          fields[3] == "installed" ? index_path.parent_path() / path : std::filesystem::path{},
          version_satisfies(fields[2], compiler_version)};
      if (package.install_state == package_install_state::installed)
      {
        if (cancellation.is_cancelled())
        { result.cancelled = true; result.packages.clear(); return result; }
        try
        {
          const auto manifest = load_package(package.manifest_path);
          if (manifest.name != package.name || manifest.version != package.version)
            throw std::runtime_error("Indexed manifest identity does not match");
        }
        catch (const std::exception &error)
        {
          result.state = package_index_state::invalid;
          result.message = "Invalid installed package '" + package.name + "': " + error.what();
          result.packages.clear();
          return result;
        }
      }
      result.packages.push_back(std::move(package));
    }
    if (cancellation.is_cancelled())
    { result.cancelled = true; result.packages.clear(); return result; }
    std::sort(result.packages.begin(), result.packages.end(), [](const auto &left, const auto &right)
    {
      if (left.name != right.name) return left.name < right.name;
      return *numeric_version(left.version) < *numeric_version(right.version);
    });
    return result;
  }

  auto resolve_indexed_dependencies(const package_manifest &project_manifest,
                                    const std::filesystem::path &index_path,
                                    const std::string_view compiler_version,
                                    const std::filesystem::path &lock_path)
    -> dependency_resolution
  {
    dependency_resolution result;
    const auto index = query_package_index(index_path, compiler_version);
    if (index.state != package_index_state::ready)
    {
      result.state = index.state == package_index_state::invalid
                         ? dependency_state::invalid_index : dependency_state::index_unavailable;
      result.message = index.message;
      return result;
    }
    std::map<std::string, std::string> pinned;
    const bool locked = !lock_path.empty();
    if (locked)
    {
      std::ifstream lock(lock_path, std::ios::binary);
      std::string line;
      if (!lock || !std::getline(lock, line))
      { result.state = dependency_state::invalid_lock; result.message = "Package lockfile is missing or empty"; return result; }
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line != package_lock_schema)
      { result.state = dependency_state::invalid_lock; result.message = "Unsupported package lockfile schema"; return result; }
      while (std::getline(lock, line))
      {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 2 ||
            !std::regex_match(fields[0], std::regex{"[A-Za-z][A-Za-z0-9_-]*"}) ||
            !numeric_version(fields[1]) || !pinned.emplace(fields[0], fields[1]).second)
        { result.state = dependency_state::invalid_lock; result.message = "Invalid package lockfile row"; return result; }
      }
    }
    std::map<std::string, indexed_package> selected;
    std::set<std::string> visiting;
    try
    {
      std::function<void(const std::string &, const std::string &)> select;
      select = [&](const std::string &name, const std::string &requirement)
      {
        if (const auto existing = selected.find(name); existing != selected.end())
        {
          if (!version_satisfies(requirement, existing->second.version))
          {
            result.state = dependency_state::conflict;
            throw std::runtime_error("Conflicting requirements for package '" + name + "'");
          }
          return;
        }
        if (!visiting.insert(name).second)
        {
          result.state = dependency_state::conflict;
          throw std::runtime_error("Cyclic package dependency involving '" + name + "'");
        }
        const indexed_package *candidate = nullptr;
        bool named = false, compatible = false, installed = false;
        for (const auto &item : index.packages)
        {
          if (item.name != name) continue;
          named = true;
          if (!version_satisfies(requirement, item.version)) continue;
          if (!item.compiler_compatible) continue;
          compatible = true;
          if (item.install_state != package_install_state::installed) continue;
          installed = true;
          if (locked && (pinned.find(name) == pinned.end() || pinned.at(name) != item.version))
            continue;
          if (!candidate || *numeric_version(candidate->version) < *numeric_version(item.version))
            candidate = &item;
        }
        if (!candidate)
        {
          result.state = !named ? dependency_state::missing :
              !compatible ? dependency_state::incompatible :
              !installed ? dependency_state::unavailable : dependency_state::invalid_lock;
          throw std::runtime_error("No locked, installed, compiler-compatible version of package '" +
                                   name + "' satisfies '" + requirement + "'");
        }
        const auto manifest = load_package(candidate->manifest_path);
        for (const auto &dependency : manifest.dependencies)
          select(dependency.name, dependency.requirement);
        visiting.erase(name);
        selected.emplace(name, *candidate);
      };
      for (const auto &dependency : project_manifest.dependencies)
        select(dependency.name, dependency.requirement);
      if (locked && pinned.size() != selected.size())
      { result.state = dependency_state::invalid_lock; throw std::runtime_error("Lockfile has unused package rows"); }
    }
    catch (const std::exception &error)
    {
      if (result.state == dependency_state::index_unavailable)
        result.state = dependency_state::invalid_index;
      result.message = error.what();
      return result;
    }
    result.state = dependency_state::ready;
    result.lock_text = std::string(package_lock_schema) + '\n';
    for (const auto &[name, package] : selected)
    {
      result.packages.push_back(package);
      result.lock_text += name + '\t' + package.version + '\n';
    }
    return result;
  }

  auto resolve_indexed_dependencies(const std::filesystem::path &project_manifest,
                                    const std::filesystem::path &index_path,
                                    const std::string_view compiler_version,
                                    const std::filesystem::path &lock_path)
    -> dependency_resolution
  {
    try
    {
      return resolve_indexed_dependencies(load_package(project_manifest), index_path,
                                          compiler_version, lock_path);
    }
    catch (const std::exception &error)
    {
      return {dependency_state::invalid_index, error.what(), {}, {}};
    }
  }
}
