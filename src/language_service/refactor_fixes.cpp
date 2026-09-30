#include "refactor.hpp"
#include "../syntax/syntax.hpp"

namespace sagan::language_service
{
  auto plan_diagnostic_fix(const source::document_snapshot &document,
                           const diagnostics::analysis_result<check_summary> &analysis,
                           const std::size_t diagnostic_index, const std::size_t fix_index) -> edit_plan
  {
    if (analysis.analyzed_version != document.version())
      return {edit_state::stale, "Diagnostic belongs to an older document version", {}};
    if (diagnostic_index >= analysis.diagnostics.size())
      return {edit_state::invalid, "Diagnostic index is unavailable", {}};
    const auto &diagnostic = analysis.diagnostics[diagnostic_index];
    if (diagnostic.primary.document != document.identity().id || fix_index >= diagnostic.fixes.size())
      return {edit_state::invalid, "Fix is not available for this document", {}};
    const auto &fix = diagnostic.fixes[fix_index];
    if (fix.edits.empty()) return {edit_state::unsupported, "Fix contains no source edits", {}};
    for (const auto &edit : fix.edits)
      if (edit.range.document != document.identity().id)
        return {edit_state::unsupported, "Cross-document fix needs workspace proof", {}};
    workspace_edit edits{{{document.identity().uri, document.version(), fix.edits}}};
    const auto preview = preview_edits(edits, {&document});
    if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
    const source::document_snapshot changed(document.identity(), document.version(),
                                             preview.documents.front().text);
    if (diagnostic.owner == diagnostics::phase::lexical ||
        diagnostic.owner == diagnostics::phase::syntax)
    {
      const auto checked = syntax::analyze(changed, {.recover = false});
      if (!checked.value || !checked.value->strict_ast)
        return {edit_state::unsupported, "Fix did not produce strict syntax", {}};
    }
    else if (check_document(changed).state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Fix did not pass semantic and type checking", {}};
    return {edit_state::ready, {}, std::move(edits)};
  }
}
