#include "language_service.hpp"

#include "../parser/lex.hpp"
#include "../parser/parse_error.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"
#include "../semantic/analyzer.hpp"
#include "../semantic/semantic_error.hpp"
#include "../semantic/type_checker.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sagan::language_service
{
  namespace
  {
    auto make_diagnostic(const source::document_snapshot &document, const diagnostics::phase owner,
                         const parser::span range, const std::string &message) -> diagnostics::diagnostic
    {
      const auto text_size = static_cast<int>(document.text().size());
      const auto begin = static_cast<source::byte_offset>(std::clamp(range.begin, 0, text_size));
      const auto end = static_cast<source::byte_offset>(
          std::clamp(range.end, static_cast<int>(begin), text_size));
      return diagnostics::diagnostic{
          std::string(diagnostics::default_code(owner)), diagnostics::severity::error, owner,
          source::source_range{document.identity().id, source::byte_range{begin, end}}, message, {}, {}, {}};
    }

    auto cancelled(const source::document_snapshot &document)
      -> diagnostics::analysis_result<check_summary>
    {
      return diagnostics::analysis_result<check_summary>{
          diagnostics::result_state::cancelled, {}, {}, document.version()};
    }
  }

  auto supported_capabilities() -> capabilities
  {
    return capabilities{diagnostics::schema_version, true, true, true, true, false, false, false};
  }

  auto capabilities_json() -> std::string
  {
    const auto value = supported_capabilities();
    const auto boolean = [](const bool enabled) { return enabled ? "true" : "false"; };
    std::ostringstream output;
    output << "{\"schema\":\"" << value.schema << "\",\"positionEncodings\":[\"utf-16\",\"utf-8-bytes\"],"
           << "\"capabilities\":{\"strictDocumentCheck\":" << boolean(value.strict_document_check)
           << ",\"structuredDiagnostics\":" << boolean(value.structured_diagnostics)
           << ",\"utf16Positions\":" << boolean(value.utf16_positions)
           << ",\"cancellation\":" << boolean(value.cancellation)
           << ",\"recovery\":" << boolean(value.recovery)
           << ",\"documentOverlays\":" << boolean(value.document_overlays)
           << ",\"languageServer\":" << boolean(value.language_server) << "}}\n";
    return output.str();
  }

  auto check_document(const source::document_snapshot &document, const check_options options,
                      const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<check_summary>
  {
    check_summary summary;
    diagnostics::phase active_phase = diagnostics::phase::lexical;
    try
    {
      if (cancellation.is_cancelled()) return cancelled(document);
      parser::tokenizer lexer(parser::programText{std::string(document.text())}, get_token);
      std::vector<parser::token> tokens;
      while (auto next = lexer.next())
      {
        tokens.push_back(std::move(*next));
        if (cancellation.is_cancelled()) return cancelled(document);
      }
      summary.token_count = tokens.size();

      active_phase = diagnostics::phase::syntax;
      if (cancellation.is_cancelled()) return cancelled(document);
      parser::syntax_parser syntax(std::move(tokens));
      const auto tree = syntax.parse();
      summary.statement_count = tree.statements.size();

      active_phase = diagnostics::phase::semantic;
      if (cancellation.is_cancelled()) return cancelled(document);
      const auto semantic_model = semantic::analyze(tree);
      summary.scope_count = semantic_model.scopes.size();

      if (options.check_types)
      {
        active_phase = diagnostics::phase::type;
        if (cancellation.is_cancelled()) return cancelled(document);
        const auto type_model = semantic::check_types(tree);
        summary.typed_expression_count = type_model.expressions.size();
      }

      if (options.check_entry_point)
      {
        active_phase = diagnostics::phase::entry_point;
        if (cancellation.is_cancelled()) return cancelled(document);
        semantic::validate_entry_point(tree);
      }

      return diagnostics::analysis_result<check_summary>{
          diagnostics::result_state::complete, summary, {}, document.version()};
    }
    catch (const parser::parse_error &error)
    {
      return diagnostics::analysis_result<check_summary>{
          diagnostics::result_state::incomplete, {},
          {make_diagnostic(document, active_phase, error.range, error.what())}, document.version()};
    }
    catch (const semantic::semantic_error &error)
    {
      return diagnostics::analysis_result<check_summary>{
          diagnostics::result_state::incomplete, {},
          {make_diagnostic(document, active_phase, error.range, error.what())}, document.version()};
    }
  }
}

