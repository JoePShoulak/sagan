#include "../src/language_service/language_service.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{
  auto find_named(const semantic::semantic_index &index, const std::string &name,
                  const semantic::symbol_kind kind) -> const semantic::indexed_symbol *
  {
    for (const auto &entry : index.symbols())
      if (entry.name == name && entry.kind == kind) return &entry;
    return nullptr;
  }
}

auto main() -> int
{
  using namespace sagan;
  const std::string text =
      "module launch\n"
      "fun orbit(value: Int): Int => value + 1\n"
      "fun orbit(value: Float): Float => value + 1.0\n"
      "fun main(): Int {\n"
      "  let altitude = orbit(41)\n"
      "  if altitude > 0 {\n"
      "    let altitude = 7\n"
      "    print(altitude)\n"
      "  }\n"
      "  return altitude\n"
      "}\n";
  const source::document_snapshot document(
      source::document_identity{source::document_id{42}, source::document_uri{"file:///semantic-index-demo.sagan"}, {}},
      7, text);
  const auto result = language_service::index_document(document);
  if (result.state != diagnostics::result_state::complete || !result.value)
    throw std::runtime_error("semantic indexing did not complete");

  const auto &index = result.value->index;
  const auto repeated = language_service::index_document(document);
  if (!repeated.value || repeated.value->index.symbols().size() != index.symbols().size())
    throw std::runtime_error("repeated indexing changed the symbol set");
  std::unordered_set<std::string> orbit_ids;
  std::unordered_set<std::string> altitude_ids;
  for (const auto &entry : index.symbols())
  {
    if (entry.name == "orbit" && entry.kind == semantic::symbol_kind::function) orbit_ids.insert(entry.id.value);
    if (entry.name == "altitude" && entry.kind == semantic::symbol_kind::variable)
      altitude_ids.insert(entry.id.value);
  }
  if (orbit_ids.size() != 2 || altitude_ids.size() != 2)
    throw std::runtime_error("overloads and shadowed locals need distinct identities");

  const auto *main_symbol = find_named(index, "main", semantic::symbol_kind::function);
  if (!main_symbol || !index.definition(main_symbol->id))
    throw std::runtime_error("function definition is not indexed");
  const auto *repeated_main = find_named(repeated.value->index, "main", semantic::symbol_kind::function);
  if (!repeated_main || repeated_main->id != main_symbol->id)
    throw std::runtime_error("stable source produced a different symbol identity");

  const source::document_snapshot other_document(
      source::document_identity{source::document_id{43}, source::document_uri{"file:///other.sagan"}, {}},
      1, "fun main(): Int => 0\n");
  const auto other = language_service::index_document(other_document);
  if (!other.value) throw std::runtime_error("second document was not indexed");
  const auto *other_main = find_named(other.value->index, "main", semantic::symbol_kind::function);
  const auto *print_symbol = find_named(index, "print", semantic::symbol_kind::function);
  const auto *other_print = find_named(other.value->index, "print", semantic::symbol_kind::function);
  if (!other_main || other_main->id == main_symbol->id)
    throw std::runtime_error("source symbols from different modules share an identity");
  if (!print_symbol || !other_print || print_symbol->id != other_print->id)
    throw std::runtime_error("built-in identities must be stable across modules");

  std::cout << "Semantic index demo\n"
            << "  document: " << index.document().uri.value << " @ version " << index.version() << '\n'
            << "  symbols: " << index.symbols().size() << '\n'
            << "  references: " << index.references().size() << '\n'
            << "  orbit overload identities: " << orbit_ids.size() << '\n'
            << "  shadowed altitude identities: " << altitude_ids.size() << '\n'
            << "  built-in identity stable across modules: true\n"
            << "  main: " << main_symbol->id.value << '\n';
  return 0;
}
