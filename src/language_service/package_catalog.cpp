#include "package_catalog.hpp"

#include "documentation.hpp"
#include "language_service.hpp"
#include "../modules/resolver.hpp"
#include "../semantic/workspace_index.hpp"
#include "../semantic/type_checker.hpp"
#include "../source/provider.hpp"
#include "../syntax/syntax.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"
#include "../version.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>

namespace sagan::language_service
{
  namespace
  {
    auto manifest_error_range(const source::document_snapshot &document,
                              const std::size_t one_based_line) -> source::source_range
    {
      const auto text = document.text();
      if (one_based_line == 0 || one_based_line > document.lines().line_count())
        return {document.identity().id, {0, static_cast<source::byte_offset>(text.size())}};
      const auto start = document.to_byte({static_cast<std::uint32_t>(one_based_line - 1), 0});
      if (!start) return {document.identity().id, {0, static_cast<source::byte_offset>(text.size())}};
      auto end = *start;
      while (end < text.size() && text[end] != '\r' && text[end] != '\n') ++end;
      return {document.identity().id, {*start, end}};
    }
  }

  auto analyze_manifest_document(const source::document_snapshot &document,
                                 const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<modules::package_manifest>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml")
      return {diagnostics::result_state::incomplete, {},
              {{std::string(diagnostics::default_code(diagnostics::phase::project)),
                diagnostics::severity::error, diagnostics::phase::project,
                manifest_error_range(document, 0), "Expected a file named sagan.toml", {}, {}, {}}}, version};
    try
    {
      auto manifest = modules::parse_package_manifest(*document.identity().canonical_path,
                                                       document.text());
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      return {diagnostics::result_state::complete, std::move(manifest), {}, version};
    }
    catch (const modules::manifest_error &error)
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      return {diagnostics::result_state::incomplete, {},
              {{std::string(diagnostics::default_code(diagnostics::phase::project)),
                diagnostics::severity::error, diagnostics::phase::project,
                manifest_error_range(document, error.line()), error.what(), {}, {}, {}}}, version};
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      return {diagnostics::result_state::incomplete, {},
              {{std::string(diagnostics::default_code(diagnostics::phase::project)),
                diagnostics::severity::error, diagnostics::phase::project,
                manifest_error_range(document, 0), error.what(), {}, {}, {}}}, version};
    }
  }

  auto complete_manifest_document(const source::document_snapshot &document,
                                  const source::byte_offset offset,
                                  const diagnostics::cancellation_token cancellation,
                                  const modules::package_resolution_options &options)
    -> diagnostics::analysis_result<std::vector<manifest_completion_candidate>>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml" ||
        !document.to_utf16(offset))
      return {diagnostics::result_state::incomplete, {}, {}, version};
    const auto text = document.text();
    std::string section;
    std::set<std::string> seen_sections;
    std::set<std::string> seen_keys;
    std::size_t line_begin = 0;
    while (line_begin < offset)
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      const auto end = text.find_first_of("\r\n", line_begin);
      if (end == std::string_view::npos || end >= offset) break;
      auto line = text.substr(line_begin, end - line_begin);
      const auto first = line.find_first_not_of(" \t");
      if (first != std::string_view::npos)
      {
        line.remove_prefix(first);
        if (line.starts_with('[') && line.ends_with(']'))
        {
          section = std::string(line.substr(1, line.size() - 2));
          seen_sections.insert(section);
          seen_keys.clear();
        }
        else if (const auto equals = line.find('='); equals != std::string_view::npos)
        {
          auto key = line.substr(0, equals);
          while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.remove_suffix(1);
          seen_keys.insert(std::string(key));
        }
      }
      line_begin = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
    }
    const auto line = text.substr(line_begin, offset - line_begin);
    const auto first = line.find_first_not_of(" \t");
    const auto start = line_begin + (first == std::string_view::npos ? line.size() : first);
    const auto typed = text.substr(start, offset - start);
    const auto line_end = text.find_first_of("\r\n", offset);
    const auto line_limit = line_end == std::string_view::npos ? text.size() : line_end;
    if (section == "application")
    {
      const auto full_line = text.substr(line_begin, line_limit - line_begin);
      const auto equals = full_line.find('=');
      if (equals != std::string_view::npos && line_begin + equals < offset)
      {
        auto key = full_line.substr(0, equals);
        const auto key_begin = key.find_first_not_of(" \t");
        if (key_begin != std::string_view::npos) key.remove_prefix(key_begin);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.remove_suffix(1);
        if (key == "mode")
        {
          if (full_line.find('#') != std::string_view::npos)
            return {diagnostics::result_state::complete, std::vector<manifest_completion_candidate>{}, {}, version};
          auto value_begin = line_begin + equals + 1;
          while (value_begin < line_limit && (text[value_begin] == ' ' || text[value_begin] == '\t'))
            ++value_begin;
          auto value_end = line_limit;
          while (value_end > value_begin && (text[value_end - 1] == ' ' || text[value_end - 1] == '\t'))
            --value_end;
          const auto quoted = value_begin < value_end && text[value_begin] == '"';
          if (quoted)
          {
            ++value_begin;
            if (value_end > value_begin && text[value_end - 1] == '"') --value_end;
            const auto first_closing_quote = text.find('"', value_begin);
            if (first_closing_quote != std::string_view::npos && first_closing_quote < value_end)
              return {diagnostics::result_state::complete, std::vector<manifest_completion_candidate>{}, {}, version};
          }
          if (value_begin <= offset && offset <= value_end)
          {
            const auto prefix = text.substr(value_begin, offset - value_begin);
            std::vector<manifest_completion_candidate> choices;
            for (const auto mode : modules::manifest_application_modes)
              if (mode.starts_with(prefix))
                choices.push_back({std::string(mode),
                    {{document.identity().id, {static_cast<source::byte_offset>(value_begin - (quoted ? 1 : 0)),
                                               static_cast<source::byte_offset>(value_end +
                                                   (quoted && value_end < line_limit ? 1 : 0))}},
                     "\"" + std::string(mode) + "\""}});
            if (cancellation.is_cancelled())
              return {diagnostics::result_state::cancelled, {}, {}, version};
            return {diagnostics::result_state::complete, std::move(choices), {}, version};
          }
        }
      }
    }
    if (typed.find_first_of("=#\"'") != std::string_view::npos ||
        (typed.find_first_of(" \t") != std::string_view::npos && !typed.empty()))
      return {diagnostics::result_state::complete, std::vector<manifest_completion_candidate>{}, {}, version};
    auto replace_end = offset;
    while (replace_end < line_limit &&
           (std::isalnum(static_cast<unsigned char>(text[replace_end])) != 0 ||
            text[replace_end] == '_' || text[replace_end] == '-')) ++replace_end;
    if (typed.starts_with('[') && replace_end < line_limit && text[replace_end] == ']') ++replace_end;
    if (!typed.starts_with('[') && text.substr(offset, line_limit - offset).find('=') != std::string_view::npos)
      return {diagnostics::result_state::complete, std::vector<manifest_completion_candidate>{}, {}, version};
    std::vector<manifest_completion_candidate> values;
    const auto add = [&](const std::string_view label, const std::string_view prefix,
                         const source::byte_offset replace_begin,
                         const std::string &replacement)
    {
      if (!label.starts_with(prefix)) return;
      values.push_back({std::string(label), {{document.identity().id, {replace_begin, replace_end}}, replacement}});
    };
    if (typed.starts_with('['))
    {
      for (const auto name : modules::manifest_sections)
        if (!seen_sections.contains(std::string(name)))
          add(name, typed.substr(1), static_cast<source::byte_offset>(start + 1),
              std::string(name) + "]");
    }
    else if (section == "package" || section == "application")
    {
      const auto keys = section == "package" ? modules::manifest_package_keys.data() :
                                               modules::manifest_application_keys.data();
      const auto count = section == "package" ? modules::manifest_package_keys.size() :
                                                modules::manifest_application_keys.size();
      for (std::size_t index = 0; index < count; ++index)
        if (!seen_keys.contains(std::string(keys[index])))
          add(keys[index], typed, static_cast<source::byte_offset>(start),
              std::string(keys[index]) + (keys[index] == "mode" ? " = \"console\"" : " = \"\""));
    }
    else if (section == "dependencies" && !typed.empty())
    {
      const auto index_path = options.index_path.empty() ? modules::default_package_index_path() :
                              options.index_path;
      std::string compiler_version = options.compiler_version.empty() ? SAGAN_VERSION :
                                     options.compiler_version;
      if (const auto suffix = compiler_version.find_first_of("+-"); suffix != std::string::npos)
        compiler_version.erase(suffix);
      const auto index = modules::query_package_index(index_path, compiler_version);
      if (index.state == modules::package_index_state::ready)
      {
        std::map<std::string, std::string> latest;
        for (const auto &package : index.packages)
        {
          if (cancellation.is_cancelled())
            return {diagnostics::result_state::cancelled, {}, {}, version};
          if (package.install_state != modules::package_install_state::installed ||
              !package.compiler_compatible) continue;
          latest.insert_or_assign(package.name, package.version);
        }
        for (const auto &[name, package_version] : latest)
        {
          if (values.size() == 256) break;
          auto alias = name;
          std::replace(alias.begin(), alias.end(), '-', '_');
          if (seen_keys.contains(alias)) continue;
          const auto declaration = alias == name ? alias + " = \"^" + package_version + "\"" :
              alias + " = { package = \"" + name + "\", version = \"^" + package_version + "\" }";
          add(alias, typed, static_cast<source::byte_offset>(start), declaration);
        }
      }
    }
    else if (section.empty() && typed.empty())
      for (const auto name : modules::manifest_sections)
        if (!seen_sections.contains(std::string(name)))
          add(name, {}, static_cast<source::byte_offset>(start), "[" + std::string(name) + "]");
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    return {diagnostics::result_state::complete, std::move(values), {}, version};
  }

  auto hover_manifest_document(const source::document_snapshot &document,
                               const source::byte_offset offset,
                               const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<manifest_hover_information>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml" ||
        !document.to_utf16(offset))
      return {diagnostics::result_state::incomplete, {}, {}, version};
    const auto text = document.text();
    const auto preceding = offset == 0 ? std::string_view::npos : text.find_last_of("\r\n", offset - 1);
    const auto line_begin = preceding == std::string_view::npos ? 0 : preceding + 1;
    const auto ending = text.find_first_of("\r\n", offset);
    const auto line_end = ending == std::string_view::npos ? text.size() : ending;
    const auto line = text.substr(line_begin, line_end - line_begin);
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string_view::npos || line[first] == '#')
      return {diagnostics::result_state::complete, {}, {}, version};
    std::string_view section;
    std::size_t scan = 0;
    while (scan < line_begin)
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      const auto end = text.find_first_of("\r\n", scan);
      if (end == std::string_view::npos || end >= line_begin) break;
      auto prior = text.substr(scan, end - scan);
      const auto prior_first = prior.find_first_not_of(" \t");
      if (prior_first != std::string_view::npos)
      {
        prior.remove_prefix(prior_first);
        while (!prior.empty() && (prior.back() == ' ' || prior.back() == '\t')) prior.remove_suffix(1);
        if (prior.starts_with('[') && prior.ends_with(']')) section = prior.substr(1, prior.size() - 2);
      }
      scan = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
    }
    const auto selected = [&](const std::size_t begin, const std::size_t end,
                              const std::string_view explanation)
      -> diagnostics::analysis_result<manifest_hover_information>
    {
      if (offset < begin || offset >= end || explanation.empty())
        return {diagnostics::result_state::complete, {}, {}, version};
      return {diagnostics::result_state::complete,
              manifest_hover_information{{document.identity().id,
                                          {static_cast<source::byte_offset>(begin),
                                           static_cast<source::byte_offset>(end)}},
                                         std::string(explanation)}, {}, version};
    };
    if (line[first] == '[')
    {
      const auto closing = line.find(']', first + 1);
      if (closing == std::string_view::npos) return {diagnostics::result_state::complete, {}, {}, version};
      const auto name = line.substr(first + 1, closing - first - 1);
      const auto description = name == "package" ? "Package identity, version, source directory, and entry module." :
          name == "application" ? "How a Sagan project launches as a console or windowed application." :
          name == "dependencies" ? "External packages selected by name or import alias and pinned by sagan.lock." : "";
      return selected(line_begin + first, line_begin + closing + 1, description);
    }
    const auto equals = line.find('=', first);
    if (equals == std::string_view::npos) return {diagnostics::result_state::complete, {}, {}, version};
    auto key_end = equals;
    while (key_end > first && (line[key_end - 1] == ' ' || line[key_end - 1] == '\t')) --key_end;
    const auto key = line.substr(first, key_end - first);
    std::string_view description;
    if (section == "package")
    {
      if (key == "name") description = "The package's import-safe published name.";
      else if (key == "version") description = "The package version in MAJOR.MINOR.PATCH form.";
      else if (key == "source") description = "Source directory inside this package root.";
      else if (key == "entry") description = "Qualified module used as this project's entry point.";
    }
    else if (section == "application" && key == "mode")
      description = "Launch mode: `console` (default) or `windowed`.";
    else if (section == "dependencies" && !key.empty())
      description = "Dependency name or import alias. Use a version string or a `{ package, version }` table; sagan.lock pins the selected version.";
    return selected(line_begin + first, line_begin + key_end, description);
  }

  auto manifest_document_symbols(const source::document_snapshot &document,
                                 const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<std::vector<manifest_document_symbol>>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml")
      return {diagnostics::result_state::incomplete, {}, {}, version};
    const auto text = document.text();
    std::vector<manifest_document_symbol> sections;
    std::size_t line_begin = 0;
    while (line_begin < text.size())
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      const auto ending = text.find_first_of("\r\n", line_begin);
      const auto line_end = ending == std::string_view::npos ? text.size() : ending;
      const auto line = text.substr(line_begin, line_end - line_begin);
      const auto first = line.find_first_not_of(" \t");
      if (first != std::string_view::npos && line[first] != '#')
      {
        const auto absolute_first = line_begin + first;
        if (line[first] == '[')
        {
          const auto closing = line.find(']', first + 1);
          if (closing != std::string_view::npos && closing > first + 1)
          {
            if (!sections.empty())
              sections.back().range.bytes.end = static_cast<source::byte_offset>(line_begin);
            const auto selection = source::source_range{
                document.identity().id,
                {static_cast<source::byte_offset>(absolute_first),
                 static_cast<source::byte_offset>(line_begin + closing + 1)}};
            sections.push_back({std::string(line.substr(first + 1, closing - first - 1)),
                                {document.identity().id,
                                 {static_cast<source::byte_offset>(line_begin),
                                  static_cast<source::byte_offset>(text.size())}},
                                selection, {}});
          }
        }
        else if (!sections.empty())
        {
          const auto equals = line.find('=', first);
          if (equals != std::string_view::npos)
          {
            auto key_end = equals;
            while (key_end > first && (line[key_end - 1] == ' ' || line[key_end - 1] == '\t'))
              --key_end;
            if (key_end > first)
              sections.back().children.push_back({std::string(line.substr(first, key_end - first)),
                  {document.identity().id,
                   {static_cast<source::byte_offset>(line_begin),
                    static_cast<source::byte_offset>(line_end)}},
                  {document.identity().id,
                   {static_cast<source::byte_offset>(absolute_first),
                    static_cast<source::byte_offset>(line_begin + key_end)}}, {}});
          }
        }
      }
      if (ending == std::string_view::npos) break;
      line_begin = ending + (text[ending] == '\r' && ending + 1 < text.size() &&
                             text[ending + 1] == '\n' ? 2 : 1);
    }
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    return {diagnostics::result_state::complete, std::move(sections), {}, version};
  }

  auto query_manifest_entry_target(const source::document_snapshot &document,
                                   const source::byte_offset offset,
                                   const source::source_provider &provider,
                                   const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<manifest_entry_target>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml" ||
        !document.to_utf16(offset))
      return {diagnostics::result_state::incomplete, {}, {}, version};
    const auto manifest = analyze_manifest_document(document, cancellation);
    if (manifest.state == diagnostics::result_state::cancelled)
      return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!manifest.value)
      return {diagnostics::result_state::complete, {}, {}, version};
    const auto text = document.text();
    std::string_view section;
    std::size_t line_begin = 0;
    while (line_begin < text.size())
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      const auto ending = text.find_first_of("\r\n", line_begin);
      const auto line_end = ending == std::string_view::npos ? text.size() : ending;
      const auto line = text.substr(line_begin, line_end - line_begin);
      const auto first = line.find_first_not_of(" \t");
      if (first != std::string_view::npos && line[first] != '#')
      {
        if (line[first] == '[')
        {
          const auto close = line.find(']', first + 1);
          section = close == std::string_view::npos ? std::string_view{} :
                    line.substr(first + 1, close - first - 1);
        }
        else if (section == "package")
        {
          const auto equals = line.find('=', first);
          if (equals != std::string_view::npos)
          {
            auto key_end = equals;
            while (key_end > first && (line[key_end - 1] == ' ' || line[key_end - 1] == '\t')) --key_end;
            if (line.substr(first, key_end - first) == "entry")
            {
              const auto open = line.find('"', equals + 1);
              const auto close = open == std::string_view::npos ? open : line.find('"', open + 1);
              if (close != std::string_view::npos &&
                  line.substr(open + 1, close - open - 1) == manifest.value->entry_module &&
                  line_begin + open + 1 <= offset && offset < line_begin + close)
              {
                std::filesystem::path relative;
                std::string_view remaining = manifest.value->entry_module;
                while (!remaining.empty())
                {
                  const auto dot = remaining.find('.');
                  relative /= remaining.substr(0, dot);
                  if (dot == std::string_view::npos) break;
                  remaining.remove_prefix(dot + 1);
                }
                relative += ".sagan";
                const auto target = provider.read_path(manifest.value->source_root / relative);
                if (!target) return {diagnostics::result_state::complete, {}, {}, version};
                return {diagnostics::result_state::complete,
                        manifest_entry_target{{document.identity().id,
                                               {static_cast<source::byte_offset>(line_begin + open + 1),
                                                static_cast<source::byte_offset>(line_begin + close)}},
                                              target.value->identity().uri, {0, 0}, {0, 0}}, {}, version};
              }
            }
          }
        }
      }
      if (ending == std::string_view::npos) break;
      line_begin = ending + (text[ending] == '\r' && ending + 1 < text.size() &&
                             text[ending + 1] == '\n' ? 2 : 1);
    }
    return {diagnostics::result_state::complete, {}, {}, version};
  }

  auto query_manifest_dependency_target(const source::document_snapshot &document,
                                        const source::byte_offset offset,
                                        const source::source_provider &provider,
                                        const diagnostics::cancellation_token cancellation,
                                        const modules::package_resolution_options &options)
    -> diagnostics::analysis_result<manifest_entry_target>
  {
    const auto version = document.version();
    if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!document.identity().canonical_path ||
        document.identity().canonical_path->filename() != "sagan.toml" ||
        !document.to_utf16(offset))
      return {diagnostics::result_state::incomplete, {}, {}, version};
    const auto manifest = analyze_manifest_document(document, cancellation);
    if (manifest.state == diagnostics::result_state::cancelled)
      return {diagnostics::result_state::cancelled, {}, {}, version};
    if (!manifest.value) return {diagnostics::result_state::complete, {}, {}, version};
    const auto text = document.text();
    std::string_view section;
    std::size_t line_begin = 0;
    while (line_begin < text.size())
    {
      if (cancellation.is_cancelled()) return {diagnostics::result_state::cancelled, {}, {}, version};
      const auto ending = text.find_first_of("\r\n", line_begin);
      const auto line_end = ending == std::string_view::npos ? text.size() : ending;
      const auto line = text.substr(line_begin, line_end - line_begin);
      const auto first = line.find_first_not_of(" \t");
      if (first != std::string_view::npos && line[first] != '#')
      {
        if (line[first] == '[')
        {
          const auto close = line.find(']', first + 1);
          section = close == std::string_view::npos ? std::string_view{} :
                    line.substr(first + 1, close - first - 1);
        }
        else if (section == "dependencies")
        {
          const auto equals = line.find('=', first);
          if (equals != std::string_view::npos)
          {
            auto key_end = equals;
            while (key_end > first && (line[key_end - 1] == ' ' || line[key_end - 1] == '\t')) --key_end;
            const auto alias = line.substr(first, key_end - first);
            const auto dependency = std::find_if(manifest.value->dependencies.begin(),
                manifest.value->dependencies.end(), [&](const auto &item) { return item.alias == alias; });
            if (dependency != manifest.value->dependencies.end())
            {
              std::size_t selected_begin = line_begin + first;
              std::size_t selected_end = line_begin + key_end;
              bool selected = selected_begin <= offset && offset < selected_end;
              const auto package_key = line.find("package", equals + 1);
              if (!selected && package_key != std::string_view::npos)
              {
                const auto name_equals = line.find('=', package_key + 7);
                const auto open = name_equals == std::string_view::npos ? name_equals :
                                  line.find('"', name_equals + 1);
                const auto close = open == std::string_view::npos ? open : line.find('"', open + 1);
                if (close != std::string_view::npos &&
                    line.substr(open + 1, close - open - 1) == dependency->name)
                {
                  selected_begin = line_begin + open + 1;
                  selected_end = line_begin + close;
                  selected = selected_begin <= offset && offset < selected_end;
                }
              }
              if (selected)
              {
                try
                {
                  const auto installed = modules::installed_package_for_dependency(
                      *manifest.value, dependency->name, options);
                  if (cancellation.is_cancelled())
                    return {diagnostics::result_state::cancelled, {}, {}, version};
                  if (!installed) return {diagnostics::result_state::complete, {}, {}, version};
                  const auto target = provider.read_path(installed->manifest_path);
                  if (!target) return {diagnostics::result_state::complete, {}, {}, version};
                  return {diagnostics::result_state::complete,
                          manifest_entry_target{{document.identity().id,
                                                 {static_cast<source::byte_offset>(selected_begin),
                                                  static_cast<source::byte_offset>(selected_end)}},
                                                target.value->identity().uri, {0, 0}, {0, 0}}, {}, version};
                }
                catch (const std::exception &)
                {
                  if (cancellation.is_cancelled())
                    return {diagnostics::result_state::cancelled, {}, {}, version};
                  return {diagnostics::result_state::complete, {}, {}, version};
                }
              }
            }
          }
        }
      }
      if (ending == std::string_view::npos) break;
      line_begin = ending + (text[ending] == '\r' && ending + 1 < text.size() &&
                             text[ending + 1] == '\n' ? 2 : 1);
    }
    return {diagnostics::result_state::complete, {}, {}, version};
  }

  auto query_import_modules(const source::document_snapshot &document,
                            const source::byte_offset offset,
                            const diagnostics::cancellation_token cancellation,
                            const modules::package_resolution_options &options) -> import_module_result
  {
    import_module_result result;
    if (!document.identity().canonical_path || !document.to_utf16(offset)) return result;
    const auto syntax = syntax::analyze(document, {}, cancellation);
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    if (!syntax.value) return result;
    const syntax::lossless_token *keyword = nullptr;
    const auto line = document.to_utf16(offset)->line;
    for (const auto &token : syntax.value->tokens)
    {
      if (token.range.begin >= offset) break;
      if (token.kind == tokens::NEWLINE) { keyword = nullptr; continue; }
      if (token.kind == tokens::KWD_IMPORT || token.kind == tokens::KWD_FROM) keyword = &token;
    }
    if (!keyword || document.to_utf16(keyword->range.begin)->line != line) return result;
    for (const auto &token : syntax.value->tokens)
    {
      if (token.range.begin < keyword->range.end || token.range.begin >= offset) continue;
      if (token.kind != tokens::IDENTIFIER && token.kind != tokens::DOT) return result;
    }
    if (keyword->kind == tokens::KWD_IMPORT)
      for (const auto &token : syntax.value->tokens)
        if (token.kind == tokens::KWD_FROM && token.range.begin >= offset &&
            document.to_utf16(token.range.begin)->line == line) return result;
    auto begin = keyword->range.end;
    const auto text = document.text();
    while (begin < offset && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    source::byte_range replacement{begin, offset};
    for (const auto &token : syntax.value->tokens)
      if (token.kind == tokens::IDENTIFIER && token.range.begin < offset &&
          offset <= token.range.end) replacement.end = token.range.end;
    const auto prefix = unicode::normalize_nfc(text.substr(begin, offset - begin));
    result.applicable = true;
    try
    {
      for (const auto &name : modules::importable_modules(*document.identity().canonical_path,
                                                          cancellation, options))
        if (name.starts_with(prefix))
        {
          if (result.candidates.size() == 256) { result.incomplete = true; break; }
          result.candidates.push_back({name, {document.identity().id, replacement}});
        }
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) result.cancelled = true;
      else result.error = error.what();
    }
    return result;
  }

  auto query_import_exports(const source::document_snapshot &document,
                            const source::byte_offset offset,
                            const source::source_provider &provider,
                            const diagnostics::cancellation_token cancellation,
                            const modules::package_resolution_options &options) -> import_export_result
  {
    import_export_result result;
    if (!document.identity().canonical_path || !document.to_utf16(offset)) return result;
    const auto syntax = syntax::analyze(document, {}, cancellation);
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    if (!syntax.value) return result;
    const auto line = document.to_utf16(offset)->line;
    const syntax::lossless_token *import_token = nullptr;
    const syntax::lossless_token *from_token = nullptr;
    for (const auto &token : syntax.value->tokens)
    {
      const auto position = document.to_utf16(token.range.begin);
      if (!position || position->line != line) continue;
      if (token.kind == tokens::KWD_IMPORT && token.range.end <= offset) import_token = &token;
      if (import_token && token.kind == tokens::KWD_FROM && token.range.begin >= offset)
      { from_token = &token; break; }
    }
    if (!import_token || !from_token) return result;
    auto begin = import_token->range.end;
    const auto text = document.text();
    while (begin < offset && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    source::byte_range replacement{begin, offset};
    for (const auto &token : syntax.value->tokens)
      if (token.kind == tokens::IDENTIFIER && token.range.begin < offset &&
          offset <= token.range.end) replacement.end = token.range.end;
    const auto prefix = unicode::normalize_nfc(text.substr(begin, offset - begin));
    std::string module_name;
    bool began = false;
    for (const auto &token : syntax.value->tokens)
    {
      if (token.range.begin < from_token->range.end) continue;
      if (!document.to_utf16(token.range.begin) ||
          document.to_utf16(token.range.begin)->line != line ||
          token.kind == tokens::KWD_AS || token.kind == tokens::NEWLINE) break;
      if (token.kind != tokens::IDENTIFIER && token.kind != tokens::DOT) return result;
      if (token.kind == tokens::DOT && !began) return result;
      module_name += token.source_text;
      began = true;
    }
    if (module_name.empty() || module_name.back() == '.') return result;
    module_name = unicode::normalize_nfc(module_name);
    result.applicable = true;
    result.module = module_name;
    result.replacement = {document.identity().id, replacement};
    try
    {
      const auto modules = modules::importable_module_sources(
          *document.identity().canonical_path, cancellation, options);
      const auto target = std::find_if(modules.begin(), modules.end(), [&](const auto &candidate)
      { return candidate.name == module_name; });
      if (target == modules.end())
      { result.error = "Module '" + module_name + "' is not installed or importable"; return result; }
      const auto loaded = provider.read_path(target->path);
      if (!loaded)
      { result.error = loaded.error->message; return result; }
      const auto parsed = syntax::analyze(*loaded.value, {}, cancellation);
      if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
      if (!parsed.value || !parsed.value->strict_ast)
      { result.error = "Module '" + module_name + "' has incomplete source"; return result; }
      const auto indexed = index_document(*loaded.value, cancellation);
      if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
      if (!indexed.value)
      { result.error = "Module '" + module_name + "' could not be indexed"; return result; }
      for (const auto &statement : parsed.value->strict_ast->statements)
      {
        const auto *exported = dynamic_cast<const parser::export_declaration *>(statement.get());
        if (!exported) continue;
        const auto public_name = exported->alias.value_or(exported->exported_name);
        if (!public_name.starts_with(prefix)) continue;
        const auto symbol = std::find_if(indexed.value->index.symbols().begin(),
                                         indexed.value->index.symbols().end(), [&](const auto &item)
        { return item.name == exported->exported_name && item.scope_id == 0 &&
                 item.origin == semantic::symbol_origin::source; });
        if (symbol == indexed.value->index.symbols().end()) continue;
        const auto start = loaded.value->to_utf16(symbol->declaration.bytes.begin);
        const auto end = loaded.value->to_utf16(symbol->declaration.bytes.end);
        if (!start || !end) continue;
        const auto documentation = documentation_for(*symbol, module_name);
        result.candidates.push_back({symbol->id.value, public_name, exported->exported_name,
                                     std::string(semantic::name(symbol->kind)),
                                     semantic::callable_signature(indexed.value->index, symbol->id),
                                     documentation.summary, documentation.deprecated,
                                     loaded.value->identity().uri, *start, *end});
      }
      std::sort(result.candidates.begin(), result.candidates.end(), [](const auto &left, const auto &right)
      { return left.public_name < right.public_name; });
      constexpr std::size_t completion_limit = 256;
      if (result.candidates.size() > completion_limit)
      {
        result.incomplete = true;
        result.candidates.resize(completion_limit);
      }
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) result.cancelled = true;
      else result.error = error.what();
    }
    return result;
  }

  auto query_import_module_target(const source::document_snapshot &document,
                                  const source::byte_offset offset,
                                  const source::source_provider &provider,
                                  const diagnostics::cancellation_token cancellation,
                                  const modules::package_resolution_options &options)
    -> import_module_target_result
  {
    import_module_target_result result;
    if (!document.identity().canonical_path || !document.to_utf16(offset)) return result;
    const auto parsed = syntax::analyze(document, {}, cancellation);
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    if (!parsed.value) return result;
    const auto line = document.to_utf16(offset)->line;
    const syntax::lossless_token *import_token = nullptr;
    const syntax::lossless_token *from_token = nullptr;
    for (const auto &token : parsed.value->tokens)
    {
      const auto position = document.to_utf16(token.range.begin);
      if (!position || position->line != line) continue;
      if (token.kind == tokens::KWD_IMPORT) import_token = &token;
      if (import_token && token.kind == tokens::KWD_FROM) from_token = &token;
    }
    if (!import_token) return result;
    const auto begin = from_token ? from_token->range.end : import_token->range.end;
    std::string name;
    std::optional<source::byte_offset> name_begin;
    source::byte_offset name_end = begin;
    for (const auto &token : parsed.value->tokens)
    {
      if (token.range.begin < begin) continue;
      const auto position = document.to_utf16(token.range.begin);
      if (!position || position->line != line || token.kind == tokens::KWD_AS ||
          token.kind == tokens::NEWLINE) break;
      if (token.kind != tokens::IDENTIFIER && token.kind != tokens::DOT) break;
      if (!name_begin) name_begin = token.range.begin;
      name += token.source_text;
      name_end = token.range.end;
    }
    if (!name_begin || name.empty() || name.back() == '.' ||
        offset < *name_begin || offset > name_end) return result;
    result.applicable = true;
    try
    {
      const auto choices = modules::importable_module_sources(
          *document.identity().canonical_path, cancellation, options);
      const auto selected = std::find_if(choices.begin(), choices.end(), [&](const auto &candidate)
      { return candidate.name == unicode::normalize_nfc(name); });
      if (selected == choices.end())
      { result.error = "Module '" + name + "' is not installed or importable"; return result; }
      const auto loaded = provider.read_path(selected->path);
      if (!loaded)
      { result.error = loaded.error->message; return result; }
      result.source_uri = loaded.value->identity().uri;
      result.start = {0, 0};
      result.end = {0, 0};
      const auto target_syntax = syntax::analyze(*loaded.value, {}, cancellation);
      if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
      if (target_syntax.value)
        for (const auto &token : target_syntax.value->tokens)
          if (token.kind == tokens::KWD_MODULE)
          {
            const auto start = loaded.value->to_utf16(token.range.begin);
            const auto end = loaded.value->to_utf16(token.range.end);
            if (start && end) { result.start = *start; result.end = *end; }
            break;
          }
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) result.cancelled = true;
      else result.error = error.what();
    }
    return result;
  }

  auto query_import_export_target(const source::document_snapshot &document,
                                  const source::byte_offset offset,
                                  const source::source_provider &provider,
                                  const diagnostics::cancellation_token cancellation,
                                  const modules::package_resolution_options &options)
    -> import_export_target_result
  {
    import_export_target_result result;
    if (!document.identity().canonical_path || !document.to_utf16(offset)) return result;
    const auto parsed = syntax::analyze(document, {}, cancellation);
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    if (!parsed.value) return result;
    const auto line = document.to_utf16(offset)->line;
    const syntax::lossless_token *import_token = nullptr;
    const syntax::lossless_token *name_token = nullptr;
    bool has_from = false;
    for (const auto &token : parsed.value->tokens)
    {
      const auto position = document.to_utf16(token.range.begin);
      if (!position || position->line != line) continue;
      if (token.kind == tokens::KWD_IMPORT)
      { import_token = &token; name_token = nullptr; has_from = false; continue; }
      if (!import_token) continue;
      if (!name_token && token.kind == tokens::IDENTIFIER) name_token = &token;
      if (token.kind == tokens::KWD_FROM) has_from = true;
    }
    if (!import_token || !name_token || !has_from ||
        offset < name_token->range.begin || offset > name_token->range.end) return result;
    result.applicable = true;
    result.selection = {document.identity().id, name_token->range};
    const auto exports = query_import_exports(document, name_token->range.end, provider,
                                              cancellation, options);
    if (exports.cancelled || cancellation.is_cancelled())
    { result.cancelled = true; return result; }
    result.error = exports.error;
    if (!exports.applicable || !result.error.empty()) return result;
    const auto name = unicode::normalize_nfc(name_token->source_text);
    const auto target = std::find_if(exports.candidates.begin(), exports.candidates.end(),
        [&](const auto &candidate) { return candidate.public_name == name; });
    if (target != exports.candidates.end()) result.target = *target;
    return result;
  }

  auto query_package_catalog(const std::filesystem::path &index_path,
                             const std::string_view compiler_version,
                             const std::string_view prefix,
                             const std::size_t limit,
                             const diagnostics::cancellation_token cancellation)
    -> package_catalog_result
  {
    package_catalog_result result;
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
    const auto index = modules::query_package_index(index_path, compiler_version, prefix);
    result.state = index.state;
    result.message = index.message;
    if (index.state != modules::package_index_state::ready) return result;
    const source::disk_source_provider source;
    for (const auto &entry : index.packages)
    {
      if (cancellation.is_cancelled())
      { result.packages.clear(); result.cancelled = true; return result; }
      if (result.packages.size() >= limit) break;
      catalog_package package{entry.name + "@" + entry.version, entry.name, entry.version,
                              entry.compiler_requirement, entry.compiler_compatible,
                              entry.install_state,
                              entry.manifest_path.empty() ? source::document_uri{} :
                                  source::identity_from_path({}, entry.manifest_path).uri,
                              {}, {}};
      if (entry.install_state == modules::package_install_state::installed)
      {
        try
        {
          const auto graph = modules::resolve_package(entry.manifest_path, source, cancellation);
          if (cancellation.is_cancelled())
          { result.packages.clear(); result.cancelled = true; return result; }
          // Do not publish an apparently usable API from source that the
          // compiler itself cannot link and type-check.
          const auto linked = modules::link_package(entry.manifest_path, source, cancellation);
          static_cast<void>(semantic::check_types(linked));
          const auto workspace = semantic::build_workspace_index(graph, source);
          const auto exports = workspace.exported_symbols();
          for (const auto &module : workspace.modules())
          {
            if (cancellation.is_cancelled())
            { result.packages.clear(); result.cancelled = true; return result; }
            const auto document = source.read(module.index.document().uri);
            if (!document) throw std::runtime_error("Indexed module source is missing");
            catalog_module catalog{module.name, module.index.document().uri, {}};
            for (const auto &exported : exports)
            {
              if (exported.module != module.name) continue;
              for (const auto &target : exported.targets)
              {
                const auto *symbol = module.index.find(target);
                if (!symbol) continue;
                const auto start = document.value->to_utf16(symbol->declaration.bytes.begin);
                const auto end = document.value->to_utf16(symbol->declaration.bytes.end);
                if (!start || !end) continue;
                const auto documentation = documentation_for(*symbol, module.name);
                catalog.exports.push_back({symbol->id.value, exported.public_name,
                                           exported.local_name, std::string(semantic::name(symbol->kind)),
                                           semantic::callable_signature(module.index, target), documentation.summary,
                                           documentation.deprecated, module.index.document().uri,
                                           *start, *end});
              }
            }
            std::sort(catalog.exports.begin(), catalog.exports.end(), [](const auto &left, const auto &right)
            {
              if (left.public_name != right.public_name) return left.public_name < right.public_name;
              return left.symbol_id < right.symbol_id;
            });
            package.modules.push_back(std::move(catalog));
          }
          std::sort(package.modules.begin(), package.modules.end(), [](const auto &left, const auto &right)
          { return left.name < right.name; });
        }
        catch (const std::exception &error)
        { package.error = error.what(); package.modules.clear(); }
      }
      result.packages.push_back(std::move(package));
    }
    if (cancellation.is_cancelled()) { result.packages.clear(); result.cancelled = true; }
    return result;
  }
}
