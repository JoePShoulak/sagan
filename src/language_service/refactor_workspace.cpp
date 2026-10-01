#include "refactor.hpp"

#include "queries.hpp"
#include "../modules/resolver.hpp"
#include "../parser/tokens.hpp"
#include "../parser/unicode.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>

namespace sagan::language_service
{
  namespace
  {
    struct loaded_document
    {
      source::document_snapshot snapshot;
      const semantic::semantic_index *index{};
      syntax::syntax_document syntax;
    };

    auto identifier_token(const loaded_document &document, const source::source_range within,
                          const std::string_view name, const bool last = false)
      -> std::optional<source::source_range>
    {
      std::optional<source::source_range> result;
      for (const auto &token : document.syntax.tokens)
        if ((token.kind == tokens::IDENTIFIER || token.kind == tokens::METHOD_IDENTIFIER) &&
            token.range.begin >= within.bytes.begin && token.range.end <= within.bytes.end &&
            unicode::normalize_nfc(token.source_text) == name)
        {
          result = source::source_range{within.document, token.range};
          if (!last) break;
        }
      return result;
    }

    auto valid_identifier(const std::string_view name) -> bool
    {
      if (name.empty() || unicode::normalize_nfc(name) != name) return false;
      const source::document_snapshot probe(
          {{source::document_id{~std::uint64_t{}}, source::document_uri{"untitled:workspace-rename"}, {}},
           1, "let " + std::string(name) + " = 0\n"});
      const auto parsed = syntax::analyze(probe, {.recover = false});
      if (!parsed.value) return false;
      return std::any_of(parsed.value->tokens.begin(), parsed.value->tokens.end(), [&](const auto &token)
      {
        return (token.kind == tokens::IDENTIFIER || token.kind == tokens::METHOD_IDENTIFIER) &&
               token.source_text == name;
      });
    }

    auto contains(const std::vector<semantic::symbol_id> &values, const semantic::symbol_id &value) -> bool
    {
      return std::find(values.begin(), values.end(), value) != values.end();
    }

    class preview_source final : public source::source_provider
    {
      const source::source_provider &fallback_;
      std::unordered_map<std::string, source::document_snapshot> documents_;

    public:
      preview_source(const source::source_provider &fallback,
                     const std::vector<preview_document> &previews,
                     const std::map<std::uint64_t, loaded_document> &loaded)
          : fallback_(fallback)
      {
        for (const auto &preview : previews)
          for (const auto &[id, document] : loaded)
            if (document.snapshot.identity().uri == preview.uri)
              documents_.emplace(preview.uri.value,
                  source::document_snapshot(document.snapshot.identity(), preview.expected_version,
                                            preview.text));
      }

      auto read(const source::document_uri &uri) const -> source::provider_result<source::document_snapshot> override
      {
        const auto found = documents_.find(uri.value);
        return found == documents_.end() ? fallback_.read(uri) :
                                           source::provider_result<source::document_snapshot>{found->second, {}};
      }

      auto read_path(const std::filesystem::path &path) const
        -> source::provider_result<source::document_snapshot> override
      {
        const auto original = fallback_.read_path(path);
        if (!original) return original;
        const auto found = documents_.find(original.value->identity().uri.value);
        return found == documents_.end() ? original :
                                           source::provider_result<source::document_snapshot>{found->second, {}};
      }

      auto exists_path(const std::filesystem::path &path) const -> bool override
      { return fallback_.exists_path(path); }

      auto canonicalize(const source::document_uri &uri) const
        -> source::provider_result<std::filesystem::path> override
      { return fallback_.canonicalize(uri); }
    };
  }

  auto rename_workspace(const source::document_snapshot &document,
                        const semantic::semantic_index &index,
                        const semantic::workspace_semantic_index &workspace,
                        const source::source_provider &source,
                        const source::byte_offset position, const std::string_view new_name) -> edit_plan
  {
    if (document.version() != index.version() || document.identity().id != index.document().id)
      return {edit_state::stale, "Semantic index belongs to another document version", {}};
    if (!valid_identifier(new_name))
      return {edit_state::invalid, "Proposed name is not a valid binding identifier", {}};

    std::map<std::uint64_t, loaded_document> documents;
    for (const auto &module : workspace.modules())
    {
      const auto loaded = source.read(module.index.document().uri);
      if (!loaded) return {edit_state::stale, "A workspace document could not be loaded", {}};
      auto parsed = syntax::analyze(*loaded.value, {.recover = false});
      if (!parsed.value || !parsed.value->strict_ast)
        return {edit_state::unsupported, "Workspace rename requires complete source documents", {}};
      documents.emplace(module.index.document().id.value,
                        loaded_document{*loaded.value, &module.index, std::move(*parsed.value)});
    }
    const auto current = documents.find(document.identity().id.value);
    if (current == documents.end())
      return {edit_state::unsupported, "Current document is outside the resolved workspace", {}};

    const document_queries queries(document, index, &workspace);
    const auto selected = queries.symbol_at(position);
    const semantic::indexed_symbol *symbol = selected.value ? workspace.find(selected.value->id) : nullptr;
    const semantic::import_link *selected_import = nullptr;
    for (const auto &entry : workspace.imports())
      if (symbol && entry.binding == symbol->id) { selected_import = &entry; break; }

    std::optional<semantic::exported_symbol> exported_storage;
    const semantic::exported_symbol *exported = nullptr;
    semantic::symbol_id target;
    bool public_api = false;
    bool public_member_api = false;
    if (selected_import)
    {
      const auto imported_token = identifier_token(current->second, selected_import->declaration,
                                                    selected_import->imported_name);
      const bool on_imported_name = imported_token && position >= imported_token->bytes.begin &&
                                    position <= imported_token->bytes.end;
      if (!on_imported_name && selected_import->binding_name != selected_import->imported_name)
      {
        if (std::any_of(index.symbols().begin(), index.symbols().end(), [&](const auto &candidate)
            { return candidate.id != selected_import->binding && candidate.name == new_name; }))
          return {edit_state::conflict, "Proposed import alias already exists in this module", {}};
        versioned_document_edits edits{document.identity().uri, document.version(), {}};
        const auto binding_token = identifier_token(current->second, selected_import->declaration,
                                                     selected_import->binding_name, true);
        if (!binding_token) return {edit_state::unsupported, "Import alias token was not found", {}};
        edits.edits.push_back({*binding_token, std::string(new_name)});
        for (const auto &location : index.references_to(selected_import->binding))
          edits.edits.push_back({location, std::string(new_name)});
        return {edit_state::ready, {}, {{std::move(edits)}}};
      }
      if (selected_import->targets.empty())
        return {edit_state::unsupported, "Imported binding has no resolved workspace target", {}};
      target = selected_import->targets.front();
      public_api = true;
    }
    else if (symbol)
    {
      target = symbol->id;
      public_member_api = symbol->visibility == semantic::symbol_visibility::public_access &&
          (symbol->kind == semantic::symbol_kind::field ||
           symbol->kind == semantic::symbol_kind::constant_field ||
           symbol->kind == semantic::symbol_kind::method ||
           symbol->kind == semantic::symbol_kind::enum_case);
      if (public_member_api)
      {
        std::string module_name;
        std::vector<semantic::symbol_id> members;
        bool owner_exported = false;
        for (const auto &module : workspace.modules())
          if (module.index.document().id == symbol->declaration.document)
          {
            module_name = module.name;
            for (const auto &candidate : module.index.symbols())
              if (candidate.owner_type == symbol->owner_type && candidate.name == symbol->name &&
                  candidate.kind == symbol->kind) members.push_back(candidate.id);
            for (const auto &candidate : workspace.exported_symbols())
              if (candidate.module == module.name)
                for (const auto &exported_target : candidate.targets)
                  if (const auto *owner = workspace.find(exported_target);
                      owner && owner->kind == semantic::symbol_kind::type &&
                      owner->name == symbol->owner_type) owner_exported = true;
            break;
          }
        if (!owner_exported)
          return {edit_state::unsupported, "Public member owner is not an exported workspace type", {}};
        exported_storage = semantic::exported_symbol{
            module_name, symbol->name, symbol->name, std::move(members), symbol->declaration};
        exported = &*exported_storage;
        public_api = true;
      }
    }
    else
    {
      for (const auto &candidate : workspace.exported_symbols())
      {
        if (candidate.declaration.document != document.identity().id) continue;
        const auto token = identifier_token(current->second, candidate.declaration, candidate.public_name, true);
        if (token && position >= token->bytes.begin && position <= token->bytes.end)
        {
          if (candidate.targets.empty())
            return {edit_state::unsupported, "Export has no resolved workspace target", {}};
          exported_storage = candidate;
          exported = &*exported_storage;
          target = candidate.targets.front();
          public_api = true;
          break;
        }
      }
    }
    if (target.value.empty()) return {edit_state::unsupported, "No renameable workspace symbol was selected", {}};
    if (!exported)
      for (const auto &candidate : workspace.exported_symbols())
        if (contains(candidate.targets, target))
        {
          exported_storage = candidate;
          exported = &*exported_storage;
          break;
        }
    if (!exported)
      return {edit_state::unsupported, "Selected symbol is not a public workspace export", {}};
    const auto &rename_targets = exported->targets;
    if (rename_targets.empty()) return {edit_state::unsupported, "Export has no resolved identities", {}};
    if (!public_api && exported->declaration.document == document.identity().id &&
        exported->local_name != exported->public_name)
    {
      const auto public_token = identifier_token(current->second, exported->declaration,
                                                  exported->public_name, true);
      public_api = public_token && position >= public_token->bytes.begin &&
                   position <= public_token->bytes.end;
    }
    const auto *target_symbol = workspace.find(target);
    if (!target_symbol) return {edit_state::unsupported, "Export target identity was not found", {}};
    for (const auto &rename_target : rename_targets)
    {
      const auto *candidate = workspace.find(rename_target);
      if (!candidate || !semantic::rename_preserves_binding_convention(candidate->kind, new_name))
        return {edit_state::invalid, "Proposed name violates this declaration's naming convention", {}};
    }
    if (new_name == (public_api ? exported->public_name : target_symbol->name))
      return {edit_state::ready, {}, {}};

    for (const auto &candidate : workspace.exported_symbols())
      if (!public_member_api && candidate.module == exported->module && candidate.public_name == new_name &&
          std::none_of(candidate.targets.begin(), candidate.targets.end(), [&](const auto &id)
          { return contains(rename_targets, id); }))
        return {edit_state::conflict, "Proposed public name is already exported by this module", {}};
    for (const auto &module : workspace.modules())
      if (module.index.document().id == target_symbol->declaration.document)
        for (const auto &candidate : module.index.symbols())
          if (!contains(rename_targets, candidate.id) && candidate.scope_id == target_symbol->scope_id &&
              candidate.name == new_name)
            return {edit_state::conflict, "Proposed name already exists in the declaration scope", {}};

    std::map<std::string, versioned_document_edits> planned;
    std::set<std::tuple<std::uint64_t, source::byte_offset, source::byte_offset>> seen;
    const auto add = [&](const source::source_range range, const std::string_view replacement) -> bool
    {
      const auto found = documents.find(range.document.value);
      if (found == documents.end()) return false;
      const auto key = std::tuple{range.document.value, range.bytes.begin, range.bytes.end};
      if (!seen.insert(key).second) return true;
      auto [group, inserted] = planned.try_emplace(found->second.snapshot.identity().uri.value,
          versioned_document_edits{found->second.snapshot.identity().uri,
                                   found->second.snapshot.version(), {}});
      group->second.edits.push_back({range, std::string(replacement)});
      return true;
    };

    if (!public_api || exported->local_name == exported->public_name)
    {
      for (const auto &rename_target : rename_targets)
      {
        const auto *renamed_symbol = workspace.find(rename_target);
        if (!renamed_symbol) return {edit_state::unsupported, "A grouped export identity was not found", {}};
        const auto owner = documents.find(renamed_symbol->declaration.document.value);
        if (owner == documents.end()) return {edit_state::unsupported, "Declaration document is unavailable", {}};
        const auto declaration = identifier_token(owner->second, renamed_symbol->declaration,
                                                   renamed_symbol->name);
        if (!declaration || !add(*declaration, new_name))
          return {edit_state::unsupported, "Declaration token was not found", {}};
        for (const auto &location : owner->second.index->references_to(rename_target))
        {
          auto token = identifier_token(owner->second, location, renamed_symbol->name);
          if (!token || !add(*token, new_name))
            return {edit_state::unsupported, "A local reference token was not found", {}};
        }
      }
      const auto owner = documents.find(exported->declaration.document.value);
      if (owner == documents.end()) return {edit_state::unsupported, "Export document is unavailable", {}};
      const auto export_local = identifier_token(owner->second, exported->declaration, exported->local_name);
      if (!export_local || !add(*export_local, new_name))
        return {edit_state::unsupported, "Export declaration token was not found", {}};
    }

    if (public_api || exported->local_name == exported->public_name)
    {
      const auto owner = documents.find(exported->declaration.document.value);
      const auto export_public = owner == documents.end() ? std::optional<source::source_range>{} :
          identifier_token(owner->second, exported->declaration, exported->public_name, true);
      if (!export_public || !add(*export_public, new_name))
        return {edit_state::unsupported, "Public export token was not found", {}};
      if (!public_member_api) for (const auto &imported : workspace.imports())
      {
        if (std::none_of(imported.targets.begin(), imported.targets.end(), [&](const auto &id)
            { return contains(rename_targets, id); })) continue;
        const auto importer = documents.find(imported.declaration.document.value);
        if (importer == documents.end()) return {edit_state::unsupported, "Importer document is unavailable", {}};
        const auto imported_name = identifier_token(importer->second, imported.declaration,
                                                     imported.imported_name);
        if (!imported_name || !add(*imported_name, new_name))
          return {edit_state::unsupported, "Imported public-name token was not found", {}};
        if (imported.binding_name == imported.imported_name)
        {
          for (const auto &candidate : importer->second.index->symbols())
            if (candidate.id != imported.binding && candidate.name == new_name)
              return {edit_state::conflict, "Renamed import would collide in an importing module", {}};
          for (const auto &location : importer->second.index->references_to(imported.binding))
          {
            const auto token = identifier_token(importer->second, location, imported.binding_name);
            if (!token || !add(*token, new_name))
              return {edit_state::unsupported, "An imported reference token was not found", {}};
          }
        }
      }
      for (const auto &reference : workspace.external_references())
        if (contains(rename_targets, reference.target))
        {
          const auto owner_document = documents.find(reference.location.document.value);
          const auto token = owner_document == documents.end() ? std::optional<source::source_range>{} :
              identifier_token(owner_document->second, reference.location, exported->public_name, true);
          if (!token || !add(*token, new_name))
            return {edit_state::unsupported, "A namespace-qualified reference token was not found", {}};
        }
    }

    workspace_edit result;
    for (auto &[uri, group] : planned)
    {
      std::sort(group.edits.begin(), group.edits.end(), [](const auto &left, const auto &right)
      { return left.range.bytes.begin < right.range.bytes.begin; });
      result.documents.push_back(std::move(group));
    }
    if (result.documents.empty())
      return {edit_state::unsupported, "No workspace edits were produced", {}};
    std::vector<const source::document_snapshot *> snapshots;
    for (const auto &[id, loaded] : documents) snapshots.push_back(&loaded.snapshot);
    const auto preview = preview_edits(result, snapshots);
    if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
    try
    {
      const preview_source changed(source, preview.documents, documents);
      bool public_identity_verified = !(public_api || exported->local_name == exported->public_name);
      for (const auto &group : result.documents)
      {
        const auto loaded = changed.read(group.uri);
        if (!loaded || !loaded.value->identity().canonical_path)
          return {edit_state::unsupported, "Workspace rename requires canonical module paths", {}};
        const auto graph = modules::resolve(*loaded.value->identity().canonical_path, changed);
        const auto checked = semantic::build_workspace_index(graph, changed);
        if (public_member_api)
        {
          std::size_t rebound = 0;
          for (const auto &checked_module : checked.modules())
            for (const auto &candidate : checked_module.index.symbols())
              if (candidate.owner_type == target_symbol->owner_type && candidate.name == new_name &&
                  candidate.kind == target_symbol->kind) ++rebound;
          if (rebound != 0 && rebound != rename_targets.size())
            return {edit_state::unsupported, "Public member identity set changed after workspace rename", {}};
          public_identity_verified = public_identity_verified || rebound == rename_targets.size();
        }
        else if (public_api || exported->local_name == exported->public_name)
        {
          const auto rebound = checked.exported(exported->module, std::string(new_name));
          if (!rebound.empty() && rebound.size() != rename_targets.size())
            return {edit_state::unsupported, "Export identity set changed after workspace rename", {}};
          public_identity_verified = public_identity_verified || rebound.size() == rename_targets.size();
        }
      }
      if (!public_identity_verified)
        return {edit_state::unsupported, "Renamed public export was not found during workspace reanalysis", {}};
    }
    catch (const std::exception &error)
    {
      return {edit_state::unsupported,
              "Renamed workspace did not pass strict module analysis: " + std::string(error.what()), {}};
    }
    return {edit_state::ready, {}, std::move(result)};
  }
}
