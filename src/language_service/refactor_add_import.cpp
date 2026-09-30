#include "refactor.hpp"
#include "../parser/ast_node.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>

namespace sagan::language_service
{
  auto add_missing_import(const source::document_snapshot &document,
                          const semantic::workspace_semantic_index &workspace,
                          const semantic::symbol_id &target) -> edit_plan
  {
    const auto parsed = syntax::analyze(document, {.recover = false});
    const auto before = index_document(document);
    if (!parsed.value || !parsed.value->strict_ast || !before.value ||
        before.state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Import insertion requires complete source", {}};
    const auto &statements = parsed.value->strict_ast->statements;
    if (statements.empty()) return {edit_state::unsupported, "Module declaration is required", {}};
    const auto *module = dynamic_cast<const parser::module_declaration *>(statements.front().get());
    if (!module) return {edit_state::unsupported, "Module declaration is required", {}};
    const semantic::exported_symbol *selected = nullptr;
    const auto exports = workspace.exported_symbols();
    for (const auto &candidate : exports)
      if (candidate.targets.size() == 1 && candidate.targets.front() == target)
      {
        if (selected) return {edit_state::conflict, "Target has multiple public import names", {}};
        selected = &candidate;
      }
    if (!selected || !workspace.find(target) || selected->module == module->name)
      return {edit_state::unsupported, "Target is not one external public export", {}};
    for (const auto &symbol : before.value->index.symbols())
      if (symbol.name == selected->public_name)
        return {edit_state::conflict, "Import name already exists in this module", {}};
    const auto text = document.text();
    const auto module_end = static_cast<std::size_t>(module->range.end);
    const auto newline = text.find('\n', module_end);
    if (newline == std::string_view::npos ||
        !std::all_of(text.begin() + static_cast<std::ptrdiff_t>(module_end),
                     text.begin() + static_cast<std::ptrdiff_t>(newline),
                     [](const char byte) { return byte == ' ' || byte == '\t' || byte == '\r'; }))
      return {edit_state::unsupported, "Module header has no safe insertion line", {}};
    const auto offset = static_cast<source::byte_offset>(newline + 1);
    const auto eol = newline > 0 && text[newline - 1] == '\r' ? "\r\n" : "\n";
    const std::string insertion = "import " + selected->public_name + " from " +
                                  selected->module + eol;
    workspace_edit edits{{{document.identity().uri, document.version(),
                           {{{document.identity().id, {offset, offset}}, insertion}}}}};
    const auto preview = preview_edits(edits, {&document});
    if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
    const source::document_snapshot changed(document.identity(), document.version(),
                                             preview.documents.front().text);
    const auto checked = index_document(changed);
    if (!checked.value || checked.state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Added import did not pass semantic and type checking", {}};
    const auto imported = std::count_if(checked.value->index.symbols().begin(),
                                        checked.value->index.symbols().end(), [&](const auto &symbol)
                                        {
                                          return symbol.origin == semantic::symbol_origin::imported &&
                                                 symbol.name == selected->public_name;
                                        });
    if (imported != 1) return {edit_state::unsupported, "Import binding was not resolved uniquely", {}};
    return {edit_state::ready, {}, std::move(edits)};
  }
}
