#include "../src/diagnostics/diagnostic.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/language_service/workspace.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/parser/parser.hpp"
#include "../src/semantic/semantic_error.hpp"
#include "../src/semantic/units.hpp"
#include "../src/source/provider.hpp"
#include "../src/source/source.hpp"
#include "../src/syntax/syntax.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace
{
  auto check(const bool condition, const std::string &name) -> bool
  {
    if (condition) { std::cout << "[PASS] " << name << '\n'; return true; }
    std::cerr << "[FAIL] " << name << '\n';
    return false;
  }
}

auto main() -> int
{
  using namespace sagan;
  bool passed = true;
  const std::string text = "let 🚀 = 1\r\nprint(🚀)\n";
  source::document_snapshot document(
      source::document_identity{source::document_id{7}, source::document_uri{"file:///demo.sagan"}, {}},
      42, text);

  passed &= check(document.lines().line_count() == 3, "line count includes trailing line");
  const auto rocket = static_cast<source::byte_offset>(text.find("🚀"));
  passed &= check(document.to_utf16(rocket) == source::utf16_position{0, 4}, "byte to UTF-16 before emoji");
  passed &= check(document.to_utf16(rocket + 4) == source::utf16_position{0, 6}, "emoji uses UTF-16 surrogate pair");
  passed &= check(document.to_byte(source::utf16_position{0, 6}) == rocket + 4, "UTF-16 to byte after emoji");
  passed &= check(!document.to_byte(source::utf16_position{0, 5}).has_value(), "reject split surrogate position");
  passed &= check(document.to_utf16(static_cast<source::byte_offset>(text.find("print"))) ==
                      source::utf16_position{1, 0}, "CRLF starts next line once");
  const source::line_index carriage_lines("a\rb");
  passed &= check(carriage_lines.line_count() == 2 &&
                      carriage_lines.to_utf16("a\rb", 2) == source::utf16_position{1, 0},
                  "carriage return starts a new line");
  passed &= check(!document.to_utf16(rocket + 1).has_value(), "reject byte offset inside UTF-8 code point");
  passed &= check(!document.to_byte(source::utf16_position{99, 0}).has_value(), "reject missing line");

  diagnostics::cancellation_source cancellation;
  passed &= check(!cancellation.token().is_cancelled(), "cancellation begins clear");
  cancellation.cancel();
  bool parser_cancelled = false;
  try
  {
    parser::syntax_parser strict({}, cancellation.token());
    static_cast<void>(strict.parse());
  }
  catch (const parser::parse_cancelled &)
  { parser_cancelled = true; }
  passed &= check(parser_cancelled, "strict parser honors a cancellation token");
  passed &= check(cancellation.token().is_cancelled(), "cancellation propagates");

  diagnostics::diagnostic value{
      "SAG-SYN-0001", diagnostics::severity::error, diagnostics::phase::syntax,
      source::source_range{source::document_id{7}, source::byte_range{rocket, rocket + 4}},
      "example \"message\"",
      {{source::source_range{source::document_id{7}, source::byte_range{0, 3}}, "related declaration"}},
      {"example note"},
      {{"replace example", {{source::source_range{source::document_id{7}, source::byte_range{rocket, rocket + 4}},
                              "replacement"}}}}};
  const std::string json = diagnostics::render_json(document, diagnostics::result_state::recovered, {value});
  passed &= check(json.contains("\"schema\":\"sagan.language-service/1\"") &&
                      json.contains("\"version\":42") && json.contains("\"line\":0") &&
                      json.contains("\"character\":4") && json.contains("example \\\"message\\\"") &&
                      json.contains("related declaration") && json.contains("replace example") &&
                      json.contains("replacement"),
                  "structured diagnostic JSON");
  std::ostringstream terminal;
  diagnostics::render_terminal(terminal, document, value);
  passed &= check(terminal.str().contains("error[SAG-SYN-0001]: example \"message\"") &&
                      terminal.str().contains("--> file:///demo.sagan:1:5") &&
                      terminal.str().contains("related declaration") &&
                      terminal.str().contains("note: example note") &&
                      terminal.str().contains("help: replace example"),
                  "terminal diagnostic presentation");
  std::ostringstream build_terminal;
  diagnostics::render_terminal(build_terminal, document,
      {"SAG-BLD-0001", diagnostics::severity::error, diagnostics::phase::build,
       {document.identity().id, {}}, "Native compiler unavailable", {}, {}, {}});
  passed &= check(build_terminal.str().contains("error[SAG-BLD-0001]: Native compiler unavailable") &&
                      !build_terminal.str().contains(" --> "),
                  "unmapped build failure does not invent a source location");

  source::document_snapshot valid_document(
      source::document_identity{source::document_id{8}, source::document_uri{"file:///valid.sagan"}, {}},
      9, "fun value(): Int => 42\n");
  const auto valid = language_service::check_document(valid_document);
  passed &= check(valid.state == diagnostics::result_state::complete && valid.value.has_value() &&
                      valid.analyzed_version == 9 && valid.value->statement_count == 1,
                  "reusable document check service");

  source::document_snapshot invalid_document(
      source::document_identity{source::document_id{9}, source::document_uri{"file:///invalid.sagan"}, {}},
      10, "fun value(): Bool => 42\n");
  const auto invalid = language_service::check_document(invalid_document);
  passed &= check(invalid.state == diagnostics::result_state::incomplete && !invalid.value.has_value() &&
                      invalid.diagnostics.size() == 1 && invalid.diagnostics.front().code == "SAG-TYP-0001" &&
                      invalid.analyzed_version == 10,
                  "service returns structured type diagnostic");

  source::document_snapshot equivalent_units_document(
      source::document_identity{source::document_id{11}, source::document_uri{"file:///equivalent-units.sagan"}, {}},
      1,
      "fun accept(value: Vector3<Float64, kilometer / second^2>): Void {}\n"
      "fun apply(force: Vector3<Float, newton>, mass: Float<kilogram>): Void { accept(force / mass) }\n");
  const auto equivalent_units = language_service::check_document(equivalent_units_document);
  passed &= check(equivalent_units.state == diagnostics::result_state::complete &&
                      equivalent_units.value.has_value() && equivalent_units.diagnostics.empty(),
                  "derived equivalent units normalize default Float aliases");

  source::document_snapshot dimensionless_ratio_document(
      source::document_identity{source::document_id{12}, source::document_uri{"file:///dimensionless-ratio.sagan"}, {}},
      1,
      "fun sphere(mass: Float<kilogram>, parent: Float<kilogram>): Float<kilometer> {\n"
      "  let ratio: Float = mass / parent\n"
      "  return 1 kilometer * (ratio ^ (2.0 / 5.0))\n"
      "}\n");
  const auto dimensionless_ratio = language_service::check_document(dimensionless_ratio_document);
  passed &= check(dimensionless_ratio.state == diagnostics::result_state::complete &&
                      dimensionless_ratio.value.has_value() && dimensionless_ratio.diagnostics.empty(),
                  "equal measured units cancel to a dimensionless scalar");

  source::document_snapshot vector_scalar_document(
      source::document_identity{source::document_id{13}, source::document_uri{"file:///vector-scalar.sagan"}, {}},
      1,
      "fun scale(direction: Vector3<Float, kilometer>, ratio: Float): Vector3<Float, kilometer> {\n"
      "  let forward = direction * ratio\n"
      "  let reverse = ratio * direction\n"
      "  return (forward + reverse) / ratio\n"
      "}\n");
  const auto vector_scalar = language_service::check_document(vector_scalar_document);
  passed &= check(vector_scalar.state == diagnostics::result_state::complete &&
                      vector_scalar.value.has_value() && vector_scalar.diagnostics.empty(),
                  "dimensionless scalars scale measured vectors in both orders");

  diagnostics::cancellation_source pre_cancelled;
  pre_cancelled.cancel();
  const auto stopped = language_service::check_document(valid_document, {}, pre_cancelled.token());
  passed &= check(stopped.state == diagnostics::result_state::cancelled && stopped.diagnostics.empty(),
                  "service honors cancellation");
  const auto capabilities = language_service::capabilities_json();
  passed &= check(capabilities.contains("\"schema\":\"sagan.language-service/1\"") &&
                      capabilities.contains("\"structuredDiagnostics\":true") &&
                      capabilities.contains("\"recovery\":true") &&
                      capabilities.contains("\"documentOverlays\":true") &&
                      capabilities.contains("\"languageServer\":true") &&
                      capabilities.contains("\"documentationCatalog\":\"sagan-documentation-v1\"") &&
                      capabilities.contains("\"sourceEditsSchema\":\"sagan-source-edits-v1\"") &&
                      capabilities.contains("\"formattingSchema\":\"sagan-formatting-v1\"") &&
                      capabilities.contains("\"recoveredFormatting\":true") &&
                      capabilities.contains("\"operationsSchema\":\"sagan-operations-v2\"") &&
                      capabilities.contains("\"sourceMapSchema\":\"sagan-cpp-source-map-v1\"") &&
                      capabilities.contains("\"debugMetadataSchema\":\"sagan-debug-metadata-v1\"") &&
                      capabilities.contains("\"breakpointMappingSchema\":\"sagan-dap-breakpoints-v1\"") &&
                      capabilities.contains("\"packageCatalogSchema\":\"sagan-package-catalog-v1\"") &&
                      capabilities.contains("\"debugAdapterExecutable\":\"sagan-dap") &&
                      capabilities.contains("\"nativeBuild\":true") &&
                      capabilities.contains("\"nativeRun\":true") &&
                      capabilities.contains("\"debugAttach\":false") &&
                      capabilities.contains("\"debugAdapter\":false") &&
                      capabilities.contains("\"debugBreakpoints\":false") &&
                      capabilities.contains("\"testDocumentDiscovery\":true") &&
                      capabilities.contains("\"testProjectDiscovery\":true") &&
                      capabilities.contains("\"testDocumentRun\":true") &&
                      capabilities.contains("\"testProjectRun\":true") &&
                      capabilities.contains("\"packageIndexReader\":true") &&
                      capabilities.contains("\"packageQuery\":true") &&
                      capabilities.contains("\"packageCatalog\":true") &&
                      capabilities.contains("\"packageCompletion\":false") &&
                      capabilities.contains("\"operationTransport\":true") &&
                      capabilities.contains("\"operationCancellation\":true"),
                  "versioned capability discovery");
  passed &= check(diagnostics::default_code(diagnostics::phase::runtime) == "SAG-RUN-0001" &&
                      diagnostics::state_name(diagnostics::result_state::stale) == "stale" &&
                      diagnostics::severity_name(diagnostics::severity::hint) == "hint",
                  "stable diagnostic vocabulary");
  const diagnostics::phase phases[] = {
      diagnostics::phase::lexical, diagnostics::phase::syntax, diagnostics::phase::semantic,
      diagnostics::phase::type, diagnostics::phase::module, diagnostics::phase::project,
      diagnostics::phase::build, diagnostics::phase::entry_point, diagnostics::phase::runtime};
  bool diagnostic_names_complete = true;
  for (const auto phase : phases)
    diagnostic_names_complete &= diagnostics::phase_name(phase) != "unknown" &&
                                 diagnostics::default_code(phase) != "SAG-UNK-0001";
  const diagnostics::severity severities[] = {
      diagnostics::severity::error, diagnostics::severity::warning,
      diagnostics::severity::information, diagnostics::severity::hint};
  for (const auto severity : severities)
    diagnostic_names_complete &= diagnostics::severity_name(severity) != "unknown";
  const diagnostics::result_state states[] = {
      diagnostics::result_state::complete, diagnostics::result_state::recovered,
      diagnostics::result_state::incomplete, diagnostics::result_state::cancelled,
      diagnostics::result_state::stale};
  for (const auto state : states)
    diagnostic_names_complete &= diagnostics::state_name(state) != "unknown";
  diagnostic_names_complete &= diagnostics::phase_name(static_cast<diagnostics::phase>(999)) == "unknown" &&
                               diagnostics::default_code(static_cast<diagnostics::phase>(999)) == "SAG-UNK-0001" &&
                               diagnostics::severity_name(static_cast<diagnostics::severity>(999)) == "unknown" &&
                               diagnostics::state_name(static_cast<diagnostics::result_state>(999)) == "unknown";
  passed &= check(diagnostic_names_complete, "complete diagnostic enum vocabulary");

  const auto throws = [](auto action)
  {
    try { action(); }
    catch (...) { return true; }
    return false;
  };
  const parser::span unit_range{0, 1};
  semantic::units::registry unit_registry;
  const auto meter = unit_registry.resolve("meter", unit_range);
  const auto kilometer = unit_registry.resolve("kilometer", unit_range);
  const auto speed = unit_registry.resolve("meter / second", unit_range);
  const auto inverse_area = unit_registry.resolve("meter ^ -2", unit_range);
  const auto acceleration = unit_registry.resolve("meter / second^2", unit_range);
  const auto grouped = unit_registry.resolve("(meter * meter) / second", unit_range);
  const auto compound_divisor = semantic::units::combine(
      unit_registry.resolve("meter^3", unit_range),
      unit_registry.resolve("kilogram * second^2", unit_range), '/', unit_registry);
  const auto reparsed_divisor = unit_registry.resolve(compound_divisor.name, unit_range);
  const auto newton = unit_registry.resolve("newton", unit_range);
  const auto watt = unit_registry.resolve("watt", unit_range);
  const auto delta_celsius = unit_registry.resolve("Delta<Celsius>", unit_range);
  passed &= check(kilometer.scale.decimal_exponent == 3 && speed.dimension.size() == 2 &&
                      inverse_area.dimension.at("Length") == -2 && inverse_area.name == "meter^-2" &&
                      acceleration.dimension.at("Time") == -2 &&
                      acceleration.name == "meter / second^2" && grouped.dimension.at("Length") == 2 &&
                      delta_celsius.kind == semantic::units::category::affine_difference &&
                      unit_registry.find("meter") && !unit_registry.find("missing") &&
                      unit_registry.dimension_dimensions("Length") &&
                      !unit_registry.dimension_dimensions("Missing") &&
                      unit_registry.quantity_dimensions("Speed") &&
                      !unit_registry.quantity_dimensions("Missing"),
                  "unit registry resolves names prefixes grouping powers and deltas");
  passed &= check(compound_divisor.name == "meter^3 / (kilogram * second^2)" &&
                      compound_divisor.dimension == reparsed_divisor.dimension &&
                      compound_divisor.scale == reparsed_divisor.scale &&
                      newton.dimension == unit_registry.resolve("kilogram * meter / second^2", unit_range).dimension &&
                      watt.dimension == unit_registry.resolve("kilogram * meter^2 / second^3", unit_range).dimension,
                  "compound divisors round-trip and named derived units retain dimensions");
  const auto measured = semantic::units::parse_measured_type("Float64<meter / second>", unit_registry, unit_range);
  passed &= check(measured && measured->numeric == "Float64" &&
                      semantic::units::format_type("Float64", meter) == "Float64<meter>" &&
                      !semantic::units::parse_measured_type("String<meter>", unit_registry, unit_range) &&
                      !semantic::units::parse_measured_type("Float64", unit_registry, unit_range) &&
                      semantic::units::compatible(meter, kilometer) &&
                      !semantic::units::compatible(meter, speed),
                  "unit type parsing formatting and compatibility");
  const auto normalized = semantic::units::rational{20, -40, 2, 1}.normalized();
  const auto decimal = semantic::units::rational{100, 1000}.normalized();
  const auto zero = semantic::units::rational{0, -10, 4, 2}.normalized();
  const auto product = semantic::units::multiply({2, 3, 1}, {3, 4, -1});
  const auto quotient = semantic::units::divide({2, 3}, {4, 5});
  const auto sum = semantic::units::add({1, 2}, {1, 3});
  passed &= check(normalized == semantic::units::rational{-1, 2, 2, 1} &&
                      decimal == semantic::units::rational{1, 1, -1, 0} &&
                      zero == semantic::units::rational{0, 1, 0, 0} &&
                      product == semantic::units::rational{1, 2, 0, 0} &&
                      quotient == semantic::units::rational{5, 6, 0, 0} &&
                      sum == semantic::units::rational{5, 6, 0, 0},
                  "exact unit ratios normalize multiply divide and add");
  passed &= check(
      throws([] { static_cast<void>(semantic::units::rational{1, 0}.normalized()); }) &&
      throws([] { static_cast<void>(semantic::units::add({1, 1, 1}, {1, 1, 2})); }) &&
      throws([] { static_cast<void>(semantic::units::multiply(
          {std::numeric_limits<std::int64_t>::max(), 1}, {2, 1})); }) &&
      throws([&] { static_cast<void>(unit_registry.resolve("missing", unit_range)); }) &&
      throws([&] { static_cast<void>(unit_registry.resolve("meter ^ 0", unit_range)); }) &&
      throws([&] { static_cast<void>(unit_registry.resolve("(meter", unit_range)); }) &&
      throws([&] { static_cast<void>(unit_registry.resolve("Delta<Celsius", unit_range)); }) &&
      throws([&] { static_cast<void>(unit_registry.resolve("meter )", unit_range)); }) &&
      throws([&] { static_cast<void>(semantic::units::difference_of(meter)); }) &&
      throws([&] { static_cast<void>(semantic::units::combine(
          unit_registry.resolve("Celsius", unit_range), meter, '*', unit_registry)); }),
      "unit registry rejects malformed incompatible and overflowing operations");

  source::document_snapshot path_document(
      source::identity_from_path(source::document_id{70}, std::filesystem::absolute("demo path.sagan")),
      2, "x");
  diagnostics::diagnostic escaped_value{
      "SAG-LEX-0001", diagnostics::severity::warning, diagnostics::phase::lexical,
      source::source_range{source::document_id{70}, source::byte_range{2, 3}},
      std::string{"quote\" slash\\ back\b form\f line\n return\r tab\t low\x01"}, {}, {}, {}};
  const auto escaped_json = diagnostics::render_json(
      path_document, diagnostics::result_state::cancelled, {escaped_value});
  passed &= check(escaped_json.contains("\\\" slash\\\\") && escaped_json.contains("\\b") &&
                      escaped_json.contains("\\f") && escaped_json.contains("\\n") &&
                      escaped_json.contains("\\r") && escaped_json.contains("\\t") &&
                      escaped_json.contains("\\u0001") && escaped_json.contains("demo path.sagan"),
                  "diagnostic JSON escapes control text and includes canonical paths");

  const std::string preserved_source =
      "// ordinary comment\n/** declaration docs */\nfun 🚀(): Int => 42  /* trailing */\n";
  source::document_snapshot preserved_document(
      source::document_identity{source::document_id{10}, source::document_uri{"file:///preserved.sagan"}, {}},
      11, preserved_source);
  auto preserved = syntax::analyze(preserved_document);
  std::string reconstructed;
  if (preserved.value)
  {
    for (const auto &token : preserved.value->tokens)
    {
      for (const auto &trivia : token.leading_trivia) reconstructed += trivia.text;
      reconstructed += token.source_text;
    }
    for (const auto &trivia : preserved.value->trailing_trivia) reconstructed += trivia.text;
  }
  passed &= check(preserved.state == diagnostics::result_state::complete &&
                      preserved.value && preserved.value->strict_ast &&
                      reconstructed == preserved_source,
                  "lossless syntax preserves comments and whitespace");
  bool saw_line_comment = false;
  bool saw_block_comment = false;
  if (preserved.value)
  {
    for (const auto &token : preserved.value->tokens)
      for (const auto &trivia : token.leading_trivia)
      {
        saw_line_comment |= trivia.kind == syntax::trivia_kind::line_comment;
        saw_block_comment |= trivia.kind == syntax::trivia_kind::block_comment;
      }
    for (const auto &trivia : preserved.value->trailing_trivia)
      saw_block_comment |= trivia.kind == syntax::trivia_kind::block_comment;
  }
  passed &= check(saw_line_comment && saw_block_comment, "lossless syntax classifies ordinary comments");

  const std::string partial_source =
      "fun good(): Int => 1\nfun bad(: Int => 2\nlet = 3\nfun later(): Int => 4\n";
  source::document_snapshot partial_document(
      source::document_identity{source::document_id{11}, source::document_uri{"file:///partial.sagan"}, {}},
      12, partial_source);
  auto partial = syntax::analyze(partial_document);
  passed &= check(partial.state == diagnostics::result_state::recovered && partial.value &&
                      !partial.value->strict_ast && partial.value->recovered_ast &&
                      partial.value->recovered_ast->statements.size() == 2 &&
                      partial.diagnostics.size() == 2,
                  "parser recovery keeps valid declarations and reports multiple errors");
  auto service_partial = language_service::analyze_document(partial_document);
  passed &= check(service_partial.state == diagnostics::result_state::recovered &&
                      service_partial.value && service_partial.value->statement_count == 2 &&
                      service_partial.diagnostics.size() == 2,
                  "language service exposes recovered syntax results");

  const std::string lexical_source = "let first = @\nlet second = ~\n";
  source::document_snapshot lexical_document(
      source::document_identity{source::document_id{12}, source::document_uri{"file:///lexical.sagan"}, {}},
      13, lexical_source);
  auto lexical = syntax::analyze(lexical_document);
  const auto lexical_count = std::count_if(lexical.diagnostics.begin(), lexical.diagnostics.end(), [](const auto &value)
  {
    return value.owner == diagnostics::phase::lexical;
  });
  passed &= check(lexical.state == diagnostics::result_state::recovered && lexical.value &&
                      lexical_count == 2,
                  "lexer recovery reports multiple malformed regions");

  auto strict_partial = syntax::analyze(partial_document, syntax::analysis_options{false, 64});
  passed &= check(strict_partial.state == diagnostics::result_state::incomplete && strict_partial.value &&
                      !strict_partial.value->recovered_ast && strict_partial.diagnostics.size() == 1,
                  "strict syntax analysis remains fail-fast");

  diagnostics::cancellation_source syntax_cancellation;
  syntax_cancellation.cancel();
  auto cancelled_syntax = syntax::analyze(preserved_document, {}, syntax_cancellation.token());
  passed &= check(cancelled_syntax.state == diagnostics::result_state::cancelled && !cancelled_syntax.value,
                  "syntax analysis honors cancellation");

  bool prefix_safe = true;
  for (std::size_t length = 0; length <= preserved_source.size(); ++length)
  {
    source::document_snapshot prefix_document(
        source::document_identity{source::document_id{20 + length}, source::document_uri{"untitled:prefix"}, {}},
        static_cast<source::document_version>(length), preserved_source.substr(0, length));
    const auto prefix = syntax::analyze(prefix_document, syntax::analysis_options{true, 8});
    prefix_safe &= prefix.state == diagnostics::result_state::complete ||
                   prefix.state == diagnostics::result_state::recovered;
    prefix_safe &= prefix.diagnostics.size() <= 8;
  }
  passed &= check(prefix_safe, "every cursor prefix produces a bounded syntax result");

  auto disk = std::make_shared<source::disk_source_provider>();
  auto overlays = std::make_shared<source::document_store>(disk);
  const source::document_uri untitled{"untitled:workspace-demo"};
  passed &= check(static_cast<bool>(overlays->open(untitled, 1, "let 🚀 = 1\n")),
                  "open unsaved document overlay");
  auto overlay_snapshot = overlays->read(untitled);
  const auto overlay_rocket = static_cast<source::byte_offset>(overlay_snapshot.value->text().find("🚀"));
  passed &= check(static_cast<bool>(overlays->change(
                      untitled, 1, 2,
                      {{{overlay_snapshot.value->identity().id, {overlay_rocket, overlay_rocket + 4}}, "answer"}})),
                  "apply versioned UTF-8 overlay edit");
  overlay_snapshot = overlays->read(untitled);
  passed &= check(overlay_snapshot && overlay_snapshot.value->version() == 2 &&
                      overlay_snapshot.value->text() == "let answer = 1\n",
                  "overlay read returns latest unsaved snapshot");
  passed &= check(!overlays->replace(untitled, 1, 3, "let stale = 0\n") &&
                      overlays->replace(untitled, 2, 3, "let answer = 2\n") &&
                      overlays->save(untitled, 3) && overlays->close(untitled),
                  "overlay lifecycle rejects stale versions and supports save close");

  const auto provider_root = std::filesystem::absolute("build/source-provider-test").lexically_normal();
  std::filesystem::remove_all(provider_root);
  std::filesystem::create_directories(provider_root);
  const auto provider_path = provider_root / "source file.sagan";
  {
    std::ofstream provider_file(provider_path);
    provider_file << "fun value(): Int => 1\n";
  }
  const auto provider_identity = source::identity_from_path(source::document_id{71}, provider_path);
  const auto disk_read = disk->read_path(provider_path);
  const auto disk_uri_read = disk->read(provider_identity.uri);
  passed &= check(disk->exists_path(provider_path) && disk_read && disk_uri_read &&
                      disk_read.value->identity().id == disk_uri_read.value->identity().id &&
                      disk_uri_read.value->text().contains("fun value"),
                  "disk provider reads paths and keeps stable identities");
  const auto invalid_scheme = disk->canonicalize(source::document_uri{"untitled:bad"});
  const auto invalid_percent = disk->canonicalize(source::document_uri{"file:///bad%QQ.sagan"});
  const auto missing_read = disk->read_path(provider_root / "missing.sagan");
  passed &= check(!invalid_scheme && invalid_scheme.error->code == source::provider_error_code::invalid_uri &&
                      !invalid_percent && invalid_percent.error->code == source::provider_error_code::invalid_uri &&
                      !missing_read && missing_read.error->code == source::provider_error_code::not_found &&
                      !disk->exists_path(provider_root / "missing.sagan"),
                  "disk provider reports invalid URIs and missing files");
  auto path_overlays = std::make_shared<source::document_store>(disk);
  auto alias_uri_text = provider_identity.uri.value;
  const auto alias_name = alias_uri_text.rfind("source%20file.sagan");
  if (alias_name != std::string::npos) alias_uri_text.replace(alias_name, 1, "%73");
  const source::document_uri alias_uri{alias_uri_text};
  passed &= check(path_overlays->open(provider_identity.uri, 1, "overlay") &&
                      path_overlays->read_path(provider_path).value->text() == "overlay" &&
                      !path_overlays->open(alias_uri, 1, "duplicate path") &&
                      path_overlays->canonicalize(provider_identity.uri).value ==
                          std::filesystem::absolute(provider_path).lexically_normal() &&
                      path_overlays->close(provider_identity.uri),
                  "file overlays win by canonical path and reject URI aliases");

  const source::document_uri provider_untitled{"untitled:provider-errors"};
  passed &= check(!overlays->open(provider_untitled, -1, "") &&
                      overlays->open(provider_untitled, 1, "abc") &&
                      !overlays->open(provider_untitled, 2, "duplicate") &&
                      overlays->is_open(provider_untitled) &&
                      overlays->current_version(provider_untitled) == 1,
                  "overlay rejects invalid versions and duplicate opens");
  passed &= check(!overlays->replace(source::document_uri{"untitled:closed"}, 1, 2, "") &&
                      !overlays->replace(provider_untitled, 0, 2, "") &&
                      !overlays->replace(provider_untitled, 1, 1, "") &&
                      !overlays->save(provider_untitled, 2) &&
                      !overlays->save(source::document_uri{"untitled:closed"}, 1),
                  "overlay rejects stale and closed replace-save operations");
  auto invalid_edit_snapshot = overlays->read(provider_untitled);
  const auto provider_document = invalid_edit_snapshot.value->identity().id;
  passed &= check(!overlays->change(provider_untitled, 0, 2, {}) &&
                      !overlays->change(provider_untitled, 1, 1, {}) &&
                      !overlays->change(source::document_uri{"untitled:closed"}, 1, 2, {}) &&
                      !overlays->change(provider_untitled, 1, 2,
                          {{{provider_document, {2, 1}}, "x"}}) &&
                      !overlays->change(provider_untitled, 1, 2,
                          {{{source::document_id{999}, {0, 1}}, "x"}}),
                  "overlay validates edit versions ranges and document identity");
  passed &= check(overlays->save(provider_untitled, 1, std::string{"saved"}) &&
                      overlays->read(provider_untitled).value->text() == "saved" &&
                      overlays->close(provider_untitled) &&
                      !overlays->close(provider_untitled) &&
                      !overlays->current_version(provider_untitled),
                  "overlay save replacement and close error paths");
  passed &= check(overlays->read(provider_identity.uri) && overlays->read_path(provider_path) &&
                      overlays->exists_path(provider_path) && overlays->canonicalize(provider_identity.uri),
                  "document store delegates closed documents to disk");
  std::filesystem::remove_all(provider_root);

  const auto workspace_root = std::filesystem::absolute("build/editor-workspace-test").lexically_normal();
  std::filesystem::remove_all(workspace_root);
  std::filesystem::create_directories(workspace_root);
  const auto main_path = workspace_root / "main.sagan";
  const auto support_path = workspace_root / "support.sagan";
  {
    std::ofstream main_file(main_path);
    main_file << "module main\nimport live from support\nfun main(): Int => live()\n";
    std::ofstream support_file(support_path);
    support_file << "module support\nfun disk(): Int => 1\nexport disk\n";
  }
  const auto main_identity = source::identity_from_path(source::document_id{100}, main_path);
  const auto support_identity = source::identity_from_path(source::document_id{101}, support_path);
  passed &= check(static_cast<bool>(overlays->open(
                      support_identity.uri, 1, "module support\nfun live(): Int => 2\nexport live\n")),
                  "open module overlay");
  bool overlay_resolved = false;
  try
  {
    const auto graph = modules::resolve(main_path, *overlays);
    overlay_resolved = graph.modules.size() == 2 && graph.modules.front().name == "support" &&
                       graph.modules.front().exports.front().public_name == "live";
  }
  catch (...) {}
  passed &= check(overlay_resolved, "module resolver prefers unsaved overlay over disk");
  overlays->close(support_identity.uri);
  bool disk_rejected_live_import = false;
  try { static_cast<void>(modules::resolve(main_path, *overlays)); }
  catch (const std::runtime_error &error) { disk_rejected_live_import = std::string(error.what()).contains("does not export 'live'"); }
  passed &= check(disk_rejected_live_import, "closing overlay restores disk module view");
  std::filesystem::remove_all(workspace_root);

  language_service::workspace workspace(overlays);
  const source::document_uri dependency_uri{"untitled:dependency"};
  const source::document_uri dependent_uri{"untitled:dependent"};
  passed &= check(workspace.open(dependency_uri, 1, "fun value(): Int => 1\n") &&
                      workspace.open(dependent_uri, 1, "fun result(): Int => 2\n"),
                  "workspace tracks multiple open documents");
  workspace.set_dependencies(dependent_uri, {dependency_uri});
  auto superseded = workspace.begin_analysis(dependent_uri);
  auto replacement_request = workspace.begin_analysis(dependent_uri);
  auto superseded_result = language_service::analyze_document(superseded.value->document);
  superseded_result = workspace.finish_analysis(*superseded.value, std::move(superseded_result));
  passed &= check(superseded.value->cancellation.is_cancelled() && replacement_request &&
                      superseded_result.state == diagnostics::result_state::stale,
                  "new request cancels and supersedes older analysis for the same snapshot");
  auto pending_dependent = workspace.begin_analysis(dependent_uri);
  passed &= check(pending_dependent && workspace.replace(dependency_uri, 1, 2, "fun value(): Int => 3\n") &&
                      pending_dependent.value->cancellation.is_cancelled(),
                  "dependency edits cancel obsolete dependent analysis");
  auto obsolete_result = language_service::analyze_document(pending_dependent.value->document);
  obsolete_result = workspace.finish_analysis(*pending_dependent.value, std::move(obsolete_result));
  passed &= check(obsolete_result.state == diagnostics::result_state::stale && !obsolete_result.value &&
                      obsolete_result.diagnostics.empty(),
                  "workspace rejects stale analysis publication");
  const auto current_result = workspace.analyze(dependent_uri);
  passed &= check(current_result.state == diagnostics::result_state::complete &&
                      current_result.analyzed_version == 1,
                  "workspace analyzes current snapshot after invalidation");
  workspace.close(dependent_uri);
  workspace.close(dependency_uri);

  auto lifecycle_documents = std::make_shared<source::document_store>(disk);
  language_service::workspace lifecycle(lifecycle_documents);
  const source::document_uri first_uri{"untitled:lifecycle-first"};
  const source::document_uri second_uri{"untitled:lifecycle-second"};
  passed &= check(!lifecycle.analyze(first_uri).value &&
                      lifecycle.analyze(first_uri).state == diagnostics::result_state::incomplete &&
                      !lifecycle.begin_analysis(first_uri) && !lifecycle.close(first_uri),
                  "workspace reports unavailable closed documents");
  passed &= check(lifecycle.open(first_uri, 1, "fun value(): Int => 1\n") &&
                      lifecycle.open(second_uri, 1, "fun other(): Int => 2\n") &&
                      lifecycle.documents().is_open(first_uri),
                  "workspace exposes its document store");
  lifecycle.set_dependencies(first_uri, {first_uri, second_uri});
  lifecycle.set_dependencies(second_uri, {first_uri});
  passed &= check(lifecycle.invalidate(first_uri) == 2,
                  "workspace invalidation terminates across dependency cycles");
  const auto first_snapshot = lifecycle_documents->read(first_uri);
  passed &= check(lifecycle.change(first_uri, 1, 2,
                      {{{first_snapshot.value->identity().id, {20, 21}}, "3"}}) &&
                      lifecycle.save(first_uri, 2, std::string{"fun value(): Int => 4\n"}),
                  "workspace changes and saves versioned overlays");
  const auto first_analysis = lifecycle.analyze(first_uri);
  const auto cached_analysis = lifecycle.analyze(first_uri);
  passed &= check(first_analysis.state == diagnostics::result_state::complete &&
                      cached_analysis.state == diagnostics::result_state::complete &&
                      cached_analysis.analyzed_version == 2,
                  "workspace caches current completed analysis");
  const auto no_types = language_service::check_document(
      valid_document, language_service::check_options{false, false});
  source::document_snapshot entry_document(
      source::document_identity{source::document_id{72}, source::document_uri{"untitled:entry-check"}, {}},
      1, "fun helper(): Int => 1\n");
  const auto entry_check = language_service::check_document(
      entry_document, language_service::check_options{true, true});
  diagnostics::cancellation_source cancelled_index_source;
  cancelled_index_source.cancel();
  const auto cancelled_index = language_service::index_document(
      valid_document, cancelled_index_source.token());
  const auto invalid_index = language_service::index_document(invalid_document);
  passed &= check(no_types.state == diagnostics::result_state::complete &&
                      no_types.value->typed_expression_count == 0 &&
                      entry_check.state == diagnostics::result_state::complete &&
                      entry_check.diagnostics.empty() &&
                      cancelled_index.state == diagnostics::result_state::cancelled &&
                      invalid_index.state == diagnostics::result_state::incomplete &&
                      invalid_index.diagnostics.front().owner == diagnostics::phase::semantic,
                  "language service accepts declaration-only root files");
  passed &= check(lifecycle.close(first_uri) && lifecycle.close(second_uri),
                  "workspace removes cyclic dependency relationships on close");

  return passed ? 0 : 1;
}
