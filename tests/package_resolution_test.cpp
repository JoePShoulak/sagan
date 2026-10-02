#include "../src/modules/package_index.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/language_service/package_catalog.hpp"
#include "../src/semantic/type_checker.hpp"
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
