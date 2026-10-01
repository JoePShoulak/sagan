#include "language_service.hpp"
#include "documentation.hpp"
#include "operations.hpp"
#include "refactor.hpp"

#include "../parser/lex.hpp"
#include "../parser/parse_error.hpp"
#include "../parser/parser.hpp"
#include "../parser/tokenizer.hpp"
#include "../modules/resolver.hpp"
#include "../semantic/analyzer.hpp"
#include "../semantic/semantic_error.hpp"
#include "../semantic/type_checker.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sagan::language_service
{
  namespace
  {
    auto attach_closing_brace_fix(const source::document_snapshot &document,
                                  diagnostics::diagnostic &diagnostic) -> void
    {
      if (diagnostic.owner != diagnostics::phase::syntax ||
          (diagnostic.message != "Expected '}' after the block" &&
           diagnostic.message != "Expected '}' after the type body" &&
           diagnostic.message != "Expected '}' after the match cases")) return;
      const auto text = document.text();
      const std::string newline = text.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
      const bool ends_in_newline = !text.empty() && text.back() == '\n';
      const std::string insertion = (ends_in_newline ? "" : newline) + "}" + newline;
      const source::document_snapshot trial(document.identity(), document.version(),
                                            std::string(text) + insertion);
      const auto parsed = syntax::analyze(trial, {.recover = false});
      if (!parsed.value || !parsed.value->strict_ast) return;
      const auto at = static_cast<source::byte_offset>(text.size());
      diagnostic.fixes.push_back({"Insert missing '}'",
                                  {{{document.identity().id, {at, at}}, insertion}}});
    }

    auto make_diagnostic(const source::document_snapshot &document, const diagnostics::phase owner,
                         const parser::span range, const std::string &message) -> diagnostics::diagnostic
    {
      const auto text_size = static_cast<int>(document.text().size());
      const auto begin = static_cast<source::byte_offset>(std::clamp(range.begin, 0, text_size));
      const auto end = static_cast<source::byte_offset>(
          std::clamp(range.end, static_cast<int>(begin), text_size));
      auto diagnostic = diagnostics::diagnostic{
          std::string(diagnostics::default_code(owner)), diagnostics::severity::error, owner,
          source::source_range{document.identity().id, source::byte_range{begin, end}}, message, {}, {}, {}};
      attach_closing_brace_fix(document, diagnostic);
      return diagnostic;
    }

    auto cancelled(const source::document_snapshot &document)
      -> diagnostics::analysis_result<check_summary>
    {
      return diagnostics::analysis_result<check_summary>{
          diagnostics::result_state::cancelled, {}, {}, document.version()};
    }

    auto cancelled_index(const source::document_snapshot &document)
      -> diagnostics::analysis_result<semantic_snapshot>
    {
      return {diagnostics::result_state::cancelled, {}, {}, document.version()};
    }
  }

  auto supported_capabilities() -> capabilities
  {
    return capabilities{diagnostics::schema_version, true, true, true, true, true, true, true, true};
  }

  auto capabilities_json() -> std::string
  {
    const auto value = supported_capabilities();
    const auto boolean = [](const bool enabled) { return enabled ? "true" : "false"; };
    std::ostringstream output;
    output << "{\"schema\":\"" << value.schema << "\",\"documentationCatalog\":\""
           << documentation_schema_version << "\",\"sourceEditsSchema\":\""
           << source_edits_schema_version << "\",\"operationsSchema\":\""
           << operations_schema_version << "\",\"sourceMapSchema\":\""
           << codegen::source_map_schema_version << "\",\"debugMetadataSchema\":\""
           << debug_metadata_schema_version << "\",\"positionEncodings\":[\"utf-16\",\"utf-8-bytes\"],"
           << "\"capabilities\":{\"strictDocumentCheck\":" << boolean(value.strict_document_check)
           << ",\"structuredDiagnostics\":" << boolean(value.structured_diagnostics)
           << ",\"utf16Positions\":" << boolean(value.utf16_positions)
           << ",\"cancellation\":" << boolean(value.cancellation)
           << ",\"recovery\":" << boolean(value.recovery)
           << ",\"documentOverlays\":" << boolean(value.document_overlays)
           << ",\"semanticIndex\":" << boolean(value.semantic_index)
           << ",\"nativeCheck\":true,\"nativeBuild\":true,\"nativeRun\":true"
           << ",\"sourceMaps\":true,\"debugMetadata\":true,\"debugLaunchPlan\":true"
           << ",\"debugAttach\":false,\"optimizedLocalEvaluation\":false,\"testDiscovery\":false"
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

  auto analyze_document(const source::document_snapshot &document, const check_options options,
                        const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<check_summary>
  {
    auto syntax_result = syntax::analyze(document, syntax::analysis_options{true, 64}, cancellation);
    if (syntax_result.state == diagnostics::result_state::cancelled)
      return cancelled(document);
    if (syntax_result.state != diagnostics::result_state::complete)
    {
      for (auto &diagnostic : syntax_result.diagnostics)
        attach_closing_brace_fix(document, diagnostic);
      check_summary summary;
      if (syntax_result.value)
      {
        summary.token_count = syntax_result.value->tokens.size();
        if (syntax_result.value->recovered_ast)
          summary.statement_count = syntax_result.value->recovered_ast->statements.size();
      }
      return diagnostics::analysis_result<check_summary>{
          syntax_result.state, summary, std::move(syntax_result.diagnostics), document.version()};
    }
    return check_document(document, options, cancellation);
  }

  auto analyze_project_document(const source::document_snapshot &document,
                                const source::source_provider &provider,
                                const diagnostics::cancellation_token cancellation)
    -> diagnostics::analysis_result<check_summary>
  {
    if (!document.identity().canonical_path)
      return analyze_document(document, {.check_types = true}, cancellation);
    auto syntax_result = syntax::analyze(document, syntax::analysis_options{true, 64}, cancellation);
    if (syntax_result.state == diagnostics::result_state::cancelled) return cancelled(document);
    if (syntax_result.state != diagnostics::result_state::complete)
    {
      for (auto &issue : syntax_result.diagnostics) attach_closing_brace_fix(document, issue);
      return {syntax_result.state, {}, std::move(syntax_result.diagnostics), document.version()};
    }
    diagnostics::phase phase = diagnostics::phase::module;
    std::size_t module_count = 0;
    try
    {
      module_count = modules::resolve(*document.identity().canonical_path, provider).modules.size();
      if (cancellation.is_cancelled()) return cancelled(document);
      const auto tree = modules::link(*document.identity().canonical_path, provider);
      phase = diagnostics::phase::semantic;
      (void)semantic::analyze(tree);
      if (cancellation.is_cancelled()) return cancelled(document);
      phase = diagnostics::phase::type;
      (void)semantic::check_types(tree);
      if (cancellation.is_cancelled()) return cancelled(document);
      check_summary summary;
      summary.token_count = syntax_result.value ? syntax_result.value->tokens.size() : 0;
      summary.statement_count = tree.statements.size();
      return {diagnostics::result_state::complete, summary, {}, document.version()};
    }
    catch (const semantic::semantic_error &failure)
    {
      parser::span range{};
      if ((failure.origin_path ? *failure.origin_path == *document.identity().canonical_path
                               : module_count == 1) &&
          failure.range.begin >= 0 && failure.range.end >= failure.range.begin &&
          static_cast<std::size_t>(failure.range.end) <= document.text().size())
        range = failure.range;
      return {diagnostics::result_state::incomplete, {},
              {make_diagnostic(document, phase, range, failure.what())}, document.version()};
    }
    catch (const std::exception &failure)
    {
      return {diagnostics::result_state::incomplete, {},
              {make_diagnostic(document, phase, {}, failure.what())}, document.version()};
    }
  }

  auto index_document(const source::document_snapshot &document,
                      const diagnostics::cancellation_token cancellation,
                      const std::optional<semantic::analysis_identity> identity)
    -> diagnostics::analysis_result<semantic_snapshot>
  {
    diagnostics::phase active_phase = diagnostics::phase::syntax;
    try
    {
      if (cancellation.is_cancelled()) return cancelled_index(document);
      auto syntax_result = syntax::analyze(document, syntax::analysis_options{true, 64}, cancellation);
      if (syntax_result.state == diagnostics::result_state::cancelled) return cancelled_index(document);
      if (!syntax_result.value ||
          (!syntax_result.value->strict_ast && !syntax_result.value->recovered_ast))
        return {syntax_result.state, {}, std::move(syntax_result.diagnostics), document.version()};
      if (cancellation.is_cancelled()) return cancelled_index(document);
      active_phase = diagnostics::phase::semantic;
      const auto module = document.identity().canonical_path
                              ? document.identity().canonical_path->generic_string()
                              : document.identity().uri.value;
      const bool recovered = !syntax_result.value->strict_ast;
      const auto &tree = recovered ? *syntax_result.value->recovered_ast : *syntax_result.value->strict_ast;
      const auto selected_identity = identity.value_or(semantic::analysis_identity{"local", module});
      auto model = recovered
                       ? semantic::analyze_partial(tree, selected_identity)
                       : semantic::analyze(tree, selected_identity);
      std::optional<semantic::type_model> type_model;
      if (!recovered) type_model = semantic::check_types(tree);
      auto index = semantic::build_index(document, model, type_model ? &*type_model : nullptr);
      return {recovered ? diagnostics::result_state::recovered : diagnostics::result_state::complete,
              semantic_snapshot{std::move(model), std::move(index), std::move(type_model)},
              std::move(syntax_result.diagnostics), document.version()};
    }
    catch (const parser::parse_error &error)
    {
      return {diagnostics::result_state::incomplete, {},
              {make_diagnostic(document, active_phase, error.range, error.what())}, document.version()};
    }
    catch (const semantic::semantic_error &error)
    {
      return {diagnostics::result_state::incomplete, {},
              {make_diagnostic(document, active_phase, error.range, error.what())}, document.version()};
    }
  }
}

