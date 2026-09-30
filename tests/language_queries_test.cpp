#include "../src/language_service/queries.hpp"
#include "../src/source/provider.hpp"
#include "../src/modules/resolver.hpp"

#include <stdexcept>
#include <string>

namespace
{
  auto require(const bool valid, const char *message) -> void
  {
    if (!valid) throw std::runtime_error(message);
  }

  auto at(const std::string &text, const std::string &needle, const std::size_t after = 0)
    -> sagan::source::byte_offset
  {
    const auto found = text.find(needle, after);
    if (found == std::string::npos) throw std::runtime_error("missing test marker");
    return static_cast<sagan::source::byte_offset>(found);
  }
}

auto main() -> int
{
  using namespace sagan;
  const std::string text =
      "/// Launches a value.\n"
      "fun 🚀(value: Int): Int => value + 1\n"
      "fun main(): Int {\n"
      "  let value = 🚀(41)\n"
      "  let message = \"🚀\"\n"
      "  return value\n"
      "}\n";
  const source::document_snapshot document(
      source::document_identity{source::document_id{80}, source::document_uri{"untitled:queries"}, {}},
      4, text);
  const auto indexed = language_service::index_document(document);
  require(indexed.value.has_value(), "source did not produce a semantic index");
  const language_service::document_queries query(document, indexed.value->index);

  const auto emoji_use = at(text, "🚀(41)");
  const auto selected = query.symbol_at(emoji_use);
  require(selected.value && selected.value->name == "🚀" &&
              selected.value->selection.bytes.begin == emoji_use,
          "emoji call did not select its exact symbol range");
  const auto utf16 = document.to_utf16(emoji_use);
  require(utf16 && query.symbol_at(*utf16).value->id == selected.value->id,
          "UTF-16 and byte queries disagree");
  require(query.symbol_at(source::utf16_position{utf16->line, utf16->character + 1}).state ==
              diagnostics::result_state::incomplete,
          "split surrogate pair was accepted");

  const auto definition = query.definitions(emoji_use);
  require(definition.value && definition.value->size() == 1 &&
              definition.value->front().bytes.begin == at(text, "🚀(value"),
          "emoji call did not resolve to its declaration");
  const auto references = query.references(emoji_use, true);
  require(references.value && references.value->size() >= 2,
          "declaration and call were not both reported");
  const auto highlights = query.document_highlights(emoji_use);
  require(highlights.value && highlights.value->size() == references.value->size(),
          "document highlights lost local symbol occurrences");
  require(query.resolved_type(emoji_use).value.has_value(),
          "resolved type was not available at the call site");
  const auto hover = query.hover(emoji_use);
  require(hover.value && hover.value->documentation.size() == 1 &&
              hover.value->documentation.front() == "Launches a value.",
          "source documentation did not reach hover");
  require(!query.symbol_at(at(text, "return value") + 1).value,
          "keyword should not resolve to a declaration spanning its function body");
  require(!query.symbol_at(at(text, "\"🚀\"") + 1).value,
          "text inside a string should not resolve as a source symbol");
  const auto outline = query.document_symbols();
  require(outline.value && outline.value->size() >= 2 &&
              outline.value->front().symbol.name == "🚀" &&
              !outline.value->back().children.empty(),
          "document symbols are not in source order");

  const source::document_snapshot changed(document.identity(), 5, text);
  const language_service::document_queries stale(changed, indexed.value->index);
  require(stale.symbol_at(emoji_use).state == diagnostics::result_state::stale &&
              stale.document_symbols().state == diagnostics::result_state::stale,
          "old index was used for a newer document");

  const std::string composition =
      "face Readable { fun read(): Int }\n"
      "class Probe is Readable { fun read(): Int => 1 }\n";
  const source::document_snapshot composed(
      source::document_identity{source::document_id{81}, source::document_uri{"untitled:composition"}, {}},
      1, composition);
  const auto composed_index = language_service::index_document(composed);
  require(composed_index.value.has_value(), "face fixture did not produce an index");
  const language_service::document_queries composed_query(composed, composed_index.value->index);
  const auto implementations = composed_query.implementations(at(composition, "Readable"));
  require(implementations.value && implementations.value->size() == 1 &&
              implementations.value->front().bytes.begin == at(composition, "class Probe"),
          "face implementation did not use the conformance index");

  const std::string shadowing =
      "fun main(): Int {\n"
      "  let altitude = 41\n"
      "  if altitude > 0 {\n"
      "    let altitude = 7\n"
      "    print(altitude)\n"
      "  }\n"
      "  return altitude\n"
      "}\n";
  const source::document_snapshot shadowed(
      source::document_identity{source::document_id{82}, source::document_uri{"untitled:shadowing"}, {}},
      1, shadowing);
  const auto shadowed_index = language_service::index_document(shadowed);
  require(shadowed_index.value.has_value(), "shadowing fixture did not produce an index");
  const language_service::document_queries shadowed_query(shadowed, shadowed_index.value->index);
  const auto inner = shadowed_query.symbol_at(at(shadowing, "altitude)") );
  const auto outer = shadowed_query.symbol_at(at(shadowing, "return altitude") + 7);
  require(inner.value && outer.value && inner.value->id != outer.value->id,
          "shadowed locals resolved to the same symbol identity");

  const source::disk_source_provider disk;
  const auto graph = modules::resolve("tests/fixtures/modules/module_demo/main.sagan", disk);
  const auto workspace_index = semantic::build_workspace_index(graph, disk);
  const auto source = disk.read_path("tests/fixtures/modules/module_demo/main.sagan");
  require(source.value.has_value(), "could not read workspace fixture");
  const semantic::semantic_index *module_index = nullptr;
  for (const auto &module : workspace_index.modules())
    if (module.name == "main") module_index = &module.index;
  require(module_index, "workspace index omitted main module");
  const language_service::document_queries workspace_query(*source.value, *module_index, &workspace_index);
  const auto imported_use = at(std::string(source.value->text()), "calculate_course(40)");
  const auto linked = workspace_query.definitions(imported_use);
  require(linked.value && linked.value->size() == 1 &&
              linked.value->front().document != source.value->identity().id,
          "imported name did not navigate to its target module");
  return 0;
}
