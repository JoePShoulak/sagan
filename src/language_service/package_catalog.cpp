#include "package_catalog.hpp"

#include "documentation.hpp"
#include "../modules/resolver.hpp"
#include "../semantic/workspace_index.hpp"
#include "../semantic/type_checker.hpp"
#include "../source/provider.hpp"
#include "../syntax/syntax.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <stdexcept>

namespace sagan::language_service
{
  namespace
  {
    auto signature_for(const semantic::semantic_index &index,
                       const semantic::symbol_id &id) -> std::string
    {
      for (const auto &call : index.parameters())
      {
        if (call.callable != id) continue;
        std::string text{"("};
        for (std::size_t i = 0; i < call.types.size(); ++i)
        {
          if (i) text += ", ";
          if (i < call.names.size()) text += call.names[i] + ": ";
          text += call.types[i];
        }
        text += ")";
        if (!call.result_type.empty()) text += ": " + call.result_type;
        return text;
      }
      return {};
    }
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
          result.candidates.push_back({name, {document.identity().id, replacement}});
    }
    catch (const std::exception &error)
    {
      if (cancellation.is_cancelled()) result.cancelled = true;
      else result.error = error.what();
    }
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
                                           signature_for(module.index, target), documentation.summary,
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
