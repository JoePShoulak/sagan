#include "../src/diagnostics/diagnostic.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/source/source.hpp"

#include <iostream>
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
  passed &= check(terminal.str().contains("syntax error at 1:5 [SAG-SYN-0001]"),
                  "terminal diagnostic presentation");

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

  diagnostics::cancellation_source pre_cancelled;
  pre_cancelled.cancel();
  const auto stopped = language_service::check_document(valid_document, {}, pre_cancelled.token());
  passed &= check(stopped.state == diagnostics::result_state::cancelled && stopped.diagnostics.empty(),
                  "service honors cancellation");
  const auto capabilities = language_service::capabilities_json();
  passed &= check(capabilities.contains("\"schema\":\"sagan.language-service/1\"") &&
                      capabilities.contains("\"structuredDiagnostics\":true") &&
                      capabilities.contains("\"recovery\":false") &&
                      capabilities.contains("\"languageServer\":false"),
                  "versioned capability discovery");
  passed &= check(diagnostics::default_code(diagnostics::phase::runtime) == "SAG-RUN-0001" &&
                      diagnostics::state_name(diagnostics::result_state::stale) == "stale" &&
                      diagnostics::severity_name(diagnostics::severity::hint) == "hint",
                  "stable diagnostic vocabulary");

  return passed ? 0 : 1;
}
