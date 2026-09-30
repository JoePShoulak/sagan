#include "../src/language_service/language_service.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/semantic/workspace_index.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
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
      "/// Advances a value by one.\n"
      "fun orbit(value: Int): Int => value + 1\n"
      "fun orbit(value: Float): Float => value + 1.0\n"
      "fun identity<T>(value: T): T => value\n"
      "face Readable {\n"
      "  fun read(): Int\n"
      "}\n"
      "class Probe is Readable {\n"
      "  let serial = 7\n"
      "  fun .secret(): Int => 9\n"
      "  fun read(): Int => 1\n"
      "  fun use(): Int => self.read() + self.secret()\n"
      "}\n"
      "enum State {\n  ready\n  waiting\n}\n"
      "fun main(): Int {\n"
      "  let explicit = identity<Int>(1)\n"
      "  let inferred = identity(2)\n"
      "  let probe = Probe()\n"
      "  let reading = probe.read()\n"
      "  let state = State.ready\n"
      "  let altitude = 41\n"
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
  {
    for (const auto &diagnostic : result.diagnostics) std::cerr << diagnostic.message << '\n';
    throw std::runtime_error("semantic indexing did not complete");
  }

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
  if (index.overloads().size() != 1 || index.overloads().front().candidates.size() != 2)
    throw std::runtime_error("callable overload set was not indexed");
  bool documented_orbit = false;
  for (const auto &entry : index.symbols())
    if (entry.name == "orbit" && !entry.documentation.empty() &&
        entry.documentation.front() == "Advances a value by one.") documented_orbit = true;
  if (!documented_orbit) throw std::runtime_error("declaration documentation was not indexed");
  if (index.specializations().size() != 1 || index.specializations().front().arguments !=
                                                  std::vector<std::string>{"Int"})
    throw std::runtime_error("explicit generic specialization was not indexed");
  if (index.inferred_specializations().size() != 1 ||
      index.inferred_specializations().front().arguments.empty() ||
      index.inferred_specializations().front().candidates.empty())
    throw std::runtime_error("inferred generic specialization was not indexed");
  if (index.conformances().size() != 1)
    throw std::runtime_error("face conformance was not indexed");
  if (index.types().empty() || index.typed_ranges().empty())
    throw std::runtime_error("typed semantic ranges were not indexed");
  const semantic::indexed_symbol *read_method = nullptr;
  for (const auto &entry : index.symbols())
    if (entry.name == "read" && entry.kind == semantic::symbol_kind::method &&
        !index.references_to(entry.id).empty()) read_method = &entry;
  if (!read_method)
    throw std::runtime_error("self member reference was not linked by identity");
  const auto *secret_method = find_named(index, "secret", semantic::symbol_kind::method);
  const auto *ready_case = find_named(index, "ready", semantic::symbol_kind::enum_case);
  if (!secret_method || secret_method->visibility != semantic::symbol_visibility::private_access ||
      index.references_to(secret_method->id).empty())
    throw std::runtime_error("private member reference was not linked by identity");
  if (!ready_case || index.references_to(ready_case->id).empty())
    throw std::runtime_error("enum case reference was not linked by identity");
  if (index.member_resolutions().empty() ||
      std::any_of(index.member_resolutions().begin(), index.member_resolutions().end(),
                  [](const auto &entry) { return entry.candidates.empty(); }))
    throw std::runtime_error("typed receiver members were not linked to declaration identities");

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

  const source::document_snapshot incomplete_document(
      source::document_identity{source::document_id{44}, source::document_uri{"untitled:incomplete"}, {}},
      3, "fun available(): Int => 42\nfun unfinished(\n");
  const auto incomplete = language_service::index_document(incomplete_document);
  if (incomplete.state != diagnostics::result_state::recovered || !incomplete.value ||
      !find_named(incomplete.value->index, "available", semantic::symbol_kind::function))
    throw std::runtime_error("recovering semantic index did not retain valid declarations");

  std::cout << "Semantic index demo\n"
            << "  document: " << index.document().uri.value << " @ version " << index.version() << '\n'
            << "  symbols: " << index.symbols().size() << '\n'
            << "  references: " << index.references().size() << '\n'
            << "  orbit overload identities: " << orbit_ids.size() << '\n'
            << "  overload sets: " << index.overloads().size() << '\n'
            << "  documentation attached: " << std::boolalpha << documented_orbit << '\n'
            << "  generic specializations: " << index.specializations().size() << '\n'
            << "  inferred generic specializations: " << index.inferred_specializations().size() << '\n'
            << "  face conformances: " << index.conformances().size() << '\n'
            << "  canonical types: " << index.types().size() << '\n'
            << "  typed ranges: " << index.typed_ranges().size() << '\n'
            << "  self member references: " << index.references_to(read_method->id).size() << '\n'
            << "  private member references: " << index.references_to(secret_method->id).size() << '\n'
            << "  enum case references: " << index.references_to(ready_case->id).size() << '\n'
            << "  typed receiver members: " << index.member_resolutions().size() << '\n'
            << "  shadowed altitude identities: " << altitude_ids.size() << '\n'
            << "  built-in identity stable across modules: true\n"
            << "  incomplete document indexed: recovered\n"
            << "  main: " << main_symbol->id.value << '\n';

  const source::disk_source_provider disk;
  const auto graph = modules::resolve("examples/module_demo/main.sagan", disk);
  const auto workspace_index = semantic::build_workspace_index(graph, disk);
  const auto course = workspace_index.exported("guidance", "course");
  if (course.size() != 1) throw std::runtime_error("export alias did not resolve to one declaration");
  const auto cross_module_references = workspace_index.references_to(course.front());
  if (cross_module_references.size() < 2)
    throw std::runtime_error("selective import references did not link across modules");
  const auto offset = workspace_index.exported("telemetry", "offset");
  if (offset.size() != 1 || workspace_index.references_to(offset.front()).size() < 2 ||
      workspace_index.external_references().empty())
    throw std::runtime_error("namespace member references did not link across modules");
  std::cout << "  indexed modules: " << workspace_index.modules().size() << '\n'
            << "  guidance.course target: " << course.front().value << '\n'
            << "  cross-module references: " << cross_module_references.size() << '\n';
  std::cout << "  namespace-member references: "
            << workspace_index.references_to(offset.front()).size() << '\n';
  return 0;
}
