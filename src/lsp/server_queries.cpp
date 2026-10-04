#include "server.hpp"
#include "conversions.hpp"

#include "../language_service/formatter.hpp"
#include "../language_service/language_service.hpp"
#include "../language_service/package_catalog.hpp"
#include "../language_service/queries.hpp"
#include "../language_service/refactor.hpp"
#include "../language_service/tests.hpp"
#include "../modules/resolver.hpp"
#include "../modules/package_index.hpp"
#include "../version.hpp"
#include "../parser/tokens.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>

namespace sagan::lsp
{
  namespace
  {
    using namespace language_service;

    auto symbol_kind(const semantic::symbol_kind kind) -> int
    {
      switch (kind)
      {
        case semantic::symbol_kind::module: case semantic::symbol_kind::imported_namespace: return 2;
        case semantic::symbol_kind::type: case semantic::symbol_kind::builtin_type: return 5;
        case semantic::symbol_kind::type_parameter: return 26;
        case semantic::symbol_kind::dimension: case semantic::symbol_kind::unit: return 23;
        case semantic::symbol_kind::function: return 12;
        case semantic::symbol_kind::method: return 6;
        case semantic::symbol_kind::constructor: case semantic::symbol_kind::enum_constructor: return 9;
        case semantic::symbol_kind::enum_case: return 22;
        case semantic::symbol_kind::field: case semantic::symbol_kind::constant_field: return 7;
        case semantic::symbol_kind::constant: return 14;
        default: return 13;
      }
    }
    auto completion_kind(const semantic::symbol_kind kind) -> int
    {
      switch (kind)
      {
        case semantic::symbol_kind::module: case semantic::symbol_kind::imported_namespace: return 9;
        case semantic::symbol_kind::type: case semantic::symbol_kind::builtin_type: return 7;
        case semantic::symbol_kind::function: return 3;
        case semantic::symbol_kind::method: return 2;
        case semantic::symbol_kind::constructor: case semantic::symbol_kind::enum_constructor: return 4;
        case semantic::symbol_kind::field: case semantic::symbol_kind::constant_field: return 5;
        case semantic::symbol_kind::enum_case: return 20;
        default: return 6;
      }
    }
    auto token_type(const semantic::symbol_kind kind) -> int
    {
      switch (kind)
      {
        case semantic::symbol_kind::module: case semantic::symbol_kind::imported_namespace: return 0;
        case semantic::symbol_kind::type: case semantic::symbol_kind::builtin_type: return 1;
        case semantic::symbol_kind::type_parameter: return 5;
        case semantic::symbol_kind::parameter: return 6;
        case semantic::symbol_kind::field: case semantic::symbol_kind::constant_field: return 8;
        case semantic::symbol_kind::enum_case: case semantic::symbol_kind::enum_constructor: return 9;
        case semantic::symbol_kind::function: case semantic::symbol_kind::constructor: return 10;
        case semantic::symbol_kind::method: return 11;
        default: return 7;
      }
    }
    auto content(const std::vector<std::string> &documentation) -> std::string
    {
      std::string combined;
      for (const auto &line : documentation)
      { if (!combined.empty()) combined += '\n'; combined += line; }
      return combined;
    }
    auto location(const source::document_snapshot &document, const source::source_range range) -> J
    {
      return J::object{{"uri", document.identity().uri.value},
                       {"range", lsp_range(document, range.bytes)}};
    }
    auto symbol_item(const source::document_snapshot &document, const document_symbol &symbol) -> J
    {
      J::array children;
      for (const auto &child : symbol.children) children.push_back(symbol_item(document, child));
      return J::object{{"name", symbol.symbol.name}, {"kind", symbol_kind(symbol.symbol.kind)},
                       {"range", lsp_range(document, symbol.symbol.declaration.bytes)},
                       {"selectionRange", lsp_range(document, symbol.symbol.selection.bytes)},
                       {"children", std::move(children)}};
    }
    auto text_edits(const source::source_provider &source, const workspace_edit &edits) -> J
    {
      J::array changes;
      for (const auto &group : edits.documents)
      {
        const auto loaded = source.read(group.uri);
        if (!loaded || group.expected_version != loaded.value->version())
          throw std::invalid_argument("Source edit requires an unavailable or newer document");
        J::array entries;
        for (const auto &edit : group.edits) entries.push_back(lsp_edit(*loaded.value, edit));
        changes.push_back(J::object{{"textDocument", J::object{{"uri", group.uri.value},
                                                                 {"version", group.expected_version}}},
                                    {"edits", std::move(entries)}});
      }
      return J::object{{"documentChanges", std::move(changes)}};
    }
    auto formatting_edits(const source::document_snapshot &document, const format_result &result) -> J
    {
      if (result.state != edit_state::ready) return J::array{};
      if (result.edits.documents.empty()) return J::array{};
      J::array entries;
      for (const auto &edit : result.edits.documents.front().edits)
        entries.push_back(lsp_edit(document, edit));
      return entries;
    }
  }

  auto server::query(const std::string_view method, const J &params,
                     const diagnostics::cancellation_token cancellation) -> J
  {
    if (method == "sagan/operation") return query_operation(params, cancellation);
    if (method == "sagan/packages/catalog")
    {
      const auto configured = string_field(params, "indexUri");
      std::filesystem::path path;
      if (!configured.empty())
      {
        const auto resolved = documents_->canonicalize(source::document_uri{configured});
        if (!resolved) throw std::invalid_argument(resolved.error->message);
        path = *resolved.value;
      }
      else if (const char *environment = std::getenv("SAGAN_PACKAGE_INDEX")) path = environment;
      std::string version = SAGAN_VERSION;
      if (const auto suffix = version.find_first_of("+-"); suffix != std::string::npos)
        version.erase(suffix);
      const auto prefix = string_field(params, "prefix");
      std::size_t limit = 100;
      if (const auto *requested = params.get("limit"))
      {
        if (!requested->integer() || *requested->integer() < 0 || *requested->integer() > 1000)
          throw std::invalid_argument("Package catalog limit must be between 0 and 1000");
        limit = static_cast<std::size_t>(*requested->integer());
      }
      const auto found = language_service::query_package_catalog(path, version, prefix, limit, cancellation);
      J::array packages;
      for (const auto &entry : found.packages)
      {
        J::array modules;
        for (const auto &module : entry.modules)
        {
          J::array exports;
          for (const auto &symbol : module.exports)
            exports.push_back(J::object{{"symbolId", symbol.symbol_id},
                {"name", symbol.public_name}, {"localName", symbol.local_name},
                {"kind", symbol.kind}, {"signature", symbol.signature},
                {"documentation", symbol.documentation}, {"deprecated", symbol.deprecated},
                {"sourceUri", symbol.source_uri.value},
                {"range", J::object{
                    {"start", J::object{{"line", static_cast<std::int64_t>(symbol.start.line)},
                                         {"character", static_cast<std::int64_t>(symbol.start.character)}}},
                    {"end", J::object{{"line", static_cast<std::int64_t>(symbol.end.line)},
                                       {"character", static_cast<std::int64_t>(symbol.end.character)}}}}}});
          modules.push_back(J::object{{"name", module.name}, {"sourceUri", module.source_uri.value},
                                      {"exports", std::move(exports)}});
        }
        packages.push_back(J::object{{"identity", entry.identity}, {"name", entry.name},
            {"version", entry.version}, {"compilerRequirement", entry.compiler_requirement},
            {"compilerCompatible", entry.compiler_compatible},
            {"installState", entry.install_state == modules::package_install_state::installed
                                 ? "installed" : "available"},
            {"manifestUri", entry.manifest_uri.value.empty() ? J{nullptr} : J{entry.manifest_uri.value}},
            {"modules", std::move(modules)}, {"error", entry.error}});
      }
      const auto state = found.cancelled ? "cancelled" :
          found.state == modules::package_index_state::ready ? "ready" :
          found.state == modules::package_index_state::unavailable ? "unavailable" : "invalid";
      return J::object{{"schema", std::string(language_service::package_catalog_schema)},
                       {"state", state}, {"message", found.message},
                       {"packages", std::move(packages)}};
    }
    if (method == "sagan/packages/query")
    {
      const auto configured = string_field(params, "indexUri");
      std::filesystem::path path;
      if (!configured.empty())
      {
        const auto resolved = documents_->canonicalize(source::document_uri{configured});
        if (!resolved) throw std::invalid_argument(resolved.error->message);
        path = *resolved.value;
      }
      else if (const char *environment = std::getenv("SAGAN_PACKAGE_INDEX")) path = environment;
      std::string version = SAGAN_VERSION;
      if (const auto suffix = version.find_first_of("+-"); suffix != std::string::npos)
        version.erase(suffix);
      const auto prefix = string_field(params, "prefix");
      const auto found = modules::query_package_index(path, version, prefix);
      J::array packages;
      for (const auto &entry : found.packages)
        packages.push_back(J::object{{"name", entry.name}, {"version", entry.version},
            {"compilerRequirement", entry.compiler_requirement},
            {"compilerCompatible", entry.compiler_compatible},
            {"installState", entry.install_state == modules::package_install_state::installed
                                 ? "installed" : "available"},
            {"manifestUri", entry.manifest_path.empty() ? J{nullptr} :
                J{source::identity_from_path(source::document_id{}, entry.manifest_path).uri.value}}});
      const auto state = found.state == modules::package_index_state::ready ? "ready" :
                         found.state == modules::package_index_state::unavailable ? "unavailable" : "invalid";
      return J::object{{"schema", std::string(modules::package_index_schema)},
                       {"state", state}, {"message", found.message},
                       {"packages", std::move(packages)}};
    }
    if (method == "sagan/tests/discover")
    {
      const auto uri = source::document_uri{string_field(field(params, "textDocument"), "uri")};
      const auto loaded = documents_->read(uri);
      if (!loaded) throw std::invalid_argument(loaded.error->message);
      const auto document = *loaded.value;
      if (const auto *version = field(params, "textDocument").get("version"))
        if (!version->integer() || *version->integer() != document.version())
          throw std::invalid_argument("Test discovery document version is stale");
      const auto scope = string_field(params, "scope");
      if (!scope.empty() && scope != "document" && scope != "project")
        throw std::invalid_argument("Test discovery scope must be document or project");
      language_service::test_discovery_result discovered;
      if (scope == "project")
      {
        const auto project_uri = source::document_uri{string_field(params, "projectUri")};
        const auto canonical = documents_->canonicalize(project_uri.value.empty() ? uri : project_uri);
        if (!canonical) throw std::invalid_argument(canonical.error->message);
        discovered = language_service::discover_project_tests(*canonical.value, *documents_, cancellation);
      }
      else discovered = language_service::discover_document_tests(document, "local", {}, cancellation);
      const auto current = documents_->read(uri);
      const bool stale = !current || current.value->version() != document.version() ||
                         current.value->text() != document.text();
      J::array tests;
      if (!stale && discovered.state != diagnostics::result_state::cancelled)
        for (const auto &test : discovered.tests)
        {
          J::array suites;
          for (const auto &suite : test.suites) suites.push_back(suite);
          tests.push_back(J::object{{"id", test.id}, {"name", test.name}, {"suites", std::move(suites)},
              {"package", test.package}, {"module", test.module}, {"uri", test.uri.value},
              {"version", test.version},
              {"range", J::object{{"start", J::object{{"line", static_cast<std::int64_t>(test.start.line)},
                                                      {"character", static_cast<std::int64_t>(test.start.character)}}},
                                  {"end", J::object{{"line", static_cast<std::int64_t>(test.end.line)},
                                                    {"character", static_cast<std::int64_t>(test.end.character)}}}}}});
        }
      const auto state = stale ? "stale" : discovered.state == diagnostics::result_state::cancelled
          ? "cancelled" : discovered.state == diagnostics::result_state::complete ? "complete"
          : discovered.state == diagnostics::result_state::recovered ? "recovered" : "incomplete";
      J::array issues;
      if (!stale && discovered.state != diagnostics::result_state::cancelled)
        for (const auto &issue : discovered.diagnostics)
        {
          J::object item{{"code", issue.code},
                         {"severity", std::string(diagnostics::severity_name(issue.level))},
                         {"phase", std::string(diagnostics::phase_name(issue.owner))},
                         {"message", issue.message}};
          if (issue.primary.document == document.identity().id &&
              issue.primary.bytes.end <= document.text().size())
            item["range"] = lsp_range(document, issue.primary.bytes);
          issues.push_back(std::move(item));
        }
      return J::object{{"schema", std::string(language_service::test_schema_version)},
                       {"state", state}, {"uri", uri.value}, {"version", document.version()},
                       {"tests", std::move(tests)}, {"diagnostics", std::move(issues)}};
    }
    if (method == "sagan/tests/run")
    {
      const auto uri = source::document_uri{string_field(field(params, "textDocument"), "uri")};
      const auto loaded = documents_->read(uri);
      if (!loaded) throw std::invalid_argument(loaded.error->message);
      const auto document = *loaded.value;
      if (const auto *version = field(params, "textDocument").get("version"))
        if (!version->integer() || *version->integer() != document.version())
          throw std::invalid_argument("Test run document version is stale");
      const auto scope = string_field(params, "scope");
      if (!scope.empty() && scope != "document" && scope != "project")
        throw std::invalid_argument("Test run scope must be document or project");
      std::vector<std::string> selected;
      if (const auto *ids = params.get("testIds"))
      {
        if (!ids->elements()) throw std::invalid_argument("testIds must be an array");
        for (const auto &id : *ids->elements())
        {
          if (!id.string()) throw std::invalid_argument("testIds must contain strings");
          selected.emplace_back(*id.string());
        }
      }
      const auto serialize_test = [](const language_service::test_case_result &item) -> J
      {
        J::array suites;
        for (const auto &suite : item.test.suites) suites.push_back(suite);
        return J::object{{"id", item.test.id}, {"name", item.test.name},
            {"uri", item.test.uri.value}, {"package", item.test.package},
            {"module", item.test.module}, {"suites", std::move(suites)},
            {"version", item.test.version},
            {"range", J::object{{"start", J::object{{"line", static_cast<std::int64_t>(item.test.start.line)},
                                                     {"character", static_cast<std::int64_t>(item.test.start.character)}}},
                                {"end", J::object{{"line", static_cast<std::int64_t>(item.test.end.line)},
                                                   {"character", static_cast<std::int64_t>(item.test.end.character)}}}}},
            {"state", std::string(language_service::test_case_state_name(item.state))},
            {"durationMilliseconds", static_cast<std::int64_t>(item.duration_milliseconds)},
            {"exitStatus", item.exit_status ? J{*item.exit_status} : J{nullptr}},
            {"message", item.message}, {"stdout", item.standard_output},
            {"stderr", item.standard_error}, {"outputTruncated", item.output_truncated}};
      };
      J::array pending_events;
      const auto observer = [&](const language_service::test_case_result &item)
      {
        J event = J::object{{"jsonrpc", "2.0"}, {"method", "sagan/testEvent"},
                         {"params", J::object{{"schema", std::string(language_service::test_schema_version)},
                                               {"uri", uri.value}, {"version", document.version()},
                                               {"scope", scope == "project" ? "project" : "document"},
                                               {"test", serialize_test(item)}}}};
        if (scope == "project") pending_events.push_back(std::move(event));
        else notify(event);
      };
      const auto artifact_root = std::filesystem::temp_directory_path() / "sagan-lsp-tests";
      language_service::test_run_result run;
      if (scope == "project")
      {
        const auto project_uri = source::document_uri{string_field(params, "projectUri")};
        const auto canonical = documents_->canonicalize(project_uri.value.empty() ? uri : project_uri);
        if (!canonical) throw std::invalid_argument(canonical.error->message);
        run = language_service::run_project_tests(*canonical.value, *documents_, artifact_root,
                                                   selected, cancellation, observer);
      }
      else run = language_service::run_document_tests(document, artifact_root, selected,
                                                       cancellation, observer);
      const auto current = documents_->read(uri);
      const bool stale = !current || current.value->version() != document.version() ||
                         current.value->text() != document.text();
      if (!stale && run.state != diagnostics::result_state::stale)
        for (const auto &event : pending_events) notify(event);
      J::array cases;
      if (!stale && run.state != diagnostics::result_state::stale)
        for (const auto &item : run.tests) cases.push_back(serialize_test(item));
      J::array issues;
      if (!stale)
        for (const auto &issue : run.diagnostics)
        {
          J::object item{{"code", issue.code},
                         {"severity", std::string(diagnostics::severity_name(issue.level))},
                         {"phase", std::string(diagnostics::phase_name(issue.owner))},
                         {"message", issue.message}};
          if (issue.primary.document == document.identity().id &&
              issue.primary.bytes.end <= document.text().size())
            item["range"] = lsp_range(document, issue.primary.bytes);
          issues.push_back(std::move(item));
        }
      const auto state = stale || run.state == diagnostics::result_state::stale ? "stale" :
          run.state == diagnostics::result_state::cancelled
          ? "cancelled" : run.state == diagnostics::result_state::complete ? "complete" : "incomplete";
      return J::object{{"schema", std::string(language_service::test_schema_version)},
                       {"state", state}, {"uri", uri.value}, {"version", document.version()},
                       {"tests", std::move(cases)}, {"diagnostics", std::move(issues)}};
    }
    if (cancellation.is_cancelled()) throw request_cancelled{};
    if (method == "typeHierarchy/supertypes" || method == "typeHierarchy/subtypes" ||
        method == "callHierarchy/incomingCalls" || method == "callHierarchy/outgoingCalls")
    {
      const auto &context = field(field(params, "item"), "data");
      if (string_field(context, "uri").empty()) return J::array{};
      const auto internal = method == "typeHierarchy/supertypes" ? "sagan/typeSupertypes" :
                            method == "typeHierarchy/subtypes" ? "sagan/typeSubtypes" :
                            method == "callHierarchy/incomingCalls" ? "sagan/incomingCalls" :
                            "sagan/outgoingCalls";
      return query(internal, J::object{{"textDocument", J::object{{"uri", string_field(context, "uri")}}},
                                       {"position", field(context, "position")}}, cancellation);
    }
    if (method == "workspace/symbol")
    {
      const auto query_text = string_field(params, "query");
      J::array entries;
      std::set<std::string> seen;
      const auto collect = [&](const semantic::workspace_semantic_index &workspace)
      {
        for (const auto &symbol : search_workspace_symbols(workspace, query_text))
          for (const auto &module : workspace.modules())
            if (module.index.document().id == symbol.declaration.document)
              if (const auto source = documents_->read(module.index.document().uri))
              {
                const auto key = source.value->identity().uri.value + ':' +
                                 std::to_string(symbol.declaration.bytes.begin) + ':' + symbol.name;
                if (seen.insert(key).second)
                  entries.push_back(J::object{{"name", symbol.name}, {"kind", symbol_kind(symbol.kind)},
                                              {"location", location(*source.value, symbol.declaration)}});
              }
      };
      for (const auto &root : workspace_roots_)
        try
        {
          const auto graph = modules::resolve_package(root, *documents_);
          collect(semantic::build_workspace_index(graph, *documents_));
        }
        catch (const std::exception &) {}
      for (const auto &open : open_documents_)
      {
        const auto loaded = documents_->read(open);
        if (!loaded || !loaded.value->identity().canonical_path) continue;
        try
        {
          const auto graph = modules::resolve(*loaded.value->identity().canonical_path, *documents_);
          collect(semantic::build_workspace_index(graph, *documents_));
        }
        catch (const std::exception &) { continue; }
      }
      return entries;
    }
    const auto uri = source::document_uri{string_field(field(params, "textDocument"), "uri")};
    if (uri.value.empty()) throw std::invalid_argument("Missing document URI");
    const auto loaded = documents_->read(uri);
    if (!loaded) throw std::invalid_argument(loaded.error->message);
    const auto &document = *loaded.value;
    if (document.identity().canonical_path &&
        document.identity().canonical_path->filename() == "sagan.toml")
    {
      if (method == "textDocument/hover")
      {
        const auto result = hover_manifest_document(document,
            offset(document, field(params, "position")), cancellation);
        if (result.state == diagnostics::result_state::cancelled || cancellation.is_cancelled())
          throw request_cancelled{};
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!result.value) return nullptr;
        return J::object{{"contents", J::object{{"kind", "markdown"},
                                                {"value", result.value->markdown}}},
                         {"range", lsp_range(document, result.value->selection.bytes)}};
      }
      if (method == "textDocument/completion")
      {
        const auto result = complete_manifest_document(document,
            offset(document, field(params, "position")), cancellation);
        if (result.state == diagnostics::result_state::cancelled || cancellation.is_cancelled())
          throw request_cancelled{};
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        J::array entries;
        if (result.value)
          for (const auto &candidate : *result.value)
            entries.push_back(J::object{{"label", candidate.label}, {"kind", 14},
                                        {"textEdit", J::object{{"range", lsp_range(document,
                                            candidate.edit.range.bytes)},
                                                               {"newText", candidate.edit.replacement_utf8}}}});
        return entries;
      }
      if (method == "textDocument/formatting" || method == "textDocument/rangeFormatting" ||
          method == "textDocument/onTypeFormatting" || method == "textDocument/documentSymbol" ||
          method == "textDocument/definition" || method == "textDocument/typeDefinition" ||
          method == "textDocument/references") return J::array{};
      return nullptr;
    }
    if (method == "textDocument/formatting")
      return formatting_edits(document, format_document(document));
    if (method == "textDocument/rangeFormatting")
      return formatting_edits(document, format_range(document, byte_range(document, field(params, "range"))));
    if (method == "textDocument/onTypeFormatting")
    {
      const auto trigger = string_field(params, "ch");
      if (trigger.size() != 1) return J::array{};
      return formatting_edits(document, format_on_type(document,
          offset(document, field(params, "position")), trigger[0]));
    }
    if (method == "textDocument/codeAction")
    {
      J::array actions;
      const auto only = field(field(params, "context"), "only").elements();
      const bool organize_requested = !only || std::any_of(only->begin(), only->end(), [](const auto &kind)
      { return kind.string() == "source.organizeImports"; });
      if (organize_requested)
      {
        const auto plan = organize_imports(document);
        if (plan.state == edit_state::ready && !plan.edits.documents.empty())
          actions.push_back(J::object{{"title", "Organize Sagan imports"},
                                      {"kind", "source.organizeImports"},
                                      {"edit", text_edits(*documents_, plan.edits)}});
      }
      const bool fixes_requested = !only || std::any_of(only->begin(), only->end(), [](const auto &kind)
      { return kind.string() == "quickfix"; });
      if (fixes_requested)
      {
        const auto checked = analyze_document(document);
        for (std::size_t index = 0; index < checked.diagnostics.size(); ++index)
          for (std::size_t fix = 0; fix < checked.diagnostics[index].fixes.size(); ++fix)
          {
            const auto plan = plan_diagnostic_fix(document, checked, index, fix);
            if (plan.state == edit_state::ready)
              actions.push_back(J::object{{"title", checked.diagnostics[index].fixes[fix].title},
                                          {"kind", "quickfix"}, {"edit", text_edits(*documents_, plan.edits)}});
          }
      }
      return actions;
    }
    if (method == "textDocument/definition")
    {
      const auto imported = query_import_export_target(document,
          offset(document, field(params, "position")), *documents_, cancellation);
      if (imported.cancelled || cancellation.is_cancelled()) throw request_cancelled{};
      if (imported.applicable)
      {
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!imported.target) return J::array{};
        return J::array{J::object{{"uri", imported.target->source_uri.value},
                                  {"range", J::object{{"start", lsp_position(imported.target->start)},
                                                       {"end", lsp_position(imported.target->end)}}}}};
      }
      const auto target = query_import_module_target(document, offset(document, field(params, "position")),
                                                     *documents_, cancellation);
      if (target.cancelled || cancellation.is_cancelled()) throw request_cancelled{};
      if (target.applicable)
      {
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!target.error.empty() || target.source_uri.value.empty()) return J::array{};
        return J::array{J::object{{"uri", target.source_uri.value},
                                  {"range", J::object{{"start", lsp_position(target.start)},
                                                       {"end", lsp_position(target.end)}}}}};
      }
    }
    if (method == "textDocument/completion")
    {
      const auto imports = query_import_modules(document, offset(document, field(params, "position")),
                                                cancellation);
      if (imports.cancelled || cancellation.is_cancelled()) throw request_cancelled{};
      if (imports.applicable)
      {
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!imports.error.empty()) return J::array{};
        J::array entries;
        for (const auto &candidate : imports.candidates)
          entries.push_back(J::object{{"label", candidate.name}, {"kind", 9},
                                      {"detail", "Sagan module"}, {"filterText", candidate.name},
                                      {"sortText", candidate.name},
                                      {"textEdit", J::object{{"range", lsp_range(document,
                                                                                  candidate.replacement.bytes)},
                                                             {"newText", candidate.name}}}});
        return entries;
      }
      const auto exports = query_import_exports(document, offset(document, field(params, "position")),
                                                *documents_, cancellation);
      if (exports.cancelled || cancellation.is_cancelled()) throw request_cancelled{};
      if (exports.applicable)
      {
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!exports.error.empty()) return J::object{{"isIncomplete", false}, {"items", J::array{}}};
        J::array entries;
        for (const auto &candidate : exports.candidates)
        {
          const auto kind = candidate.kind == "function" ? 3 :
                            candidate.kind == "class" ? 7 :
                            candidate.kind == "enum" ? 13 : 6;
          entries.push_back(J::object{{"label", candidate.public_name}, {"kind", kind},
                                      {"detail", candidate.signature},
                                      {"documentation", candidate.documentation},
                                      {"deprecated", candidate.deprecated},
                                      {"filterText", candidate.public_name},
                                      {"sortText", candidate.public_name},
                                      {"textEdit", J::object{{"range", lsp_range(document,
                                                                                  exports.replacement.bytes)},
                                                             {"newText", candidate.public_name}}},
                                      {"data", J::object{{"symbolId", candidate.symbol_id},
                                                         {"sourceUri", candidate.source_uri.value}}}});
        }
        return J::object{{"isIncomplete", exports.incomplete}, {"items", entries}};
      }
    }
    if (method == "textDocument/hover")
    {
      const auto imported = query_import_export_target(document,
          offset(document, field(params, "position")), *documents_, cancellation);
      if (imported.cancelled || cancellation.is_cancelled()) throw request_cancelled{};
      if (imported.applicable)
      {
        const auto current = documents_->read(uri);
        if (!current || current.value->version() != document.version() ||
            current.value->text() != document.text()) throw request_cancelled{};
        if (!imported.target) return nullptr;
        const auto &target = *imported.target;
        const auto description = target.public_name + target.signature +
            (target.documentation.empty() ? "" : "\n\n" + target.documentation);
        return J::object{{"contents", J::object{{"kind", "markdown"}, {"value", description}}},
                         {"range", lsp_range(document, imported.selection.bytes)}};
      }
    }
    std::optional<semantic::workspace_semantic_index> workspace;
    std::optional<semantic::analysis_identity> identity;
    if (document.identity().canonical_path)
      try
      {
        const auto graph = modules::resolve(*document.identity().canonical_path, *documents_);
        workspace = semantic::build_workspace_index(graph, *documents_);
        const auto module = std::find_if(graph.modules.begin(), graph.modules.end(),
            [&](const auto &candidate)
            { return candidate.path == *document.identity().canonical_path; });
        if (module == graph.modules.end()) workspace.reset();
        else identity = semantic::analysis_identity{graph.package ? graph.package->name : "local", module->name};
      }
      catch (const std::exception &) {}
    const auto analysis = index_document(document, cancellation, identity);
    if (cancellation.is_cancelled()) throw request_cancelled{};
    const semantic::semantic_index *index = analysis.value ? &analysis.value->index : nullptr;
    if (!index && workspace)
      for (const auto &module : workspace->modules())
        if (module.index.document().id == document.identity().id)
        { index = &module.index; break; }
    if (!index) return nullptr;
    document_queries queries(document, *index,
                             workspace ? &*workspace : nullptr,
                             analysis.value ? &analysis.value->model : nullptr,
                             analysis.value && analysis.value->types ? &*analysis.value->types : nullptr);
    const auto selected = [&]() -> source::byte_offset
    { return offset(document, field(params, "position")); };
    const auto related = [&](const source::source_range range) -> J
    {
      if (range.document == document.identity().id) return location(document, range);
      if (workspace)
        for (const auto &module : workspace->modules())
          if (module.index.document().id == range.document)
          {
            const auto source = documents_->read(module.index.document().uri);
            if (source) return location(*source.value, range);
          }
      return nullptr;
    };
    const auto hierarchy_item = [&](const symbol_occurrence &symbol) -> J
    {
      const auto target = related(symbol.declaration);
      if (std::holds_alternative<std::nullptr_t>(target.data)) return nullptr;
      const auto &target_uri = field(target, "uri");
      const auto &target_range = field(target, "range");
      J selection_range = target_range;
      const auto target_document = documents_->read(source::document_uri{std::string(target_uri.string().value_or(""))});
      const semantic::semantic_index *target_index = nullptr;
      if (symbol.declaration.document == document.identity().id) target_index = index;
      else if (workspace)
        for (const auto &module : workspace->modules())
          if (module.index.document().id == symbol.declaration.document)
          { target_index = &module.index; break; }
      if (target_document && target_index)
      {
        const document_queries target_queries(*target_document.value, *target_index, workspace ? &*workspace : nullptr);
        const auto syntax = syntax::analyze(*target_document.value);
        if (syntax.value)
          for (const auto &token : syntax.value->tokens)
          {
            if (token.range.begin < symbol.declaration.bytes.begin ||
                token.range.end > symbol.declaration.bytes.end) continue;
            const auto found = target_queries.symbol_at(token.range.begin);
            if (found.value && found.value->id == symbol.id)
            { selection_range = lsp_range(*target_document.value, found.value->selection.bytes); break; }
          }
      }
      return J::object{{"name", symbol.name}, {"kind", symbol_kind(symbol.kind)},
                       {"uri", target_uri}, {"range", target_range},
                       {"selectionRange", selection_range},
                       {"data", J::object{{"uri", target_uri},
                                          {"position", field(selection_range, "start")}}}};
    };
    const auto locations = [&](const auto &result) -> J
    {
      J::array entries;
      if (result.value)
        for (const auto &range : *result.value)
        {
          auto mapped = related(range);
          if (!std::holds_alternative<std::nullptr_t>(mapped.data)) entries.push_back(std::move(mapped));
        }
      return entries;
    };
    if (method == "textDocument/definition") return locations(queries.definitions(selected()));
    if (method == "textDocument/typeDefinition") return locations(queries.type_definitions(selected()));
    if (method == "textDocument/implementation") return locations(queries.implementations(selected()));
    if (method == "textDocument/references")
      return locations(queries.references(selected(), field(field(params, "context"), "includeDeclaration")
                                                  .boolean().value_or(false)));
    if (method == "textDocument/documentHighlight")
    {
      J::array entries;
      if (const auto result = queries.document_highlights(selected()); result.value)
        for (const auto &range : *result.value)
          entries.push_back(J::object{{"range", lsp_range(document, range.bytes)}, {"kind", 1}});
      return entries;
    }
    if (method == "textDocument/hover")
    {
      const auto result = queries.hover(selected());
      if (!result.value) return nullptr;
      const auto text = result.value->symbol.name +
                        (result.value->type ? ": " + *result.value->type : "") +
                        (result.value->documentation.empty() ? "" : "\n\n" + content(result.value->documentation));
      return J::object{{"contents", J::object{{"kind", "markdown"}, {"value", text}}},
                       {"range", lsp_range(document, result.value->symbol.selection.bytes)}};
    }
    if (method == "textDocument/documentSymbol")
    {
      J::array entries;
      if (const auto result = queries.document_symbols(); result.value)
        for (const auto &symbol : *result.value) entries.push_back(symbol_item(document, symbol));
      return entries;
    }
    if (method == "textDocument/foldingRange")
    {
      J::array entries;
      if (const auto result = queries.folding_regions(); result.value)
        for (const auto &fold : *result.value)
        {
          const auto begin = document.to_utf16(fold.range.bytes.begin);
          const auto end = document.to_utf16(fold.range.bytes.end);
          if (begin && end && begin->line < end->line)
            entries.push_back(J::object{{"startLine", static_cast<std::int64_t>(begin->line)},
                                        {"endLine", static_cast<std::int64_t>(end->line)},
                                        {"kind", fold.kind == folding_kind::comment ? "comment" : "region"}});
        }
      return entries;
    }
    if (method == "textDocument/selectionRange")
    {
      J::array results;
      if (const auto *positions = field(params, "positions").elements())
        for (const auto &point : *positions)
        {
          const auto selection = queries.selection_ranges(offset(document, point));
          J parent = nullptr;
          if (selection.value)
            for (auto range = selection.value->rbegin(); range != selection.value->rend(); ++range)
            {
              J::object item{{"range", lsp_range(document, range->bytes)}};
              if (!std::holds_alternative<std::nullptr_t>(parent.data)) item["parent"] = std::move(parent);
              parent = std::move(item);
            }
          results.push_back(std::move(parent));
        }
      return results;
    }
    if (method == "textDocument/documentLink")
    {
      J::array entries;
      if (const auto result = queries.import_links(); result.value)
        for (const auto &link : *result.value)
          entries.push_back(J::object{{"range", lsp_range(document, link.range.bytes)},
                                      {"target", link.target.value}});
      return entries;
    }
    if (method == "textDocument/inlayHint")
    {
      J::array entries;
      const auto target = byte_range(document, field(params, "range"));
      if (const auto result = queries.inlay_hints(target); result.value)
        for (const auto &hint : *result.value)
          if (const auto point = document.to_utf16(hint.position))
            entries.push_back(J::object{{"position", lsp_position(*point)}, {"label", hint.label},
                                        {"kind", 1}});
      return entries;
    }
    if (method == "textDocument/completion")
    {
      J::array entries;
      if (const auto result = queries.completions(selected()); result.value)
        for (const auto &candidate : *result.value)
        {
          J::object item{{"label", candidate.label}, {"kind", completion_kind(candidate.kind)},
                         {"detail", candidate.detail}, {"filterText", candidate.filter_text},
                         {"sortText", candidate.sort_text},
                         {"textEdit", J::object{{"range", lsp_range(document, candidate.replacement.bytes)},
                                                {"newText", candidate.insertion_text}}}};
          if (!candidate.documentation.empty()) item["documentation"] = content(candidate.documentation);
          if (candidate.deprecated) item["deprecated"] = true;
          if (!candidate.additional_import_edits.empty())
          {
            item["detail"] = candidate.detail + " (from " + candidate.source_module + ")";
            J::array edits;
            for (const auto &edit : candidate.additional_import_edits)
              edits.push_back(lsp_edit(document, edit));
            item["additionalTextEdits"] = std::move(edits);
          }
          entries.push_back(std::move(item));
        }
      return entries;
    }
    if (method == "textDocument/signatureHelp")
    {
      const auto result = queries.signature_help(selected());
      if (!result.value) return nullptr;
      J::array signatures;
      for (const auto &variant : result.value->alternatives)
      {
        J::array parameters;
        for (std::size_t index = 0; index < variant.parameter_names.size(); ++index)
          parameters.push_back(J::object{{"label", variant.parameter_names[index] + ": " +
              (index < variant.parameter_types.size() ? variant.parameter_types[index] : "?")}});
        signatures.push_back(J::object{{"label", variant.label}, {"parameters", std::move(parameters)},
                                       {"documentation", content(variant.documentation)}});
      }
      if (signatures.empty()) signatures.push_back(J::object{{"label", result.value->label}});
      return J::object{{"signatures", std::move(signatures)},
                       {"activeSignature", static_cast<std::int64_t>(result.value->active_signature)},
                       {"activeParameter", static_cast<std::int64_t>(result.value->active_parameter)}};
    }
    if (method == "textDocument/semanticTokens/full")
    {
      struct token { source::utf16_position start; source::utf16_position end; int kind; int modifiers; };
      std::vector<token> tokens;
      if (const auto result = queries.semantic_classifications(); result.value)
        for (const auto &classification : *result.value)
        {
          const auto begin = document.to_utf16(classification.range.bytes.begin);
          const auto end = document.to_utf16(classification.range.bytes.end);
          if (!begin || !end || begin->line != end->line || begin->character == end->character) continue;
          int modifiers = (classification.declaration ? 1 : 0) |
                          (classification.read_only ? 2 : 0) |
                          (classification.deprecated ? 8 : 0) |
                          (classification.builtin ? 16 : 0);
          tokens.push_back({*begin, *end, token_type(classification.kind), modifiers});
        }
      std::sort(tokens.begin(), tokens.end(), [](const auto &left, const auto &right)
      { return std::tie(left.start.line, left.start.character) <
               std::tie(right.start.line, right.start.character); });
      J::array data;
      std::uint32_t prior_line = 0, prior_character = 0;
      for (const auto &item : tokens)
      {
        data.push_back(static_cast<std::int64_t>(item.start.line - prior_line));
        data.push_back(static_cast<std::int64_t>(item.start.line == prior_line
                          ? item.start.character - prior_character : item.start.character));
        data.push_back(static_cast<std::int64_t>(item.end.character - item.start.character));
        data.push_back(item.kind); data.push_back(item.modifiers);
        prior_line = item.start.line; prior_character = item.start.character;
      }
      return J::object{{"data", std::move(data)}};
    }
    if (method == "textDocument/prepareRename")
    {
      const auto symbol = queries.symbol_at(selected());
      std::string placeholder;
      source::byte_range selection;
      if (symbol.value)
      {
        placeholder = symbol.value->name;
        selection = symbol.value->selection.bytes;
      }
      else
      {
        const auto parsed = syntax::analyze(document);
        if (!parsed.value) return nullptr;
        const auto token = std::find_if(parsed.value->tokens.begin(), parsed.value->tokens.end(),
            [&](const auto &candidate)
            { return candidate.range.begin <= selected() && selected() <= candidate.range.end &&
                     (candidate.kind == tokens::IDENTIFIER ||
                      candidate.kind == tokens::METHOD_IDENTIFIER); });
        if (token == parsed.value->tokens.end()) return nullptr;
        placeholder = token->source_text;
        selection = token->range;
      }
      auto result = symbol.value
          ? rename_local(document, *index, selected(), placeholder)
          : edit_plan{edit_state::unsupported, "Workspace export requires workspace proof", {}};
      if (result.state == edit_state::unsupported && workspace)
        result = rename_workspace(document, *index, *workspace, *documents_, selected(), placeholder);
      if (result.state != edit_state::ready) throw request_failed(result.reason);
      return J::object{{"range", lsp_range(document, selection)}, {"placeholder", placeholder}};
    }
    if (method == "textDocument/rename")
    {
      auto result = rename_local(document, *index, selected(), string_field(params, "newName"));
      if (result.state == edit_state::unsupported && workspace)
        result = rename_workspace(document, *index, *workspace, *documents_, selected(),
                                  string_field(params, "newName"));
      if (result.state != edit_state::ready) throw request_failed(result.reason);
      return text_edits(*documents_, result.edits);
    }
    if (method == "textDocument/prepareTypeHierarchy" ||
        method == "textDocument/prepareCallHierarchy")
    {
      const auto symbol = queries.symbol_at(selected());
      if (!symbol.value) return nullptr;
      const auto &selected_symbol = *symbol.value;
      return J::array{J::object{{"name", selected_symbol.name},
                                {"kind", symbol_kind(selected_symbol.kind)},
                                {"uri", uri.value},
                                {"range", lsp_range(document, selected_symbol.declaration.bytes)},
                                {"selectionRange", lsp_range(document, selected_symbol.selection.bytes)},
                                {"data", J::object{{"uri", uri.value},
                                                   {"position", lsp_position(position(field(params, "position")))}}}}};
    }
    if (method == "sagan/typeSupertypes" || method == "sagan/typeSubtypes")
    {
      J::array entries;
      const auto result = queries.type_hierarchy(selected());
      if (result.value)
        for (const auto &symbol : method == "sagan/typeSupertypes"
                                      ? result.value->supertypes : result.value->subtypes)
        {
          auto item = hierarchy_item(symbol);
          if (!std::holds_alternative<std::nullptr_t>(item.data)) entries.push_back(std::move(item));
        }
      return entries;
    }
    if (method == "sagan/incomingCalls" || method == "sagan/outgoingCalls")
    {
      J::array entries;
      const auto result = queries.call_hierarchy(selected());
      if (result.value)
        for (const auto &edge : method == "sagan/incomingCalls"
                                    ? result.value->incoming : result.value->outgoing)
        {
          const auto item = hierarchy_item(method == "sagan/incomingCalls" ? edge.caller : edge.callee);
          if (std::holds_alternative<std::nullptr_t>(item.data)) continue;
          J::array ranges;
          for (const auto &site : edge.call_sites)
          {
            const auto mapped = related(site);
            if (!std::holds_alternative<std::nullptr_t>(mapped.data))
              ranges.push_back(field(mapped, "range"));
          }
          entries.push_back(J::object{{method == "sagan/incomingCalls" ? "from" : "to", item},
                                      {"fromRanges", std::move(ranges)}});
        }
      return entries;
    }
    throw std::out_of_range("Unsupported LSP method: " + std::string(method));
  }
}
