#include "../src/modules/package_index.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/language_service/package_catalog.hpp"
#include "../src/language_service/queries.hpp"
#include "../src/language_service/refactor.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/semantic/type_checker.hpp"
#include "../src/semantic/workspace_index.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
  auto require(const bool value, const char *message) -> void
  {
    if (!value) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  const auto root = "tests/fixtures/catalog/consumer/sagan.toml";
  const auto index = "tests/fixtures/catalog/index.tsv";
  const auto lock = "tests/fixtures/catalog/consumer/sagan.lock";
  const auto selected = modules::resolve_indexed_dependencies(root, index, "2.1.0", lock);
  require(selected.state == modules::dependency_state::ready && selected.packages.size() == 1,
          "Locked installed dependency did not resolve");
  require(selected.packages[0].name == "orbit-tools" && selected.packages[0].version == "0.1.0" &&
              selected.lock_text == "sagan-package-lock-v1\norbit-tools\t0.1.0\n",
          "Lockfile selection is not deterministic");
  const auto candidate = modules::resolve_indexed_dependencies(root, index, "2.1.0");
  require(candidate.state == modules::dependency_state::ready &&
              candidate.lock_text == selected.lock_text,
          "Unlocked candidate selection differs from the pinned version");
  const auto aliased_manifest = modules::load_package("tests/fixtures/catalog/consumer-alias");
  require(aliased_manifest.dependencies.size() == 1 &&
              aliased_manifest.dependencies.front().alias == "orbit_tools" &&
              aliased_manifest.dependencies.front().name == "orbit-tools" &&
              aliased_manifest.dependencies.front().requirement == "^0.1.0",
          "Dependency alias table did not preserve import spelling and package identity");
  const auto reversed_manifest = modules::load_package("tests/fixtures/catalog/consumer-reversed");
  require(reversed_manifest.dependencies.size() == 1 &&
              reversed_manifest.dependencies.front().name == "orbit-tools",
          "TOML inline-table field order changed dependency meaning");
  const auto aliased = modules::resolve_indexed_dependencies(
      "tests/fixtures/catalog/consumer-alias/sagan.toml", index, "2.1.0");
  require(aliased.state == modules::dependency_state::ready &&
              aliased.packages.size() == 1 &&
              aliased.packages.front().name == "orbit-tools",
          "Aliased dependency did not select its actual package identity");
  const sagan::source::disk_source_provider source;
  const auto manifest_document = source.read_path("tests/fixtures/catalog/consumer-alias/sagan.toml");
  require(manifest_document.value.has_value(), "Manifest fixture was not readable");
  const auto manifest_check = sagan::language_service::analyze_manifest_document(*manifest_document.value);
  require(manifest_check.state == sagan::diagnostics::result_state::complete &&
              manifest_check.value && manifest_check.value->dependencies.size() == 1,
          "In-memory manifest analysis disagreed with disk resolution");
  auto spaced_manifest_text = std::string(manifest_document.value->text());
  const auto name_key = spaced_manifest_text.find("name = \"consumer-alias\"");
  require(name_key != std::string::npos, "Manifest fixture has no package name");
  spaced_manifest_text.replace(name_key, std::string_view{"name = \"consumer-alias\""}.size(),
                               "  name  =  \"consumer-alias\"  ");
  const sagan::source::document_snapshot spaced_manifest(manifest_document.value->identity(), 24,
                                                           spaced_manifest_text);
  const auto manifest_format = sagan::language_service::format_manifest_document(spaced_manifest);
  require(manifest_format.state == sagan::language_service::edit_state::ready &&
              manifest_format.edits.documents.size() == 1 &&
              manifest_format.edits.documents.front().expected_version == 24 &&
              manifest_format.edits.documents.front().edits.size() == 1 &&
              manifest_format.edits.documents.front().edits.front().replacement_utf8 ==
                  "name = \"consumer-alias\"",
          "Manifest formatter did not return a versioned, conservative edit");
  const auto &name_edit = manifest_format.edits.documents.front().edits.front();
  const auto selected_manifest_range = sagan::language_service::format_manifest_range(
      spaced_manifest, name_edit.range.bytes);
  require(selected_manifest_range.state == sagan::language_service::edit_state::ready &&
              selected_manifest_range.edits.documents.size() == 1 &&
              selected_manifest_range.edits.documents.front().edits.size() == 1 &&
              selected_manifest_range.edits.documents.front().expected_version == 24,
          "Manifest range formatting omitted the selected proven-valid line");
  const auto excluded_manifest_range = sagan::language_service::format_manifest_range(
      spaced_manifest, {0, name_edit.range.bytes.begin});
  require(excluded_manifest_range.state == sagan::language_service::edit_state::ready &&
              excluded_manifest_range.edits.documents.empty(),
          "Manifest range formatting edited outside the selected source range");
  require(sagan::language_service::format_manifest_range(
              spaced_manifest, {name_edit.range.bytes.end, name_edit.range.bytes.begin}).state ==
              sagan::language_service::edit_state::invalid,
          "Manifest range formatting accepted a reversed source range");
  auto crlf_manifest_text = spaced_manifest_text;
  for (std::size_t newline = crlf_manifest_text.find('\n'); newline != std::string::npos;
       newline = crlf_manifest_text.find('\n', newline + 2))
    crlf_manifest_text.insert(newline, "\r");
  const sagan::source::document_snapshot crlf_format_manifest(manifest_document.value->identity(), 26,
                                                               crlf_manifest_text);
  const auto crlf_name = crlf_manifest_text.find("  name  =");
  const auto crlf_end = crlf_manifest_text.find("\r\n", crlf_name);
  const auto crlf_range = sagan::language_service::format_manifest_range(
      crlf_format_manifest, {static_cast<sagan::source::byte_offset>(crlf_name),
                      static_cast<sagan::source::byte_offset>(crlf_end)});
  require(crlf_range.state == sagan::language_service::edit_state::ready &&
              crlf_range.edits.documents.size() == 1 &&
              crlf_range.edits.documents.front().edits.size() == 1 &&
              crlf_range.edits.documents.front().edits.front().replacement_utf8 ==
                  "name = \"consumer-alias\"",
          "Manifest range formatting did not preserve a CRLF document's line boundaries");
  spaced_manifest_text.replace(name_edit.range.bytes.begin,
                               name_edit.range.bytes.end - name_edit.range.bytes.begin,
                               name_edit.replacement_utf8);
  const sagan::source::document_snapshot formatted_manifest(manifest_document.value->identity(), 25,
                                                              spaced_manifest_text);
  require(sagan::language_service::format_manifest_document(formatted_manifest).edits.documents.empty(),
          "Manifest formatter was not idempotent");
  const auto entry_offset = manifest_document.value->text().find("entry = \"main\"");
  require(entry_offset != std::string_view::npos, "Manifest fixture has no entry key");
  const auto entry_target = sagan::language_service::query_manifest_entry_target(
      *manifest_document.value, static_cast<sagan::source::byte_offset>(entry_offset + 10), source);
  require(entry_target.value && entry_target.value->uri ==
              sagan::source::identity_from_path({}, "tests/fixtures/catalog/consumer-alias/src/main.sagan").uri,
          "Manifest entry value did not navigate to its source module");
  const auto no_key_target = sagan::language_service::query_manifest_entry_target(
      *manifest_document.value, static_cast<sagan::source::byte_offset>(entry_offset + 2), source);
  require(!no_key_target.value, "Manifest key was mistaken for its module value");
  auto changed_entry = std::string(manifest_document.value->text());
  changed_entry.replace(entry_offset, std::string_view{"entry = \"main\""}.size(),
                        "entry = \"orbit_tools\"");
  const sagan::source::document_snapshot changed_manifest(manifest_document.value->identity(), 21,
                                                            changed_entry);
  const auto changed_target = sagan::language_service::query_manifest_entry_target(
      changed_manifest, static_cast<sagan::source::byte_offset>(changed_entry.find("orbit_tools") + 2), source);
  require(changed_target.value && changed_target.value->uri ==
              sagan::source::identity_from_path({}, "tests/fixtures/catalog/consumer-alias/src/orbit_tools.sagan").uri,
          "Unsaved manifest entry edit did not navigate using the overlay");
  auto crlf_entry = std::string(manifest_document.value->text());
  for (std::size_t newline = 0; (newline = crlf_entry.find('\n', newline)) != std::string::npos; newline += 2)
    crlf_entry.replace(newline, 1, "\r\n");
  const sagan::source::document_snapshot crlf_manifest(manifest_document.value->identity(), 22, crlf_entry);
  const auto crlf_target = sagan::language_service::query_manifest_entry_target(
      crlf_manifest, static_cast<sagan::source::byte_offset>(crlf_entry.find("entry = \"main\"") + 10), source);
  require(crlf_target.value && crlf_target.value->uri == entry_target.value->uri,
          "CRLF manifest entry navigation lost its target");
  const auto dependency_alias = manifest_document.value->text().find("orbit_tools =");
  const auto dependency_name = manifest_document.value->text().find("orbit-tools");
  require(dependency_alias != std::string_view::npos && dependency_name != std::string_view::npos,
          "Manifest fixture has no aliased dependency");
  const modules::package_resolution_options manifest_options{
      "tests/fixtures/catalog/current-index.tsv", "2.1.0",
      "tests/fixtures/catalog/consumer-alias/sagan.lock"};
  const auto dependency_target = sagan::language_service::query_manifest_dependency_target(
      *manifest_document.value, static_cast<sagan::source::byte_offset>(dependency_alias + 2),
      source, {}, manifest_options);
  const auto package_name_target = sagan::language_service::query_manifest_dependency_target(
      *manifest_document.value, static_cast<sagan::source::byte_offset>(dependency_name + 2),
      source, {}, manifest_options);
  const auto installed_manifest_uri = sagan::source::identity_from_path(
      {}, "tests/fixtures/catalog/orbit-tools/sagan.toml").uri;
  require(dependency_target.value && dependency_target.value->uri == installed_manifest_uri &&
              package_name_target.value && package_name_target.value->uri == installed_manifest_uri,
          "Locked dependency alias and package name did not navigate to installed manifest");
  auto renamed_dependency = std::string(manifest_document.value->text());
  renamed_dependency.replace(dependency_alias, std::string_view{"orbit_tools"}.size(), "orbit_library");
  const sagan::source::document_snapshot renamed_manifest(manifest_document.value->identity(), 23,
                                                            renamed_dependency);
  const auto renamed_target = sagan::language_service::query_manifest_dependency_target(
      renamed_manifest, static_cast<sagan::source::byte_offset>(dependency_alias + 2),
      source, {}, manifest_options);
  require(renamed_target.value && renamed_target.value->uri == installed_manifest_uri,
          "Unsaved dependency alias edit did not retain locked package navigation");
  auto invalid_manifest_text = std::string(manifest_document.value->text());
  const auto invalid_line = invalid_manifest_text.find("[dependencies]");
  require(invalid_line != std::string::npos, "Manifest fixture has no dependency section");
  invalid_manifest_text.insert(invalid_line, "mystery = \"value\"\n");
  const sagan::source::document_snapshot invalid_manifest(manifest_document.value->identity(), 7,
                                                            invalid_manifest_text);
  const auto invalid_check = sagan::language_service::analyze_manifest_document(invalid_manifest);
  require(invalid_check.state == sagan::diagnostics::result_state::incomplete &&
              invalid_check.diagnostics.size() == 1 &&
              invalid_check.diagnostics.front().primary.bytes.begin == invalid_line &&
              invalid_check.diagnostics.front().message.find("mystery") != std::string::npos,
          "Unsaved manifest key error lost its precise source line");
  require(sagan::language_service::format_manifest_document(invalid_manifest).state ==
              sagan::language_service::edit_state::unsupported,
          "Invalid manifest must not receive speculative formatting edits");
  const sagan::source::document_snapshot duplicate_manifest(manifest_document.value->identity(), 10,
                                                              "[package]\r\nname = \"demo\"\r\nname = \"again\"\r\n");
  const auto duplicate_check = sagan::language_service::analyze_manifest_document(duplicate_manifest);
  const auto duplicate_offset = duplicate_manifest.text().find("name", duplicate_manifest.text().find("name") + 1);
  require(duplicate_check.state == sagan::diagnostics::result_state::incomplete &&
              duplicate_check.diagnostics.size() == 1 &&
              duplicate_check.diagnostics.front().primary.bytes.begin == duplicate_offset &&
              duplicate_check.diagnostics.front().message.find("Duplicate") != std::string::npos,
          "Duplicate CRLF manifest key lost its source location");
  sagan::diagnostics::cancellation_source cancelled_manifest;
  cancelled_manifest.cancel();
  require(sagan::language_service::query_manifest_entry_target(
              *manifest_document.value, static_cast<sagan::source::byte_offset>(entry_offset + 10),
              source, cancelled_manifest.token()).state == sagan::diagnostics::result_state::cancelled,
          "Cancelled manifest navigation exposed a target");
  require(sagan::language_service::query_manifest_dependency_target(
              *manifest_document.value, static_cast<sagan::source::byte_offset>(dependency_alias + 2),
              source, cancelled_manifest.token(), manifest_options).state ==
              sagan::diagnostics::result_state::cancelled,
          "Cancelled dependency navigation exposed a target");
  require(sagan::language_service::analyze_manifest_document(*manifest_document.value,
              cancelled_manifest.token()).state == sagan::diagnostics::result_state::cancelled,
          "Cancelled manifest analysis did not stop");
  const std::string unfinished_manifest_text = "[package]\nna";
  const sagan::source::document_snapshot unfinished_manifest(manifest_document.value->identity(), 8,
                                                               unfinished_manifest_text);
  const auto manifest_keys = sagan::language_service::complete_manifest_document(
      unfinished_manifest, static_cast<sagan::source::byte_offset>(unfinished_manifest_text.size()));
  require(manifest_keys.value && manifest_keys.value->size() == 1 &&
              manifest_keys.value->front().label == "name" &&
              manifest_keys.value->front().edit.replacement_utf8 == "name = \"\"",
          "Incomplete manifest did not receive compiler-owned key completion");
  const std::string dependency_prefix = "[dependencies]\norbit_";
  const sagan::source::document_snapshot dependency_manifest(
      manifest_document.value->identity(), 13, dependency_prefix);
  const auto dependency_items = sagan::language_service::complete_manifest_document(
      dependency_manifest, static_cast<sagan::source::byte_offset>(dependency_prefix.size()),
      {}, manifest_options);
  require(dependency_items.value && dependency_items.value->size() == 1 &&
              dependency_items.value->front().label == "orbit_tools" &&
              dependency_items.value->front().edit.replacement_utf8 ==
                  "orbit_tools = { package = \"orbit-tools\", version = \"^0.1.0\" }",
          "Installed package completion did not preserve an importable alias");
  const modules::package_resolution_options multiple_versions{
      "tests/fixtures/catalog/resolution-index.tsv", "2.1.0", {}};
  const auto newest_dependency = sagan::language_service::complete_manifest_document(
      dependency_manifest, static_cast<sagan::source::byte_offset>(dependency_prefix.size()),
      {}, multiple_versions);
  require(newest_dependency.value && newest_dependency.value->size() == 1 &&
              newest_dependency.value->front().edit.replacement_utf8.find("^0.1.1") != std::string::npos,
          "Dependency completion did not prefer the newest compatible installed version");
  const std::string dependency_value_text = "[dependencies]\norbit-tools = \"^0.\"";
  const sagan::source::document_snapshot dependency_value_document(
      manifest_document.value->identity(), 14, dependency_value_text);
  const auto value_position = dependency_value_text.find("^0.") + 3;
  const auto dependency_versions = sagan::language_service::complete_manifest_document(
      dependency_value_document, static_cast<sagan::source::byte_offset>(value_position),
      {}, multiple_versions);
  require(dependency_versions.value && dependency_versions.value->size() == 1 &&
              dependency_versions.value->front().label == "^0.1.1" &&
              dependency_versions.value->front().edit.replacement_utf8 == "\"^0.1.1\"" &&
              dependency_versions.value->front().edit.range.bytes ==
                  sagan::source::byte_range{static_cast<sagan::source::byte_offset>(value_position - 4),
                                            static_cast<sagan::source::byte_offset>(value_position + 1)},
          "Installed package version completion lost its requirement or quoted replacement range");
  const std::string alias_value_text =
      "[dependencies]\norbit_tools = { package = \"orbit-tools\", version = \"^0.\" }";
  const sagan::source::document_snapshot alias_value_document(
      manifest_document.value->identity(), 15, alias_value_text);
  const auto alias_value_position = alias_value_text.find("^0.") + 3;
  const auto alias_versions = sagan::language_service::complete_manifest_document(
      alias_value_document, static_cast<sagan::source::byte_offset>(alias_value_position),
      {}, multiple_versions);
  require(alias_versions.value && alias_versions.value->size() == 1 &&
              alias_versions.value->front().label == "^0.1.1" &&
              alias_versions.value->front().edit.replacement_utf8 == "\"^0.1.1\"",
          "Aliased dependency requirement completion did not resolve its package name");
  const modules::package_resolution_options unavailable_index{"build/no-such-index.tsv", "2.1.0", {}};
  const auto unavailable_versions = sagan::language_service::complete_manifest_document(
      dependency_value_document, static_cast<sagan::source::byte_offset>(value_position),
      {}, unavailable_index);
  require(unavailable_versions.value && unavailable_versions.value->empty(),
          "Dependency version completion invented a package without an installed index");
  require(sagan::language_service::complete_manifest_document(
              dependency_value_document, static_cast<sagan::source::byte_offset>(value_position),
              cancelled_manifest.token(), multiple_versions).state ==
              sagan::diagnostics::result_state::cancelled,
          "Cancelled dependency version completion exposed a stale candidate");
  const auto unavailable_dependencies = sagan::language_service::complete_manifest_document(
      dependency_manifest, static_cast<sagan::source::byte_offset>(dependency_prefix.size()),
      {}, unavailable_index);
  require(unavailable_dependencies.value && unavailable_dependencies.value->empty(),
          "Dependency completion invented a package without an installed index");
  require(sagan::language_service::complete_manifest_document(
              unfinished_manifest, static_cast<sagan::source::byte_offset>(unfinished_manifest_text.size()),
              cancelled_manifest.token()).state == sagan::diagnostics::result_state::cancelled,
          "Cancelled manifest completion did not stop");
  const sagan::source::document_snapshot section_manifest(manifest_document.value->identity(), 9,
                                                            "[pa]");
  const auto section_items = sagan::language_service::complete_manifest_document(section_manifest, 3);
  require(section_items.value && section_items.value->size() == 1 &&
              section_items.value->front().label == "package" &&
              section_items.value->front().edit.range.bytes == sagan::source::byte_range{1, 4} &&
              section_items.value->front().edit.replacement_utf8 == "package]",
          "Manifest section completion duplicated an existing closing bracket");
  const std::string mode_text = "[application]\nmode = \"wi\"";
  const sagan::source::document_snapshot mode_manifest(manifest_document.value->identity(), 11, mode_text);
  const auto mode_start = mode_text.find("\"wi\"");
  const auto mode_items = sagan::language_service::complete_manifest_document(
      mode_manifest, static_cast<sagan::source::byte_offset>(mode_start + 3));
  require(mode_items.value && mode_items.value->size() == 1 &&
              mode_items.value->front().label == "windowed" &&
              mode_items.value->front().edit.range.bytes ==
                  sagan::source::byte_range{static_cast<sagan::source::byte_offset>(mode_start),
                                            static_cast<sagan::source::byte_offset>(mode_start + 4)} &&
              mode_items.value->front().edit.replacement_utf8 == "\"windowed\"",
          "Application mode completion did not safely replace the quoted value");
  const std::string commented_mode_text = "[application]\nmode = \"wi\" # note";
  const sagan::source::document_snapshot commented_mode(manifest_document.value->identity(), 12,
                                                          commented_mode_text);
  const auto commented_items = sagan::language_service::complete_manifest_document(
      commented_mode, static_cast<sagan::source::byte_offset>(commented_mode_text.find("\"wi\"") + 3));
  require(commented_items.value && commented_items.value->empty(),
          "Manifest completion must not replace an adjacent comment");
  const auto mode_hover = sagan::language_service::hover_manifest_document(
      mode_manifest, static_cast<sagan::source::byte_offset>(mode_text.find("mode") + 2));
  require(mode_hover.value && mode_hover.value->markdown.find("console") != std::string::npos &&
              mode_hover.value->selection.bytes == sagan::source::byte_range{14, 18},
          "Manifest mode hover lost its compiler-owned documentation or key range");
  const auto section_hover = sagan::language_service::hover_manifest_document(section_manifest, 2);
  require(!section_hover.value, "Incomplete manifest section should not claim hover documentation");
  require(sagan::language_service::hover_manifest_document(mode_manifest, 15,
              cancelled_manifest.token()).state == sagan::diagnostics::result_state::cancelled,
          "Cancelled manifest hover did not stop");
  const sagan::source::document_snapshot outline_manifest(manifest_document.value->identity(), 13,
      "[package]\r\nname = \"demo\"\r\n[application]\r\nmode = \"console\"\r\n");
  const auto outline = sagan::language_service::manifest_document_symbols(outline_manifest);
  require(outline.value && outline.value->size() == 2 &&
              outline.value->front().name == "package" &&
              outline.value->front().children.size() == 1 &&
              outline.value->front().children.front().name == "name" &&
              outline.value->back().name == "application" &&
              outline.value->back().children.front().name == "mode" &&
              outline.value->front().range.bytes.end == 26,
          "CRLF manifest outline lost its section hierarchy or byte ranges");
  require(sagan::language_service::manifest_document_symbols(outline_manifest,
              cancelled_manifest.token()).state == sagan::diagnostics::result_state::cancelled,
          "Cancelled manifest symbol discovery did not stop");
  const modules::package_resolution_options options{index, "2.1.0", {}};
  const auto graph = modules::resolve_package(
      "tests/fixtures/catalog/consumer-alias", source, {}, options);
  require(graph.modules.size() == 3 &&
              std::any_of(graph.modules.begin(), graph.modules.end(), [](const auto &module)
              { return module.name == "orbit_tools.main" &&
                       module.path.parent_path().filename() == "src" &&
                       module.path.parent_path().parent_path().filename() == "orbit-tools"; }) &&
              std::any_of(graph.modules.begin(), graph.modules.end(), [](const auto &module)
              { return module.name == "orbit_tools" &&
                       module.path.filename() == "orbit_tools.sagan"; }),
          "Qualified dependency import did not resolve to installed source");
  const auto linked = modules::link_package(
      "tests/fixtures/catalog/consumer-alias", source, {}, options);
  static_cast<void>(semantic::check_types(linked));
  const auto importable = modules::importable_modules(
      "tests/fixtures/catalog/consumer-alias/src/main.sagan", {}, options);
  require(std::find(importable.begin(), importable.end(), "orbit_tools.main") != importable.end() &&
              std::find(importable.begin(), importable.end(), "orbit_tools") != importable.end(),
          "Import discovery omitted the package-qualified or local module");
  const auto entry = source.read_path("tests/fixtures/catalog/consumer-alias/src/main.sagan");
  require(static_cast<bool>(entry), "Import completion fixture was not readable");
  const auto workspace = semantic::build_workspace_index(graph, source);
  const semantic::semantic_index *entry_index = nullptr;
  for (const auto &module : workspace.modules())
    if (module.name == "main") entry_index = &module.index;
  require(entry_index, "Installed-package fixture omitted the consumer module");
  const sagan::language_service::document_queries package_query(*entry.value, *entry_index, &workspace);
  const std::string entry_text(entry.value->text());
  const auto package_dot = entry_text.find("package_tools.orbit_answer()");
  require(package_dot != std::string::npos, "Package namespace fixture is missing");
  const auto package_members = package_query.completions(
      static_cast<sagan::source::byte_offset>(package_dot + std::string("package_tools.").size()));
  require(package_members.value &&
              std::any_of(package_members.value->begin(), package_members.value->end(),
                          [](const auto &item)
                          { return item.label == "orbit_answer" && item.detail == "(): Int" &&
                                   item.source_module == "orbit_tools.main" &&
                                   !item.documentation.empty(); }),
          "Standard package namespace completion omitted signature or documentation");
  const auto probe_member = entry_text.find("probe.sample(41)");
  require(probe_member != std::string::npos, "Package-owned member fixture is missing");
  const auto probe_members = package_query.completions(
      static_cast<sagan::source::byte_offset>(probe_member + std::string("probe.").size()));
  require(probe_members.value &&
              std::any_of(probe_members.value->begin(), probe_members.value->end(),
                          [](const auto &item)
                          { return item.label == "sample" && item.detail == "(value: Int): Int" &&
                                   item.source_module == "orbit_tools.main"; }),
          "Package-owned method completion omitted its callable signature or source module");
  const std::string unimported_text = "module scratch\nfun main(): Int {\n  return 0\n}\n";
  const sagan::source::document_snapshot unimported(entry.value->identity(), 12, unimported_text);
  const auto unimported_index = sagan::language_service::index_document(unimported);
  require(unimported_index.value.has_value(), "Unimported package fixture did not index");
  const sagan::language_service::document_queries unimported_query(
      unimported, unimported_index.value->index, &workspace, &unimported_index.value->model);
  const auto package_imports = unimported_query.completions(
      static_cast<sagan::source::byte_offset>(unimported_text.find("return 0")));
  require(package_imports.value &&
              std::any_of(package_imports.value->begin(), package_imports.value->end(),
                          [](const auto &item)
                          { return item.label == "orbit_answer" && item.detail == "(): Int" &&
                                   item.additional_import_edits.size() == 1 &&
                                   item.additional_import_edits.front().replacement_utf8 ==
                                       "import orbit_answer from orbit_tools.main\n"; }),
          "Installed-package auto-import omitted the signature or dependency alias");
  const std::string crlf_text = "module scratch\r\n\r\nfun main(): Int {\r\n  return 0\r\n}\r\n";
  const sagan::source::document_snapshot crlf(entry.value->identity(), 13, crlf_text);
  const auto crlf_index = sagan::language_service::index_document(crlf);
  require(crlf_index.value.has_value(), "CRLF package fixture did not index");
  const sagan::language_service::document_queries crlf_query(
      crlf, crlf_index.value->index, &workspace, &crlf_index.value->model);
  const auto crlf_imports = crlf_query.completions(
      static_cast<sagan::source::byte_offset>(crlf_text.find("return 0")));
  require(crlf_imports.value &&
              std::any_of(crlf_imports.value->begin(), crlf_imports.value->end(),
                          [](const auto &item)
                          { return item.label == "orbit_answer" &&
                                   item.additional_import_edits.size() == 1 &&
                                   item.additional_import_edits.front().replacement_utf8 ==
                                       "import orbit_answer from orbit_tools.main\r\n"; }),
          "Package auto-import did not preserve the document's CRLF line endings");
  const auto proven_import = sagan::language_service::add_missing_import(
      crlf, workspace, [&]() -> semantic::symbol_id
      {
        for (const auto &item : *crlf_imports.value)
          if (item.label == "orbit_answer" && !item.additional_import_edits.empty()) return item.id;
        return {};
      }());
  require(proven_import.state == sagan::language_service::edit_state::ready,
          "Package auto-import candidate was not accepted by the safe-edit planner");
  const std::string collision_text = "module scratch\n"
      "fun other(): Int {\n  let orbit_answer = 1\n  return orbit_answer\n}\n"
      "fun main(): Int {\n  return 0\n}\n";
  const sagan::source::document_snapshot collision(entry.value->identity(), 14, collision_text);
  const auto collision_index = sagan::language_service::index_document(collision);
  require(collision_index.value.has_value(), "Package import-collision fixture did not index");
  const sagan::language_service::document_queries collision_query(
      collision, collision_index.value->index, &workspace, &collision_index.value->model);
  const auto collision_imports = collision_query.completions(
      static_cast<sagan::source::byte_offset>(collision_text.rfind("return 0")));
  require(collision_imports.value &&
              std::none_of(collision_imports.value->begin(), collision_imports.value->end(),
                           [](const auto &item)
                           { return item.label == "orbit_answer" &&
                                    !item.additional_import_edits.empty(); }),
          "Package auto-import offered a binding that collides elsewhere in the document");
  const std::string annotated_header =
      "module scratch // keep this comment\nfun main(): Int {\n  return 0\n}\n";
  const sagan::source::document_snapshot annotated(entry.value->identity(), 15, annotated_header);
  const auto annotated_index = sagan::language_service::index_document(annotated);
  require(annotated_index.value.has_value(), "Annotated module-header fixture did not index");
  const sagan::language_service::document_queries annotated_query(
      annotated, annotated_index.value->index, &workspace, &annotated_index.value->model);
  const auto annotated_imports = annotated_query.completions(
      static_cast<sagan::source::byte_offset>(annotated_header.find("return 0")));
  require(annotated_imports.value &&
              std::none_of(annotated_imports.value->begin(), annotated_imports.value->end(),
                           [](const auto &item) { return !item.additional_import_edits.empty(); }),
          "Package auto-import edited a module header without a proven insertion line");
  const std::string partial = "module main\nimport orbit_t";
  const sagan::source::document_snapshot unsaved(entry.value->identity(), 2, partial);
  const auto completions = sagan::language_service::query_import_modules(
      unsaved, static_cast<sagan::source::byte_offset>(partial.size()), {}, options);
  require(completions.applicable && completions.error.empty() &&
              std::any_of(completions.candidates.begin(), completions.candidates.end(),
                          [](const auto &item) { return item.name == "orbit_tools.main"; }),
          "An incomplete unsaved import did not retain package-module completion");
  const std::string qualified = "module main\nimport orbit_answer from orbit_tools.";
  const sagan::source::document_snapshot dotted(entry.value->identity(), 3, qualified);
  const auto dotted_completion = sagan::language_service::query_import_modules(
      dotted, static_cast<sagan::source::byte_offset>(qualified.size()), {}, options);
  require(dotted_completion.applicable && dotted_completion.error.empty() &&
              std::any_of(dotted_completion.candidates.begin(), dotted_completion.candidates.end(),
                          [](const auto &item)
                          { return item.name == "orbit_tools.main" &&
                                   item.replacement.bytes.begin == 37; }),
          "Dotted package import completion did not replace the full module path");
  const auto bounded_root = std::filesystem::path("build/package-resolution-module-bound");
  std::filesystem::create_directories(bounded_root);
  for (int module_index = 0; module_index < 270; ++module_index)
    std::ofstream(bounded_root / ("sample" + std::to_string(module_index) + ".sagan")) << "";
  const std::string bounded_text = "module main\nimport sample";
  const sagan::source::document_snapshot bounded_document(
      sagan::source::identity_from_path({}, bounded_root / "main.sagan"), 1, bounded_text);
  const auto bounded_modules = sagan::language_service::query_import_modules(
      bounded_document, static_cast<sagan::source::byte_offset>(bounded_text.size()));
  require(bounded_modules.applicable && bounded_modules.error.empty() &&
              bounded_modules.incomplete && bounded_modules.candidates.size() == 256 &&
              bounded_modules.candidates.front().name == "sample0",
          "Import module completion did not bound and sort its result set");
  std::filesystem::remove_all(bounded_root);
  const std::string partial_export = "module main\nimport orbit_a from orbit_tools.main\n";
  const sagan::source::document_snapshot export_document(entry.value->identity(), 4, partial_export);
  const auto export_completion = sagan::language_service::query_import_exports(
      export_document, static_cast<sagan::source::byte_offset>(partial_export.find(" from ")),
      source, {}, options);
  require(export_completion.applicable && export_completion.error.empty() &&
              export_completion.module == "orbit_tools.main" &&
              std::any_of(export_completion.candidates.begin(), export_completion.candidates.end(),
                          [](const auto &item)
                          { return item.public_name == "orbit_answer" &&
                                   !item.symbol_id.empty() && !item.source_uri.value.empty(); }),
          "Incomplete selective import did not offer installed package exports");
  const std::string exact_export = "module main\nimport orbit_answer from orbit_tools.main\n";
  const sagan::source::document_snapshot exact_document(entry.value->identity(), 8, exact_export);
  const auto export_target = sagan::language_service::query_import_export_target(
      exact_document, static_cast<sagan::source::byte_offset>(exact_export.find("orbit_answer") + 2),
      source, {}, options);
  require(export_target.applicable && export_target.target &&
              export_target.target->public_name == "orbit_answer" &&
              export_target.target->source_uri == sagan::source::identity_from_path(
                  {}, "tests/fixtures/catalog/orbit-tools/src/main.sagan").uri,
          "Selective import name did not navigate to the installed export");
  const auto alias_target = sagan::language_service::query_import_export_target(
      exact_document, static_cast<sagan::source::byte_offset>(exact_export.find("orbit_tools.main") + 2),
      source, {}, options);
  require(!alias_target.applicable,
          "Export navigation incorrectly captured the import module path");
  const auto imported_target = sagan::language_service::query_import_module_target(
      export_document, static_cast<sagan::source::byte_offset>(partial_export.find("orbit_tools.main") + 2),
      source, {}, options);
  require(imported_target.applicable && imported_target.error.empty() &&
              imported_target.source_uri == sagan::source::identity_from_path(
                  {}, "tests/fixtures/catalog/orbit-tools/src/main.sagan").uri &&
              imported_target.start.line == 0,
          "Package import path did not navigate to installed module source");
  const std::string local_import = "module main\nimport orbit_tools\n";
  const sagan::source::document_snapshot local_document(entry.value->identity(), 7, local_import);
  const auto local_target = sagan::language_service::query_import_module_target(
      local_document, static_cast<sagan::source::byte_offset>(local_import.find("orbit_tools") + 2),
      source, {}, options);
  require(local_target.applicable && local_target.error.empty() &&
              local_target.source_uri.value.find("/consumer-alias/src/orbit_tools.sagan") !=
                  std::string::npos,
          "Unqualified import navigation did not prefer the workspace module");
  const std::string unavailable_import = "module main\nimport missing_package.main\n";
  const sagan::source::document_snapshot unavailable_document(entry.value->identity(), 11,
                                                                unavailable_import);
  const auto unavailable_target = sagan::language_service::query_import_module_target(
      unavailable_document,
      static_cast<sagan::source::byte_offset>(unavailable_import.find("missing_package.main") + 2),
      source, {}, options);
  require(unavailable_target.applicable && !unavailable_target.error.empty() &&
              unavailable_target.source_uri.value.empty(),
          "Unavailable module navigation invented a filesystem target");
  sagan::diagnostics::cancellation_source cancelled_navigation;
  cancelled_navigation.cancel();
  const auto cancelled_target = sagan::language_service::query_import_module_target(
      export_document, static_cast<sagan::source::byte_offset>(partial_export.find("orbit_tools.main") + 2),
      source, cancelled_navigation.token(), options);
  require(cancelled_target.cancelled && cancelled_target.source_uri.value.empty(),
          "Cancelled import navigation exposed a stale source target");
  sagan::source::document_store overlays;
  const auto installed_uri = sagan::source::identity_from_path(
      {}, "tests/fixtures/catalog/orbit-tools/src/main.sagan").uri;
  require(static_cast<bool>(overlays.open(installed_uri, 1,
      "module main\nfun overlay_answer(): Int => 7\nexport overlay_answer\n")),
      "Could not open an unsaved installed-module overlay");
  const std::string overlay_import = "module main\nimport overlay_a from orbit_tools.main\n";
  const sagan::source::document_snapshot overlay_document(entry.value->identity(), 5, overlay_import);
  const auto overlay_completion = sagan::language_service::query_import_exports(
      overlay_document, static_cast<sagan::source::byte_offset>(overlay_import.find(" from ")),
      overlays, {}, options);
  require(overlay_completion.applicable && overlay_completion.error.empty() &&
              overlay_completion.candidates.size() == 1 &&
              overlay_completion.candidates.front().public_name == "overlay_answer",
          "Installed-package completion ignored the newer in-memory source overlay");
  const std::string overlay_exact = "module main\nimport overlay_answer from orbit_tools.main\n";
  const sagan::source::document_snapshot overlay_exact_document(entry.value->identity(), 9,
                                                                  overlay_exact);
  const auto overlay_target = sagan::language_service::query_import_export_target(
      overlay_exact_document,
      static_cast<sagan::source::byte_offset>(overlay_exact.find("overlay_answer") + 2),
      overlays, {}, options);
  require(overlay_target.applicable && overlay_target.target &&
              overlay_target.target->public_name == "overlay_answer" &&
              overlay_target.target->source_uri == installed_uri,
          "Selective import navigation ignored the installed-source overlay");
  sagan::diagnostics::cancellation_source cancelled_export_target;
  cancelled_export_target.cancel();
  const auto cancelled_target_name = sagan::language_service::query_import_export_target(
      overlay_exact_document,
      static_cast<sagan::source::byte_offset>(overlay_exact.find("overlay_answer") + 2),
      overlays, cancelled_export_target.token(), options);
  require(cancelled_target_name.cancelled && !cancelled_target_name.target,
          "Cancelled selective import navigation exposed a source target");
  const std::string missing_export = "module main\nimport hidden_answer from orbit_tools.main\n";
  const sagan::source::document_snapshot missing_document(entry.value->identity(), 10,
                                                            missing_export);
  const auto missing_target = sagan::language_service::query_import_export_target(
      missing_document,
      static_cast<sagan::source::byte_offset>(missing_export.find("hidden_answer") + 2),
      overlays, {}, options);
  require(missing_target.applicable && !missing_target.target,
          "Selective import navigation invented a non-exported symbol");
  sagan::source::document_store unicode_overlays;
  require(static_cast<bool>(unicode_overlays.open(installed_uri, 1,
      "module main\nfun 🚀(): Int => 7\nexport 🚀\n")),
      "Could not open the Unicode installed-module overlay");
  const std::string unicode_import = "module main\nimport 🚀 from orbit_tools.main\n";
  const sagan::source::document_snapshot unicode_document(entry.value->identity(), 11,
                                                            unicode_import);
  const auto unicode_target = sagan::language_service::query_import_export_target(
      unicode_document,
      static_cast<sagan::source::byte_offset>(unicode_import.find("🚀")),
      unicode_overlays, {}, options);
  require(unicode_target.applicable && unicode_target.target &&
              unicode_target.target->public_name == "🚀" &&
              unicode_target.target->start.line == 1,
          "Emoji export navigation did not preserve UTF-16 source positions");
  std::string many_exports = "module main\n";
  for (int index = 0; index < 270; ++index)
  {
    const auto name = "item" + std::to_string(index);
    many_exports += "fun " + name + "(): Int => " + std::to_string(index) + "\n";
    many_exports += "export " + name + "\n";
  }
  require(static_cast<bool>(overlays.replace(installed_uri, 1, 2, many_exports)),
          "Could not replace the installed-module overlay");
  const std::string broad_import = "module main\nimport item from orbit_tools.main\n";
  const sagan::source::document_snapshot broad_document(entry.value->identity(), 6, broad_import);
  const auto bounded_completion = sagan::language_service::query_import_exports(
      broad_document, static_cast<sagan::source::byte_offset>(broad_import.find(" from ")),
      overlays, {}, options);
  require(bounded_completion.applicable && bounded_completion.error.empty() &&
              bounded_completion.incomplete && bounded_completion.candidates.size() == 256,
          "Installed-package export completion did not bound its result set");
  sagan::diagnostics::cancellation_source cancelled_import;
  cancelled_import.cancel();
  const auto cancelled_export = sagan::language_service::query_import_exports(
      export_document, static_cast<sagan::source::byte_offset>(partial_export.find(" from ")),
      source, cancelled_import.token(), options);
  require(cancelled_export.cancelled && cancelled_export.candidates.empty(),
          "Cancelled package-export completion returned stale candidates");
  const auto multi_index = "tests/fixtures/catalog/resolution-index.tsv";
  const auto newest = modules::resolve_indexed_dependencies(root, multi_index, "2.1.0");
  require(newest.state == modules::dependency_state::ready && newest.packages.size() == 1 &&
              newest.packages[0].version == "0.1.1",
          "Unlocked selection did not choose the highest compatible installed version");
  const auto older_lock = modules::resolve_indexed_dependencies(root, multi_index, "2.1.0", lock);
  require(older_lock.state == modules::dependency_state::ready &&
              older_lock.packages[0].version == "0.1.0",
          "An exact lockfile pin was upgraded implicitly");
  const auto transitive = modules::resolve_indexed_dependencies(
      "tests/fixtures/catalog/consumer-transitive/sagan.toml", multi_index, "2.1.0");
  require(transitive.state == modules::dependency_state::ready && transitive.packages.size() == 2 &&
              transitive.packages[0].name == "orbit-tools" &&
              transitive.packages[1].name == "parent",
          "Transitive package closure or deterministic ordering failed");
  const modules::package_resolution_options transitive_options{multi_index, "2.1.0", {}};
  const auto transitive_graph = modules::resolve_package(
      "tests/fixtures/catalog/consumer-transitive", source, {}, transitive_options);
  require(transitive_graph.modules.size() == 3 &&
              std::any_of(transitive_graph.modules.begin(), transitive_graph.modules.end(),
                          [](const auto &module)
                          { return module.name == "parent.orbit_tools.main"; }),
          "Transitive package import did not retain its qualified module identity");
  static_cast<void>(semantic::check_types(modules::link_package(
      "tests/fixtures/catalog/consumer-transitive", source, {}, transitive_options)));
  require(modules::resolve_indexed_dependencies(
              "tests/fixtures/modules/package_dependency/sagan.toml", index, "2.1.0").state ==
              modules::dependency_state::unavailable,
          "Available-only package was treated as installed");
  require(modules::resolve_indexed_dependencies(root, index, "3.1.0", lock).state ==
              modules::dependency_state::incompatible,
          "Compiler-incompatible package was accepted");
  require(modules::resolve_indexed_dependencies(root, index, "2.1.0", "missing.lock").state ==
              modules::dependency_state::invalid_lock,
          "Missing lockfile was accepted");
  require(modules::resolve_indexed_dependencies(root, "missing-index.tsv", "2.1.0", lock).state ==
              modules::dependency_state::index_unavailable,
          "Unavailable index was accepted");
}
