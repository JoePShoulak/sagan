#include "../src/modules/package_index.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/language_service/package_catalog.hpp"
#include "../src/language_service/queries.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/semantic/type_checker.hpp"
#include "../src/semantic/workspace_index.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
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
