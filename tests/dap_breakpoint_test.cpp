#include "../src/dap/breakpoints.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/semantic/analyzer.hpp"
#include "../src/semantic/type_checker.hpp"
#include "../src/source/provider.hpp"
#include "../src/syntax/syntax.hpp"

#include <stdexcept>
#include <string>

namespace
{
  auto require(const bool value, const char *message) -> void
  {
    if (!value) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  const std::string text = "let 🚀 = 42\nprint(🚀)\n";
  const sagan::source::document_snapshot document(
      {{sagan::source::document_id{1}, sagan::source::document_uri{"untitled:dap"}, {}}, 5, text});
  const auto parsed = sagan::syntax::analyze(document, {.recover = false});
  require(parsed.value && parsed.value->strict_ast, "DAP source did not parse");
  const auto &tree = *parsed.value->strict_ast;
  const auto model = semantic::analyze(tree);
  const auto types = semantic::check_types(tree);
  const auto generated = codegen::generate_cpp_mapped(tree, types, "demo.sagan");
  const auto metadata = sagan::language_service::derive_debug_metadata(document, model, types, generated);
  const auto mapped = sagan::dap::map_breakpoint(metadata, generated, document, {1, 8});
  require(mapped.resolved && mapped.resolved->position.line == 1 &&
              mapped.resolved->generated_line > 0 && mapped.resolved->generated_column > 0,
          "UTF-16 breakpoint did not map to generated source");
  require(!sagan::dap::map_breakpoint(metadata, generated, document, {2, 0}).resolved &&
              !sagan::dap::map_breakpoint(metadata, generated, document, {20, 0}).resolved &&
              !sagan::dap::map_breakpoint(metadata, generated, document, {0, 5}).resolved,
          "Nonexecutable line unexpectedly accepted a breakpoint");

  const sagan::source::disk_source_provider source;
  const auto main = source.read_path("tests/fixtures/modules/module_demo/main.sagan");
  const auto imported = source.read_path("tests/fixtures/modules/module_demo/guidance.sagan");
  require(main && imported, "Multi-module fixture missing");
  const auto linked = modules::link("tests/fixtures/modules/module_demo/main.sagan");
  const auto linked_model = semantic::analyze(linked);
  const auto linked_types = semantic::check_types(linked);
  const auto linked_cpp = codegen::generate_cpp_mapped(linked, linked_types);
  const auto linked_metadata = sagan::language_service::derive_debug_metadata(
      *main.value, linked_model, linked_types, linked_cpp, &source);
  bool found_imported = false;
  for (const auto &candidate : linked_metadata.breakpoints)
  {
    if (candidate.source.document != imported.value->identity().id) continue;
    const auto position = imported.value->to_utf16(candidate.source.bytes.begin);
    if (!position) continue;
    const auto resolution = sagan::dap::map_breakpoint(linked_metadata, linked_cpp,
                                                       *imported.value, *position);
    require(resolution.resolved && resolution.resolved->source.document ==
                imported.value->identity().id, "Imported breakpoint mapped to another module");
    found_imported = true;
    break;
  }
  require(found_imported, "No imported source breakpoint was available");
  return 0;
}
