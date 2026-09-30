#include "../src/language_service/language_service.hpp"
#include "../src/language_service/queries.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
  auto require(const bool condition, const char *message) -> void
  {
    if (!condition) throw std::runtime_error(message);
  }

  auto document(const std::string &text, const std::uint64_t id = 1)
    -> sagan::source::document_snapshot
  {
    return {{sagan::source::document_id{id}, sagan::source::document_uri{"untitled:constants"}, {}},
            1, text};
  }

  auto rejects(const std::string &text, const std::string_view expected) -> void
  {
    const auto checked = sagan::language_service::check_document(document(text));
    require(!checked.diagnostics.empty(), "invalid constant source unexpectedly passed");
    if (!std::any_of(checked.diagnostics.begin(), checked.diagnostics.end(),
                     [&](const auto &diagnostic) { return diagnostic.message.contains(expected); }))
      throw std::runtime_error("expected '" + std::string(expected) + "', got '" +
                               checked.diagnostics.front().message + "' in: " + text);
  }
}

auto main() -> int
{
  using namespace sagan;
  const std::string valid =
      "const SPEED_OF_LIGHT = 299_792_458\n"
      "fun main(): Int {\n"
      "  const DISTANCE = 100 meter\n"
      "  const TIME = 20 second\n"
      "  const SPEED = DISTANCE / TIME\n"
      "  let mutable = 1\n"
      "  mutable += 1\n"
      "  print(\"${SPEED}\")\n"
      "  print(SPEED_OF_LIGHT)\n"
      "  return mutable\n"
      "}\n";
  const auto snapshot = document(valid);
  require(language_service::check_document(snapshot).state == diagnostics::result_state::complete,
          "valid constants did not type-check");
  const auto indexed = language_service::index_document(snapshot);
  require(indexed.value.has_value(), "valid constants did not produce a semantic index");
  const auto *constant = indexed.value->index.symbol_at(valid.find("SPEED_OF_LIGHT"));
  require(constant && constant->kind == semantic::symbol_kind::constant,
          "constant declaration did not receive a distinct symbol kind");
  const language_service::document_queries queries(snapshot, indexed.value->index, nullptr,
                                                     &indexed.value->model);
  const auto classifications = queries.semantic_classifications();
  require(classifications.value &&
              std::any_of(classifications.value->begin(), classifications.value->end(),
                          [](const auto &entry)
                          { return entry.kind == semantic::symbol_kind::constant && entry.read_only; }),
          "constant was not classified read-only for editor tooling");
  const auto use = valid.find("print(SPEED_OF_LIGHT)") + 6;
  const auto definition = queries.definitions(use);
  require(definition.value && definition.value->size() == 1 &&
              definition.value->front().bytes.begin == valid.find("SPEED_OF_LIGHT"),
          "constant reference did not navigate to its declaration");
  require(queries.references(use, true).value->size() == 2 &&
              queries.hover(use).value->symbol.kind == semantic::symbol_kind::constant,
          "constant references or hover lost the immutable symbol kind");
  const auto completions = queries.completions(valid.find("return mutable"));
  require(completions.value &&
              std::any_of(completions.value->begin(), completions.value->end(),
                          [](const auto &item)
                          { return item.label == "SPEED" && item.kind == semantic::symbol_kind::constant; }),
          "constant completion candidate was not visible in its local scope");
  const std::string field_source = "class Orbit {\n  const RADIUS: Int = 3\n}\n";
  const auto field_index = language_service::index_document(document(field_source, 2));
  require(field_index.value &&
              std::any_of(field_index.value->index.symbols().begin(),
                          field_index.value->index.symbols().end(),
                          [](const auto &entry)
                          { return entry.name == "RADIUS" &&
                                   entry.kind == semantic::symbol_kind::constant_field; }),
          "constant class field did not receive a distinct symbol kind");
  require(semantic::rename_preserves_binding_convention(semantic::symbol_kind::constant,
                                                         "NEW_SPEED") &&
              !semantic::rename_preserves_binding_convention(semantic::symbol_kind::constant,
                                                              "new_speed") &&
              !semantic::rename_preserves_binding_convention(semantic::symbol_kind::variable,
                                                              "NEW_SPEED"),
          "rename naming gate did not preserve constant and variable conventions");

  rejects("const nope = 1\n", "SCREAMING_SNAKE_CASE");
  rejects("let MAX_RETRIES = 5\n", "reserved for const");
  rejects("const MAX_RETRIES: Int\n", "requires an initializer");
  rejects("let const = 1\n", "identifier after 'let'");
  rejects("fun main(): Int {\n  const VALUE: String = 1\n  return 0\n}\n", "initializer");
  rejects("const VALUE = 1\nconst VALUE = 2\n", "Duplicate declaration");
  rejects("let value = 1\nconst VALUE = 2\nconst VALUE = 3\n", "Duplicate declaration");

  for (const std::string operation : {"=", "+=", "-=", "*=", "/=", "%=", "^="})
    rejects("fun main(): Int {\n  const VALUE = 5\n  VALUE " + operation + " 1\n  return 0\n}\n",
            "cannot be mutated");
  rejects("fun main(): Int {\n  const VALUE = 5\n  VALUE++\n  return 0\n}\n", "cannot be mutated");
  rejects("fun main(): Int {\n  const VALUE = 5\n  ++VALUE\n  return 0\n}\n", "cannot be mutated");
  rejects("fun main(): Int {\n  const VALUE = 5\n  VALUE--\n  return 0\n}\n", "cannot be mutated");
  rejects("fun main(): Int {\n  const VALUE = 5\n  --VALUE\n  return 0\n}\n", "cannot be mutated");
  rejects("fun main(): Int {\n  const VALUES = [1]\n  VALUES[0] = 2\n  return 0\n}\n",
          "cannot be mutated");
  rejects("class Ship {\n  let position: Int = 0\n}\n"
          "fun main(): Int {\n  const SHIP = Ship()\n  SHIP.position = 1\n  return 0\n}\n",
          "cannot be mutated");
  rejects("class Ship {\n  fun accelerate!(): Int => 1\n}\n"
          "fun main(): Int {\n  const SHIP = Ship()\n  SHIP.accelerate!()\n  return 0\n}\n",
          "cannot be mutated");
  rejects("class Orbit {\n  const STANDARD_RADIUS: Int = 3\n}\n"
          "fun main(): Int {\n  let orbit = Orbit()\n  orbit.STANDARD_RADIUS = 4\n  return 0\n}\n",
          "Constant field");
  rejects("class Orbit {\n  const STANDARD_RADIUS: Int\n}\n",
          "requires an initializer");
  rejects("class Orbit {\n  const STANDARD_RADIUS = 3\n}\n",
          "requires a type annotation");
  rejects("class Orbit {\n  const STANDARD_RADIUS: Int = 3\n"
          "  new() {\n    self.STANDARD_RADIUS = 4\n  }\n}\n",
          "Constant field");
  return 0;
}
