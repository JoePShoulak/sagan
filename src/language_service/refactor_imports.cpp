#include "refactor.hpp"
#include "language_service.hpp"
#include "../parser/ast_node.hpp"
#include "../syntax/syntax.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace sagan::language_service
{
  namespace
  {
    auto horizontal(const std::string_view text) -> bool
    {
      return std::all_of(text.begin(), text.end(), [](const char byte)
      { return byte == ' ' || byte == '\t'; });
    }

    auto whitespace(const std::string_view text) -> bool
    {
      return std::all_of(text.begin(), text.end(), [](const char byte)
      { return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n'; });
    }

    struct import_line
    {
      std::string key;
      std::string text;
      source::byte_offset begin{};
      source::byte_offset end{};
    };
  }

  auto organize_imports(const source::document_snapshot &document) -> edit_plan
  {
    const auto parsed = syntax::analyze(document, {.recover = false});
    if (!parsed.value || !parsed.value->strict_ast)
      return {edit_state::unsupported, "Import organization requires complete source", {}};
    const auto before = index_document(document);
    if (!before.value || before.state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Import organization requires a valid semantic snapshot", {}};
    const auto &statements = parsed.value->strict_ast->statements;
    const auto text = document.text();
    std::size_t next = 0;
    std::size_t allowed_begin = 0;
    if (!statements.empty())
      if (const auto *module = dynamic_cast<const parser::module_declaration *>(statements.front().get()))
      {
        allowed_begin = static_cast<std::size_t>(module->range.end);
        next = 1;
      }
    const auto first_import = next;
    while (next < statements.size() &&
           dynamic_cast<const parser::import_declaration *>(statements[next].get())) ++next;
    for (std::size_t i = next; i < statements.size(); ++i)
      if (dynamic_cast<const parser::import_declaration *>(statements[i].get()))
        return {edit_state::unsupported, "Imports are interleaved with other declarations", {}};
    if (next - first_import < 2)
      return {edit_state::ready, {}, {{{document.identity().uri, document.version(), {}}}}};

    std::vector<import_line> lines;
    std::size_t prior_line_end = 0;
    for (std::size_t i = first_import; i < next; ++i)
    {
      const auto *import = dynamic_cast<const parser::import_declaration *>(statements[i].get());
      const auto statement_begin = static_cast<std::size_t>(import->range.begin);
      const auto statement_end = static_cast<std::size_t>(import->range.end);
      if (statement_begin > text.size() || statement_end > text.size() ||
          statement_begin > statement_end)
        return {edit_state::unsupported, "Import source range is invalid", {}};
      const auto previous_newline = statement_begin == 0 ? std::string_view::npos :
                                    text.rfind('\n', statement_begin - 1);
      const auto line_begin = previous_newline == std::string_view::npos ? 0 : previous_newline + 1;
      const auto newline = text.find('\n', statement_end);
      if (newline == std::string_view::npos)
        return {edit_state::unsupported, "Import line has no terminating newline", {}};
      const auto line_end = newline + 1;
      const auto suffix_end = newline > statement_end && text[newline - 1] == '\r' ? newline - 1 : newline;
      if (!horizontal(text.substr(line_begin, statement_begin - line_begin)) ||
          !horizontal(text.substr(statement_end, suffix_end - statement_end)))
        return {edit_state::unsupported, "Import has attached or trailing trivia", {}};
      if (i == first_import)
      {
        if (line_begin < allowed_begin || !whitespace(text.substr(allowed_begin, line_begin - allowed_begin)))
          return {edit_state::unsupported, "A comment may belong to the first import", {}};
      }
      else if (line_begin != prior_line_end)
        return {edit_state::unsupported, "Import groups have comments or blank lines", {}};
      const std::string key = import->source_module.value_or(import->imported_name) + "\n" +
                              import->imported_name + "\n" + import->alias.value_or("");
      lines.push_back({key, std::string(text.substr(line_begin, line_end - line_begin)),
                       static_cast<source::byte_offset>(line_begin),
                       static_cast<source::byte_offset>(line_end)});
      prior_line_end = line_end;
    }
    const auto newline_style = lines.front().text.ends_with("\r\n");
    if (std::any_of(lines.begin(), lines.end(), [&](const auto &line)
                    { return line.text.ends_with("\r\n") != newline_style; }))
      return {edit_state::unsupported, "Mixed line endings in the import block", {}};
    auto ordered = lines;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right)
    { return left.key < right.key; });
    std::string replacement;
    for (const auto &line : ordered) replacement += line.text;
    const auto original = text.substr(lines.front().begin, lines.back().end - lines.front().begin);
    if (original == replacement)
      return {edit_state::ready, {}, {{{document.identity().uri, document.version(), {}}}}};
    workspace_edit edits{{{document.identity().uri, document.version(),
                           {{{document.identity().id, {lines.front().begin, lines.back().end}},
                             std::move(replacement)}}}}};
    const auto preview = preview_edits(edits, {&document});
    if (preview.state != edit_state::ready) return {preview.state, preview.reason, {}};
    const source::document_snapshot changed(document.identity(), document.version(),
                                             preview.documents.front().text);
    const auto after = index_document(changed);
    if (!after.value || after.state != diagnostics::result_state::complete)
      return {edit_state::unsupported, "Organized imports did not pass semantic analysis", {}};
    return {edit_state::ready, {}, std::move(edits)};
  }
}
