#include "../src/diagnostics/diagnostic.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/source/source.hpp"
#include "../src/syntax/syntax.hpp"

#include <algorithm>
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
                      capabilities.contains("\"recovery\":true") &&
                      capabilities.contains("\"languageServer\":false"),
                  "versioned capability discovery");
  passed &= check(diagnostics::default_code(diagnostics::phase::runtime) == "SAG-RUN-0001" &&
                      diagnostics::state_name(diagnostics::result_state::stale) == "stale" &&
                      diagnostics::severity_name(diagnostics::severity::hint) == "hint",
                  "stable diagnostic vocabulary");

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

  return passed ? 0 : 1;
}
