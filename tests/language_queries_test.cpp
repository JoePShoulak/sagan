#include "../src/language_service/queries.hpp"
#include "../src/source/provider.hpp"
#include "../src/modules/resolver.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

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
  const language_service::document_queries query(document, indexed.value->index, nullptr,
                                                  &indexed.value->model, &*indexed.value->types);

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
  const auto position = query.context_at(emoji_use);
  require(position.value && position.value->containing_declaration &&
              position.value->scopes.size() >= 2 && position.value->expression_type,
          "position context omitted its containing declaration, scope, or expression");
  const auto hover = query.hover(emoji_use);
  require(hover.value && hover.value->documentation.size() == 1 &&
              hover.value->documentation.front() == "Launches a value.",
          "source documentation did not reach hover");
  const auto source_docs = query.documentation_at(emoji_use);
  require(source_docs.value && source_docs.value->summary == "Launches a value." &&
              source_docs.value->source.has_value(),
          "structured source documentation was not attached to the function");
  require(!query.symbol_at(at(text, "return value") + 1).value,
          "keyword should not resolve to a declaration spanning its function body");
  require(!query.symbol_at(at(text, "\"🚀\"") + 1).value,
          "text inside a string should not resolve as a source symbol");
  const auto outline = query.document_symbols();
  require(outline.value && outline.value->size() >= 2 &&
              outline.value->front().symbol.name == "🚀" &&
              !outline.value->back().children.empty(),
          "document symbols are not in source order");
  const auto classifications = query.semantic_classifications();
  require(classifications.value &&
              std::any_of(classifications.value->begin(), classifications.value->end(),
                          [&](const auto &entry)
                          {
                            return entry.range.bytes.begin == emoji_use &&
                                   entry.kind == semantic::symbol_kind::function && !entry.declaration;
                          }),
          "semantic classification missed the emoji call");
  const auto folds = query.folding_regions();
  require(folds.value && folds.value->size() == 1 &&
              folds.value->front().kind == language_service::folding_kind::block,
          "multiline function block was not identified for folding");
  const auto selections = query.selection_ranges(at(text, "41"));
  require(selections.value && selections.value->size() >= 4 &&
              selections.value->front().bytes ==
                  source::byte_range{at(text, "41"), at(text, "41") + 2} &&
              selections.value->back().bytes == source::byte_range{0, static_cast<source::byte_offset>(text.size())},
          "selection expansion did not reach from a literal to the document");
  for (std::size_t i = 1; i < selections.value->size(); ++i)
    require((*selections.value)[i].bytes.begin <= (*selections.value)[i - 1].bytes.begin &&
                (*selections.value)[i].bytes.end >= (*selections.value)[i - 1].bytes.end,
            "selection ranges were not strictly nested");
  const auto emoji_selections = query.selection_ranges(emoji_use);
  require(emoji_selections.value && emoji_selections.value->front().bytes.end -
                                          emoji_selections.value->front().bytes.begin ==
                                          std::string("🚀").size(),
          "selection expansion split a UTF-8 emoji token");
  const auto comment_selections = query.selection_ranges(at(text, "Launches"));
  require(comment_selections.value && comment_selections.value->front().bytes.begin == 0,
          "selection expansion omitted documentation-comment trivia");
  require(query.selection_ranges(static_cast<source::byte_offset>(text.size() + 1)).state ==
              diagnostics::result_state::incomplete,
          "selection expansion accepted a position after the document");
  const auto rocket_completion = query.completions(emoji_use + std::string("🚀").size());
  require(rocket_completion.value && rocket_completion.value->size() == 1 &&
              rocket_completion.value->front().label == "🚀" &&
              rocket_completion.value->front().replacement.bytes.begin == emoji_use,
          "emoji prefix completion did not preserve the replacement range");
  const auto rocket_signature = query.signature_help(emoji_use + std::string("🚀(").size());
  require(rocket_signature.value && rocket_signature.value->active_parameter == 0 &&
              rocket_signature.value->parameter_types.size() == 1 &&
              rocket_signature.value->parameter_names == std::vector<std::string>{"value"} &&
              rocket_signature.value->documentation == std::vector<std::string>{"Launches a value."} &&
              rocket_signature.value->result_type == "Int64" &&
              rocket_signature.value->label.find("🚀(") == 0,
          "resolved call signature was not available inside its argument list");
  require(!query.signature_help(at(text, "return value")).value,
          "signature help appeared outside a call");
  const auto no_model = language_service::document_queries(document, indexed.value->index);
  require(no_model.completions(emoji_use).state == diagnostics::result_state::incomplete,
          "completion without scope metadata silently returned an empty list");
  require(no_model.signature_help(emoji_use).state == diagnostics::result_state::incomplete,
          "signature help without resolved calls silently returned an empty result");

  const source::document_snapshot changed(document.identity(), 5, text);
  const language_service::document_queries stale(changed, indexed.value->index);
  require(stale.symbol_at(emoji_use).state == diagnostics::result_state::stale &&
              stale.document_symbols().state == diagnostics::result_state::stale,
          "old index was used for a newer document");
  const language_service::document_queries stale_signature(changed, indexed.value->index, nullptr,
                                                            &indexed.value->model, &*indexed.value->types);
  require(stale_signature.signature_help(emoji_use).state == diagnostics::result_state::stale,
          "signature help accepted resolved calls from an older document version");
  require(stale.selection_ranges(emoji_use).state == diagnostics::result_state::stale,
          "selection expansion accepted an old semantic index");

  const std::string sugar_source =
      "fun main(): Int {\n"
      "  let a = 0\n"
      "  let b = 1\n"
      "  let indices = 5.times\n"
      "  let phi = (1 + 5 ^ 0.5) / 2\n"
      "  let raised = phi ^ b\n"
      "  let rounded = Int.round(raised)\n"
      "  a, b = b, a + b\n"
      "  return indices[0] + a + b + rounded\n"
      "}\n";
  const source::document_snapshot sugar_document(
      source::document_identity{source::document_id{89}, source::document_uri{"untitled:sugar"}, {}},
      1, sugar_source);
  const auto sugar_index = language_service::index_document(sugar_document);
  require(sugar_index.value && sugar_index.value->types,
          "new syntax did not produce compiler-backed language queries");
  const language_service::document_queries sugar_query(sugar_document, sugar_index.value->index,
                                                       nullptr, &sugar_index.value->model,
                                                       &*sugar_index.value->types);
  const auto times_completion = sugar_query.completions(at(sugar_source, "5.times") + 4);
  require(times_completion.value && times_completion.value->size() == 1 &&
              times_completion.value->front().label == "times" &&
              times_completion.value->front().detail == "Array<Int64>" &&
              !times_completion.value->front().documentation.empty(),
          "integer member completion did not expose the compiler's times metadata");
  const auto times_hover = sugar_query.hover(at(sugar_source, "5.times") + 2);
  require(times_hover.value && times_hover.value->type == "Array<Int64>" &&
              times_hover.value->symbol.origin == semantic::symbol_origin::builtin &&
              !times_hover.value->documentation.empty(),
          "integer times hover did not expose compiler-owned documentation");
  const auto sugar_classifications = sugar_query.semantic_classifications();
  require(sugar_classifications.value &&
              std::any_of(sugar_classifications.value->begin(), sugar_classifications.value->end(),
                          [&](const auto &entry)
                          {
                            return entry.range.bytes.begin == at(sugar_source, "5.times") + 2 &&
                                   entry.builtin && !entry.unresolved;
                          }),
          "semantic tokens marked the built-in times member unresolved");
  const auto raised_type = sugar_query.resolved_type(at(sugar_source, "phi ^ b"));
  require(raised_type.value && *raised_type.value == "Float64",
          "mixed float/integer exponent was not typed for the editor");
  const auto round_completion = sugar_query.completions(at(sugar_source, "Int.round") + 6);
  require(round_completion.value && round_completion.value->size() == 1 &&
              round_completion.value->front().label == "round" &&
              round_completion.value->front().detail == "(Float) => Int64",
          "built-in Int.round completion was not available");
  const auto round_hover = sugar_query.hover(at(sugar_source, "Int.round") + 4);
  require(round_hover.value && round_hover.value->type == "Int64" &&
              round_hover.value->symbol.kind == semantic::symbol_kind::method,
          "built-in Int.round hover was not available");
  const auto round_signature = sugar_query.signature_help(at(sugar_source, "Int.round(raised)") + 10);
  require(round_signature.value && round_signature.value->parameter_names ==
              std::vector<std::string>{"value"} &&
              round_signature.value->result_type == "Int64" &&
              !round_signature.value->documentation.empty(),
          "built-in Int.round signature help omitted compiler metadata");
  require(sugar_query.symbol_at(at(sugar_source, "a, b =")).value.has_value(),
          "parallel reassignment targets were not indexed");

  const std::string grouped_source =
      "fun fast(n: Int): Int {\n"
      "  let i, a, b = 0, 0, 1\n"
      "  while i++ <= n  a, b = b, a+b\n"
      "  return b\n"
      "}\n";
  const source::document_snapshot grouped_document(
      source::document_identity{source::document_id{82}, source::document_uri{"untitled:grouped-let"}, {}},
      1, grouped_source);
  const auto grouped_index = language_service::index_document(grouped_document);
  require(grouped_index.value && grouped_index.value->types,
          "parallel let declaration was not indexed and typed for editor queries");
  const language_service::document_queries grouped_query(grouped_document, grouped_index.value->index,
                                                           nullptr, &grouped_index.value->model,
                                                           &*grouped_index.value->types);
  const auto first_binding = at(grouped_source, "let i, a, b") + 4;
  for (const auto offset : {0, 3, 6})
    require(grouped_query.symbol_at(first_binding + offset).value.has_value(),
            "a parallel let binding was not navigable by symbol identity");
  const auto grouped_hints = grouped_query.inlay_hints(
      source::byte_range{0, static_cast<source::byte_offset>(grouped_source.size())});
  require(grouped_hints.value && grouped_hints.value->size() >= 3,
          "parallel let bindings omitted inferred type hints");

  const std::string composition =
      "face Readable { fun read(): Int\n  fun describe(): String => \"readable\" }\n"
      "class Probe has Readable { fun read(): Int => 1 }\n"
      "fun main(): Int {\n  let probe = Probe()\n  return probe.read()\n}\n";
  const source::document_snapshot composed(
      source::document_identity{source::document_id{81}, source::document_uri{"untitled:composition"}, {}},
      1, composition);
  const auto composed_index = language_service::index_document(composed);
  require(composed_index.value.has_value(), "face fixture did not produce an index");
  require(composed_index.value->types.has_value(), "composition fixture did not retain type metadata");
  const language_service::document_queries composed_query(composed, composed_index.value->index,
                                                           nullptr, &composed_index.value->model,
                                                           &*composed_index.value->types);
  const auto implementations = composed_query.implementations(at(composition, "Readable"));
  require(implementations.value && implementations.value->size() == 1 &&
              implementations.value->front().bytes.begin == at(composition, "class Probe"),
          "face implementation did not use the conformance index");
  const auto probe_type = composed_query.type_definitions(at(composition, "return probe") + 7);
  require(probe_type.value && probe_type.value->size() == 1 &&
              probe_type.value->front().bytes.begin == at(composition, "class Probe"),
          "inferred nominal type did not navigate to its declaration");
  const auto hierarchy = composed_query.type_hierarchy(at(composition, "Readable"));
  require(hierarchy.value && hierarchy.value->subtypes.size() == 1 &&
              hierarchy.value->subtypes.front().name == "Probe",
          "face hierarchy did not find its implementing class");
  const auto hints = composed_query.inlay_hints({0, static_cast<source::byte_offset>(composition.size())});
  require(hints.value && std::any_of(hints.value->begin(), hints.value->end(),
                                   [](const auto &hint) { return hint.label == ": Probe"; }),
          "inferred local type did not produce an inlay hint");
  const auto method_completion = composed_query.completions(at(composition, "probe.read()") + 8);
  require(method_completion.value && method_completion.value->size() == 1 &&
              method_completion.value->front().label == "read" &&
              method_completion.value->front().kind == semantic::symbol_kind::method,
          "member completion did not use the receiver's inferred type");
  const auto inherited_completion = composed_query.completions(at(composition, "probe.read()") + 6);
  require(inherited_completion.value &&
              std::any_of(inherited_completion.value->begin(), inherited_completion.value->end(),
                          [](const auto &item) { return item.label == "describe"; }),
          "member completion omitted a composed face default");

  const std::string inheritance =
      "face Named { fun name(): String }\n"
      "class Ship has Named { fun name(): String => \"ship\" }\n"
      "class Aircraft { fun altitude(): Int => 7 }\n"
      "class GunShip is Ship, Aircraft { fun inherited(): String => self.name()\n"
      "  fun label(): String => super.Ship.name() }\n"
      "let gunship = GunShip()\n"
      "let named: Named = gunship\n"
      "print(gunship.altitude())\n";
  const source::document_snapshot inherited_document(
      source::document_identity{source::document_id{91}, source::document_uri{"untitled:inheritance"}, {}},
      1, inheritance);
  const auto inherited_index = language_service::index_document(inherited_document);
  require(inherited_index.value && inherited_index.value->types,
          "class inheritance fixture did not produce typed editor data");
  const language_service::document_queries inherited_query(
      inherited_document, inherited_index.value->index, nullptr,
      &inherited_index.value->model, &*inherited_index.value->types);
  const auto ship_hierarchy = inherited_query.type_hierarchy(at(inheritance, "class Ship") + 6);
  require(ship_hierarchy.value && std::any_of(ship_hierarchy.value->subtypes.begin(),
              ship_hierarchy.value->subtypes.end(), [](const auto &item) { return item.name == "GunShip"; }),
          "class inheritance did not appear in type hierarchy");
  const auto inherited_definition = inherited_query.definitions(at(inheritance, "self.name()") + 6);
  require(inherited_definition.value && inherited_definition.value->size() == 1 &&
              inherited_definition.value->front().bytes.begin == at(inheritance, "fun name(): String =>") + 4,
          "inherited method did not navigate to its declaring class");
  const auto super_definition = inherited_query.definitions(at(inheritance, "super.Ship.name()") + 11);
  require(super_definition.value && super_definition.value->size() == 1 &&
              super_definition.value->front().bytes.begin == at(inheritance, "fun name(): String =>") + 4,
          "qualified parent call did not navigate to its implementation");
  const auto parent_completion = inherited_query.completions(at(inheritance, "gunship.altitude()") + 10);
  require(parent_completion.value && std::any_of(parent_completion.value->begin(),
              parent_completion.value->end(), [](const auto &item) { return item.label == "altitude"; }),
          "member completion omitted a second parent class");

  const std::string forwarding =
      "class Parent {\n  let value: Int\n  new(value: Int) { self.value = value }\n}\n"
      "class Child is Parent {\n  new(value: Int) is Parent(value) { }\n}\n"
      "let child = Child(7)\n";
  const source::document_snapshot forwarding_document(
      source::document_identity{source::document_id{92}, source::document_uri{"untitled:forwarding"}, {}},
      1, forwarding);
  const auto forwarding_index = language_service::index_document(forwarding_document);
  require(forwarding_index.value && forwarding_index.value->types,
          "constructor forwarding fixture did not produce typed editor data");
  const language_service::document_queries forwarding_query(
      forwarding_document, forwarding_index.value->index, nullptr,
      &forwarding_index.value->model, &*forwarding_index.value->types);
  const auto forwarded_parent = forwarding_query.definitions(at(forwarding, "is Parent(value)") + 4);
  require(forwarded_parent.value && forwarded_parent.value->size() == 1 &&
              forwarded_parent.value->front().bytes.begin == at(forwarding, "class Parent") + 6,
          "constructor parent initializer did not navigate to its class");

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
  const language_service::document_queries shadowed_query(shadowed, shadowed_index.value->index,
                                                            nullptr, &shadowed_index.value->model);
  const auto inner = shadowed_query.symbol_at(at(shadowing, "altitude)") );
  const auto outer = shadowed_query.symbol_at(at(shadowing, "return altitude") + 7);
  require(inner.value && outer.value && inner.value->id != outer.value->id,
          "shadowed locals resolved to the same symbol identity");
  const auto inner_completion = shadowed_query.completions(at(shadowing, "print(altitude)") + 9);
  require(inner_completion.value && inner_completion.value->size() == 1 &&
              inner_completion.value->front().id ==
                  shadowed_query.symbol_at(at(shadowing, "print(altitude)") + 6).value->id,
          "completion did not select the innermost shadowed local");
  const auto outer_completion = shadowed_query.completions(at(shadowing, "return altitude") + 10);
  require(outer_completion.value && outer_completion.value->size() == 1 &&
              outer_completion.value->front().id == outer.value->id,
          "completion outside nested scope selected the wrong local");
  const auto before_declaration = shadowed_query.completions(at(shadowing, "let altitude"));
  require(before_declaration.value &&
              std::none_of(before_declaration.value->begin(), before_declaration.value->end(),
                           [](const auto &item) { return item.label == "altitude"; }),
          "completion exposed a local before its declaration");

  const std::string nested_calls =
      "fun twice(value: Int): Int => value + value\n"
      "fun add(left: Int, right: Int): Int => left + right\n"
      "fun main(): Int => add(twice(20), 2)\n";
  const source::document_snapshot nested_document(
      source::document_identity{source::document_id{83}, source::document_uri{"untitled:nested-calls"}, {}},
      2, nested_calls);
  const auto nested_index = language_service::index_document(nested_document);
  require(nested_index.value && nested_index.value->types,
          "nested call fixture did not retain resolved call metadata");
  const language_service::document_queries nested_query(nested_document, nested_index.value->index,
                                                          nullptr, &nested_index.value->model,
                                                          &*nested_index.value->types);
  const auto inner_signature = nested_query.signature_help(at(nested_calls, "twice(20)") + 7);
  require(inner_signature.value && inner_signature.value->label.starts_with("twice(") &&
              inner_signature.value->active_parameter == 0,
          "signature help did not choose the innermost call");
  const auto second_signature = nested_query.signature_help(at(nested_calls, ", 2)") + 2);
  require(second_signature.value && second_signature.value->label.starts_with("add(") &&
              second_signature.value->active_parameter == 1 &&
              second_signature.value->parameter_types.size() == 2,
          "signature help did not track the second argument of the outer call");
  const auto incoming_calls = nested_query.call_hierarchy(at(nested_calls, "twice(value"));
  require(incoming_calls.value && incoming_calls.value->incoming.size() == 1 &&
              incoming_calls.value->incoming.front().caller.name == "main",
          "call hierarchy did not find a function caller");
  const auto outgoing_calls = nested_query.call_hierarchy(at(nested_calls, "main():"));
  require(outgoing_calls.value && outgoing_calls.value->outgoing.size() == 2,
          "call hierarchy did not find both nested calls");
  const auto call_hints = nested_query.inlay_hints({0, static_cast<source::byte_offset>(nested_calls.size())});
  require(call_hints.value &&
              std::any_of(call_hints.value->begin(), call_hints.value->end(),
                          [](const auto &hint) { return hint.label == "value:"; }) &&
              std::any_of(call_hints.value->begin(), call_hints.value->end(),
                          [](const auto &hint) { return hint.label == "right:"; }),
          "resolved calls did not expose parameter-name hints");

  const std::string incomplete_calls =
      "fun add(left: Int, right: Int): Int => left + right\n"
      "fun main(): Int {\n  return add(1, ";
  const source::document_snapshot incomplete_document(
      source::document_identity{source::document_id{84}, source::document_uri{"untitled:incomplete-call"}, {}},
      3, incomplete_calls);
  const auto incomplete_index = language_service::index_document(incomplete_document);
  require(incomplete_index.value.has_value(), "incomplete call lost recoverable declarations");
  const language_service::document_queries incomplete_query(incomplete_document,
                                                              incomplete_index.value->index,
                                                              nullptr, &incomplete_index.value->model);
  const auto recovered_signature = incomplete_query.signature_help(
      static_cast<source::byte_offset>(incomplete_calls.size()));
  require(recovered_signature.value && recovered_signature.state == diagnostics::result_state::recovered &&
              recovered_signature.value->active_parameter == 1 &&
              recovered_signature.value->parameter_names.size() == 2,
          "signature help did not survive a partially typed call");

  const std::string overloaded_calls =
      "fun pick(number: Int): Int => number\n"
      "fun pick(message: String): String => message\n"
      "fun main(): Int => pick(41)\n";
  const source::document_snapshot overloaded_document(
      source::document_identity{source::document_id{85}, source::document_uri{"untitled:overloads"}, {}},
      1, overloaded_calls);
  const auto overloaded_index = language_service::index_document(overloaded_document);
  require(overloaded_index.value && overloaded_index.value->types,
          "overload fixture lost strict type metadata");
  const language_service::document_queries overloaded_query(overloaded_document,
                                                               overloaded_index.value->index,
                                                               nullptr, &overloaded_index.value->model,
                                                               &*overloaded_index.value->types);
  const auto overload_help = overloaded_query.signature_help(at(overloaded_calls, "pick(41)") + 5);
  require(overload_help.value && overload_help.value->alternatives.size() == 2 &&
              overload_help.value->parameter_names == std::vector<std::string>{"number"},
          "signature help did not expose compiler-indexed overload alternatives");

  const std::string documented_source =
      "/// Summarizes an altitude.\n"
      "/// Additional detail.\n"
      "/// @param height measured altitude\n"
      "/// @return adjusted altitude\n"
      "/// @example rise(1)\n"
      "/// @deprecated use climb instead\n"
      "/// @since 0.77.0\n"
      "fun rise(height: Int): Int => height + 1\n"
      "fun main(): Int => rise(2)\n";
  const source::document_snapshot documented(
      source::document_identity{source::document_id{88}, source::document_uri{"untitled:documented"}, {}},
      1, documented_source);
  const auto documented_index = language_service::index_document(documented);
  require(documented_index.value.has_value(), "documented fixture did not index");
  const language_service::document_queries documented_query(documented, documented_index.value->index,
                                                              nullptr, &documented_index.value->model);
  const auto documentation = documented_query.documentation_at(at(documented_source, "rise(height"));
  require(documentation.value && documentation.value->summary == "Summarizes an altitude." &&
              documentation.value->detail == "Additional detail." &&
              documentation.value->parameters.size() == 1 &&
              documentation.value->parameters.front().name == "height" &&
              documentation.value->returns == "adjusted altitude" &&
              documentation.value->examples == std::vector<std::string>{"rise(1)"} &&
              documentation.value->deprecated && documentation.value->availability == "0.77.0",
          "structured source documentation metadata was not preserved");
  const auto documented_classes = documented_query.semantic_classifications();
  require(documented_classes.value &&
              std::any_of(documented_classes.value->begin(), documented_classes.value->end(),
                          [&](const auto &entry)
                          {
                            return entry.range.bytes.begin == at(documented_source, "rise(2)") &&
                                   entry.deprecated;
                          }),
          "deprecated reference was not classified from compiler documentation");

  const std::string keyword_source = "fun main(): Int {\n  \n  return 0\n}\n";
  const source::document_snapshot keyword_document(
      source::document_identity{source::document_id{86}, source::document_uri{"untitled:keywords"}, {}},
      1, keyword_source);
  const auto keyword_index = language_service::index_document(keyword_document);
  require(keyword_index.value.has_value(), "keyword fixture did not index");
  const language_service::document_queries keyword_query(keyword_document, keyword_index.value->index,
                                                           nullptr, &keyword_index.value->model);
  const auto body_keywords = keyword_query.completions(at(keyword_source, "  \n") + 2);
  require(body_keywords.value &&
              std::any_of(body_keywords.value->begin(), body_keywords.value->end(),
                          [](const auto &item) { return item.label == "let"; }) &&
              std::none_of(body_keywords.value->begin(), body_keywords.value->end(),
                           [](const auto &item) { return item.label == "class"; }),
          "keyword completion ignored the function-body context");

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
  const auto searched = language_service::search_workspace_symbols(workspace_index, "calculate");
  require(searched.size() == 1 && searched.front().module == "guidance" &&
              searched.front().name == "calculate" &&
              language_service::search_workspace_symbols(workspace_index, "calculate", 0).empty(),
          "workspace symbol search did not return deterministic source declarations");
  const auto links = workspace_query.import_links();
  require(links.value && links.value->size() == 2 &&
              links.value->front().target.value.find("guidance.sagan") != std::string::npos,
          "resolved imports did not produce document links");
  const auto builtin_docs = workspace_query.documentation_at(at(std::string(source.value->text()), "print"));
  require(builtin_docs.value && builtin_docs.value->summary.starts_with("Writes a value") &&
              builtin_docs.value->parameters.size() == 1 &&
              builtin_docs.value->module == "sagan/core",
          "built-in documentation was not available through the shared catalog");
  const auto module_completion = workspace_query.completions(at(std::string(source.value->text()),
                                                              "from guidance") + 6);
  require(module_completion.value && module_completion.value->size() == 1 &&
              module_completion.value->front().label == "guidance",
          "import completion did not use workspace modules");
  const auto namespace_completion = workspace_query.completions(at(std::string(source.value->text()),
                                                                 "flight_data.offset()") + 13);
  require(namespace_completion.value && namespace_completion.value->size() == 1 &&
              namespace_completion.value->front().label == "offset" &&
              namespace_completion.value->front().detail == "(): Int",
          "namespace completion did not use exported members");
  const std::string unimported = "module scratch\n\nfun main(): Int {\n  return 0\n}\n";
  const source::document_snapshot unimported_document(
      source::document_identity{source::document_id{87}, source::document_uri{"untitled:unimported"}, {}},
      1, unimported);
  const auto unimported_index = language_service::index_document(unimported_document);
  require(unimported_index.value.has_value(), "unimported fixture did not index");
  const language_service::document_queries unimported_query(unimported_document,
                                                              unimported_index.value->index,
                                                              &workspace_index,
                                                              &unimported_index.value->model);
  const auto auto_imports = unimported_query.completions(at(unimported, "return 0"));
  require(auto_imports.value &&
              std::any_of(auto_imports.value->begin(), auto_imports.value->end(),
                          [](const auto &item)
                          {
                            return item.label == "course" && item.additional_import_edits.size() == 1 &&
                                   item.additional_import_edits.front().replacement_utf8 ==
                                       "import course from guidance\n";
                          }),
          "exported workspace completion did not provide a missing-import edit");
  const auto collision_graph = modules::resolve(
      "tests/fixtures/modules/completion_collision/main.sagan", disk);
  const auto collision_workspace = semantic::build_workspace_index(collision_graph, disk);
  const auto collision_source = disk.read_path(
      "tests/fixtures/modules/completion_collision/main.sagan");
  require(collision_source.value.has_value(), "completion collision fixture was not readable");
  const auto collision_index = language_service::index_document(*collision_source.value);
  require(collision_index.value.has_value(), "completion collision fixture did not index");
  const language_service::document_queries collision_query(
      *collision_source.value, collision_index.value->index, &collision_workspace,
      &collision_index.value->model);
  const auto collision_items = collision_query.completions(
      at(std::string(collision_source.value->text()), "return 0"));
  require(collision_items.value.has_value(), "collision completion did not return candidates");
  std::vector<std::string> answer_modules;
  for (const auto &item : *collision_items.value)
    if (item.label == "answer" && item.additional_import_edits.size() == 1)
    {
      answer_modules.push_back(item.source_module);
      require(item.additional_import_edits.front().replacement_utf8 ==
                  "import answer from " + item.source_module + "\n",
              "colliding export used an ambiguous import edit");
    }
  require(answer_modules == std::vector<std::string>{"alpha", "beta"},
          "colliding exports did not remain distinct and deterministic");
  return 0;
}
