#include "../src/language_service/edits.hpp"
#include "../src/language_service/formatter.hpp"
#include "../src/language_service/refactor.hpp"
#include "../src/language_service/language_service.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <iostream>

namespace
{
  auto require(const bool condition, const char *message) -> void
  {
    if (!condition) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  using namespace sagan;
  const auto actions = language_service::source_edit_capabilities();
  const auto action = [&](std::string_view name)
  {
    return std::find_if(actions.begin(), actions.end(), [&](const auto &candidate)
                        { return candidate.action == name; });
  };
  require(language_service::source_edits_schema_version == "sagan-source-edits-v1" &&
              action("format.document") != actions.end() && action("format.document")->available &&
              action("diagnostic.fix") != actions.end() && action("diagnostic.fix")->available &&
              action("imports.add") != actions.end() && action("imports.add")->available &&
              action("rename.function") != actions.end() && action("rename.function")->available &&
              action("rename.workspace") != actions.end() && !action("rename.workspace")->available &&
              !action("rename.workspace")->limitation.empty() &&
              action("extract.function") != actions.end() && !action("extract.function")->available,
          "source-edit capability contract advertised an unsafe action or omitted a safe action");
  const source::document_snapshot document(
      {{source::document_id{101}, source::document_uri{"untitled:edit"}, {}}, 7,
       "fun main(): Int {\nlet value = 2\nreturn value\n}\n"});
  const auto formatted = language_service::format_document(document);
  require(formatted.state == language_service::edit_state::ready &&
              formatted.edits.documents.size() == 1 &&
              formatted.edits.documents.front().expected_version == 7,
          "whole-document formatting did not return versioned edits");
  const auto preview = language_service::preview_edits(formatted.edits, {&document});
  require(preview.state == language_service::edit_state::ready &&
              preview.documents.front().text ==
                  "fun main(): Int {\n  let value = 2\n  return value\n}\n",
          "formatter did not produce two-space block indentation");
  const source::document_snapshot again(document.identity(), 8, preview.documents.front().text);
  const auto idempotent = language_service::format_document(again);
  require(idempotent.state == language_service::edit_state::ready &&
              idempotent.edits.documents.front().edits.empty(),
          "formatter was not idempotent");
  const source::document_snapshot spacing(
      {{source::document_id{105}, source::document_uri{"untitled:spacing"}, {}}, 1,
       "fun add( left:Int,right :Int ):Int=>left+right\n"
       "fun main():Int{\nlet answer=add(1,2)\nreturn answer\n}\n"});
  const auto spacing_edits = language_service::format_document(spacing);
  require(spacing_edits.state == language_service::edit_state::ready,
          "spacing fixture was not safely formatted");
  const auto spacing_preview = language_service::preview_edits(spacing_edits.edits, {&spacing});
  require(spacing_preview.state == language_service::edit_state::ready &&
              spacing_preview.documents.front().text ==
                  "fun add(left: Int, right: Int): Int => left + right\n"
                  "fun main(): Int {\n  let answer = add(1, 2)\n  return answer\n}\n",
          "formatter did not apply its canonical unambiguous token gaps");
  const source::document_snapshot spacing_again(spacing.identity(), 2,
                                                 spacing_preview.documents.front().text);
  require(language_service::format_document(spacing_again).edits.documents.front().edits.empty(),
          "spacing formatter was not idempotent");
  const auto spacing_range = language_service::format_range(
      spacing, {static_cast<source::byte_offset>(spacing.text().find("left:Int")),
                static_cast<source::byte_offset>(spacing.text().find("left:Int") + 1)});
  const auto spacing_range_preview = language_service::preview_edits(spacing_range.edits, {&spacing});
  require(spacing_range_preview.state == language_service::edit_state::ready &&
              spacing_range_preview.documents.front().text.starts_with(
                  "fun add(left: Int, right: Int): Int => left + right\n") &&
              spacing_range_preview.documents.front().text.find("let answer=add(1,2)") !=
                  std::string::npos,
          "range formatting failed to normalize one selected line or changed another");
  const source::document_snapshot whitespace(
      {{source::document_id{110}, source::document_uri{"untitled:whitespace"}, {}}, 1,
       "fun main(): Int {\nlet value=-2  \n// keep comment spaces  \nreturn value\n}\n"});
  const auto whitespace_format = language_service::format_document(whitespace);
  const auto whitespace_preview = language_service::preview_edits(whitespace_format.edits, {&whitespace});
  require(whitespace_format.state == language_service::edit_state::ready &&
              whitespace_preview.state == language_service::edit_state::ready &&
              whitespace_preview.documents.front().text ==
                  "fun main(): Int {\n  let value = -2\n  // keep comment spaces  \n"
                  "  return value\n}\n",
          "formatter failed unary spacing or removed comment content");
  const auto limited = language_service::format_range(document, {17, 30});
  const auto limited_preview = language_service::preview_edits(limited.edits, {&document});
  require(limited_preview.state == language_service::edit_state::ready &&
              limited_preview.documents.front().text ==
                  "fun main(): Int {\n  let value = 2\nreturn value\n}\n",
          "range formatting changed lines outside the selected region");
  const auto typed = language_service::format_on_type(document, 45, '}');
  require(typed.state == language_service::edit_state::ready &&
              typed.edits.documents.front().edits.empty(),
          "on-type formatting changed unrelated lines");
  require(language_service::format_on_type(document, 45, 'a').state ==
              language_service::edit_state::unsupported,
          "unsupported on-type trigger was accepted");

  const source::document_snapshot incomplete(
      {{source::document_id{102}, source::document_uri{"untitled:incomplete"}, {}}, 1,
       "fun main(): Int {\nlet value = \n"});
  require(language_service::format_document(incomplete).state ==
              language_service::edit_state::unsupported,
          "formatter modified incomplete source");
  const source::document_snapshot missing_brace(
      {{source::document_id{111}, source::document_uri{"untitled:missing-brace"}, {}}, 3,
       "fun main(): Int {\n  return 0\n"});
  const auto missing_brace_diagnostic = language_service::analyze_document(missing_brace);
  require(missing_brace_diagnostic.diagnostics.size() == 1 &&
              missing_brace_diagnostic.diagnostics.front().fixes.size() == 1,
          "compiler did not offer its proven missing-brace fix");
  const auto brace_fix = language_service::plan_diagnostic_fix(missing_brace,
                                                                missing_brace_diagnostic, 0, 0);
  const auto brace_preview = language_service::preview_edits(brace_fix.edits, {&missing_brace});
  require(brace_fix.state == language_service::edit_state::ready &&
              brace_preview.state == language_service::edit_state::ready &&
              brace_preview.documents.front().text == "fun main(): Int {\n  return 0\n}\n",
          "diagnostic quick fix did not produce a valid source preview");
  const source::document_snapshot stale_brace(missing_brace.identity(), 4,
                                               std::string(missing_brace.text()));
  require(language_service::plan_diagnostic_fix(stale_brace, missing_brace_diagnostic, 0, 0).state ==
              language_service::edit_state::stale,
          "diagnostic quick fix accepted an obsolete document version");
  const source::document_snapshot comment(
      {{source::document_id{103}, source::document_uri{"untitled:comment"}, {}}, 1,
       "fun main(): Int {\n/* first\n   second */\nreturn 0\n}\n"});
  const auto comment_format = language_service::format_document(comment);
  const auto comment_preview = language_service::preview_edits(comment_format.edits, {&comment});
  require(comment_preview.state == language_service::edit_state::ready &&
              comment_preview.documents.front().text.find("   second */") != std::string::npos,
          "formatter changed content inside a multiline comment");

  language_service::workspace_edit edits{{
      {document.identity().uri, 7, {{{document.identity().id, {17, 17}}, "  "}}},
      {comment.identity().uri, 1, {{{comment.identity().id, {0, 0}}, "// note\n"}}}}};
  const auto multi = language_service::preview_edits(edits, {&document, &comment});
  require(multi.state == language_service::edit_state::ready && multi.documents.size() == 2,
          "multi-document edit preview failed");
  edits.documents.front().expected_version = 8;
  require(language_service::preview_edits(edits, {&document, &comment}).state ==
              language_service::edit_state::stale,
          "stale workspace edit was accepted");
  edits.documents.front().expected_version = 7;
  edits.documents.front().edits.push_back({{document.identity().id, {17, 17}}, "x"});
  require(language_service::preview_edits(edits, {&document, &comment}).state ==
              language_service::edit_state::conflict,
          "same-position insertions were accepted");
  edits.documents.front().edits.pop_back();
  edits.documents.front().edits.front().range.bytes = {18, 19};
  require(language_service::preview_edits(edits, {&document, &comment}).state ==
              language_service::edit_state::ready,
          "valid single-byte replacement was rejected");
  edits.documents.front().edits.front().range.bytes = {18, 18};
  edits.documents.front().edits.push_back({{document.identity().id, {18, 19}}, "x"});
  require(language_service::preview_edits(edits, {&document, &comment}).state ==
              language_service::edit_state::conflict,
          "insertion and replacement at the same position were accepted");
  const source::document_snapshot emoji(
      {{source::document_id{104}, source::document_uri{"untitled:emoji"}, {}}, 1,
       "fun 🚀(): Int => 0\n"});
  const auto emoji_start = static_cast<source::byte_offset>(emoji.text().find("🚀"));
  const language_service::workspace_edit split_emoji{{
      {emoji.identity().uri, 1, {{{emoji.identity().id, {emoji_start + 1, emoji_start + 2}}, "x"}}}}};
  require(language_service::preview_edits(split_emoji, {&emoji}).state ==
              language_service::edit_state::invalid,
          "workspace edit split a UTF-8 identifier");
  const language_service::workspace_edit invalid_utf8{{
      {document.identity().uri, 7, {{{document.identity().id, {0, 0}}, std::string(1, '\xFF')}}}}};
  require(language_service::preview_edits(invalid_utf8, {&document}).state ==
              language_service::edit_state::invalid,
          "workspace edit accepted invalid UTF-8 replacement text");
  const auto indexed = language_service::index_document(document);
  require(indexed.value.has_value(), "rename fixture did not index");
  const auto declaration = static_cast<source::byte_offset>(document.text().find("value ="));
  const auto use = static_cast<source::byte_offset>(document.text().find("return value") + 7);
  const auto declaration_rename = language_service::rename_local(
      document, indexed.value->index, declaration, "altitude");
  require(declaration_rename.state == language_service::edit_state::ready &&
              declaration_rename.edits.documents.front().edits.size() == 2,
          "identity-based local rename failed from the declaration position");
  const auto rename = language_service::rename_local(document, indexed.value->index, use, "altitude");
  require(rename.state == language_service::edit_state::ready &&
              rename.edits.documents.front().edits.size() == 2,
          "identity-based local rename did not include declaration and use");
  const auto renamed_preview = language_service::preview_edits(rename.edits, {&document});
  require(renamed_preview.state == language_service::edit_state::ready &&
              renamed_preview.documents.front().text.find("let altitude = 2") != std::string::npos &&
              renamed_preview.documents.front().text.find("return altitude") != std::string::npos,
          "local rename preview did not rewrite both bound occurrences");
  const auto emoji_rename = language_service::rename_local(document, indexed.value->index, use, "🚀");
  require(emoji_rename.state == language_service::edit_state::ready,
          "local rename rejected a valid emoji identifier");
  require(language_service::rename_local(document, indexed.value->index, use, "return").state ==
              language_service::edit_state::invalid,
          "rename accepted a keyword");
  require(language_service::rename_local(document, indexed.value->index, use, "main").state ==
              language_service::edit_state::conflict,
          "rename accepted a name already present in scope");
  require(language_service::rename_local(document, indexed.value->index, 4, "launch").state ==
              language_service::edit_state::unsupported,
          "rename offered the main entry function");
  const source::document_snapshot function_document(
      {{source::document_id{107}, source::document_uri{"untitled:function-rename"}, {}}, 1,
       "fun double(value: Int): Int => value + value\n"
       "fun main(): Int => double(21)\n"});
  const auto indexed_function = language_service::index_document(function_document);
  require(indexed_function.value.has_value(), "function rename fixture did not index");
  const auto function_declaration = static_cast<source::byte_offset>(
      function_document.text().find("double(value"));
  const auto function_use = static_cast<source::byte_offset>(function_document.text().find("double(21)"));
  const auto function_declaration_rename = language_service::rename_local(
      function_document, indexed_function.value->index, function_declaration, "twice");
  require(function_declaration_rename.state == language_service::edit_state::ready &&
              function_declaration_rename.edits.documents.front().edits.size() == 2,
          "function rename failed from the declaration position");
  const auto function_rename = language_service::rename_local(
      function_document, indexed_function.value->index, function_use, "twice");
  require(function_rename.state == language_service::edit_state::ready &&
              function_rename.edits.documents.front().edits.size() == 2,
          "function rename did not include declaration and call");
  const auto function_preview = language_service::preview_edits(function_rename.edits, {&function_document});
  require(function_preview.state == language_service::edit_state::ready &&
              function_preview.documents.front().text.find("fun twice(") != std::string::npos &&
              function_preview.documents.front().text.find("=> twice(21)") != std::string::npos,
          "function rename preview did not update both uses");
  const auto parameter_declaration = static_cast<source::byte_offset>(
      function_document.text().find("value: Int"));
  const auto parameter_rename = language_service::rename_local(
      function_document, indexed_function.value->index, parameter_declaration, "input");
  require(parameter_rename.state == language_service::edit_state::ready &&
              parameter_rename.edits.documents.front().edits.size() == 3,
          "parameter rename failed from the declaration position");
  const source::document_snapshot loop_document(
      {{source::document_id{109}, source::document_uri{"untitled:loop-rename"}, {}}, 1,
       "fun main(): Int {\n"
       "let indices: Array<Int> = 5.times\n"
       "for index in indices {\n"
       "print(index)\n"
       "}\n"
       "return 0\n"
       "}\n"});
  const auto indexed_loop = language_service::index_document(loop_document);
  require(indexed_loop.value.has_value(), "loop-binding rename fixture did not index");
  const auto loop_declaration = static_cast<source::byte_offset>(
      loop_document.text().find("index in"));
  const auto loop_rename = language_service::rename_local(
      loop_document, indexed_loop.value->index, loop_declaration, "item");
  require(loop_rename.state == language_service::edit_state::ready &&
              loop_rename.edits.documents.front().edits.size() == 2,
          "loop-binding rename failed from the declaration position");
  const source::document_snapshot constant_document(
      {{source::document_id{110}, source::document_uri{"untitled:constant-rename"}, {}}, 1,
       "fun answer(): Int {\n"
       "const OFFSET = 2\n"
       "return OFFSET + 40\n"
       "}\n"
       "fun main(): Int => answer()\n"});
  const auto indexed_constant = language_service::index_document(constant_document);
  require(indexed_constant.value.has_value(), "constant rename fixture did not index");
  const auto constant_declaration = static_cast<source::byte_offset>(
      constant_document.text().find("OFFSET ="));
  const auto constant_rename = language_service::rename_local(
      constant_document, indexed_constant.value->index, constant_declaration, "DISTANCE");
  require(constant_rename.state == language_service::edit_state::ready &&
              constant_rename.edits.documents.front().edits.size() == 2,
          "constant rename failed from the declaration position");
  const source::document_snapshot match_document(
      {{source::document_id{111}, source::document_uri{"untitled:match-rename"}, {}}, 1,
       "enum Result {\n"
       "Success(Int) = 200\n"
       "Failure(String) = 500\n"
       "}\n"
       "fun unwrap(result: Result): Int {\n"
       "match result {\n"
       "case Success(value) return value\n"
       "case Failure(message) return 0\n"
       "}\n"
       "}\n"
       "fun main(): Int => unwrap(Success(42))\n"});
  const auto indexed_match = language_service::index_document(match_document);
  require(indexed_match.value.has_value(), "match-binding rename fixture did not index");
  const auto match_declaration = static_cast<source::byte_offset>(
      match_document.text().find("value) return"));
  const auto match_rename = language_service::rename_local(
      match_document, indexed_match.value->index, match_declaration, "payload");
  require(match_rename.state == language_service::edit_state::ready &&
              match_rename.edits.documents.front().edits.size() == 2,
          "match-binding rename failed from the declaration position");
  const source::document_snapshot exported_document(
      {{source::document_id{108}, source::document_uri{"untitled:exported-rename"}, {}}, 1,
       "module helper\nfun double(value: Int): Int => value + value\nexport double\n"});
  const auto indexed_exported = language_service::index_document(exported_document);
  require(indexed_exported.value.has_value(), "exported function fixture did not index");
  const auto exported_name = static_cast<source::byte_offset>(exported_document.text().find("double"));
  require(language_service::rename_local(exported_document, indexed_exported.value->index,
                                         exported_name, "twice").state ==
              language_service::edit_state::unsupported,
          "function rename offered exported symbol without workspace proof");
  const source::document_snapshot newer(document.identity(), 8, std::string(document.text()));
  require(language_service::rename_local(newer, indexed.value->index, use, "altitude").state ==
              language_service::edit_state::stale,
          "rename accepted a stale semantic index");
  const source::document_snapshot imports(
      {{source::document_id{106}, source::document_uri{"untitled:imports"}, {}}, 1,
       "module mission\n"
       "import telemetry.flight as telemetry\n"
       "import course from guidance\n"
       "fun main(): Int => 0\n"});
  const auto organized = language_service::organize_imports(imports);
  require(organized.state == language_service::edit_state::ready &&
              organized.edits.documents.front().edits.size() == 1,
          "plain top-level imports were not organized");
  const auto organized_preview = language_service::preview_edits(organized.edits, {&imports});
  require(organized_preview.state == language_service::edit_state::ready &&
              organized_preview.documents.front().text ==
                  "module mission\n"
                  "import course from guidance\n"
                  "import telemetry.flight as telemetry\n"
                  "fun main(): Int => 0\n",
          "import sorting changed declarations or selected the wrong order");
  const source::document_snapshot imports_again(imports.identity(), 2,
                                                 organized_preview.documents.front().text);
  require(language_service::organize_imports(imports_again).edits.documents.front().edits.empty(),
          "import organization was not idempotent");
  const source::document_snapshot commented_imports(
      {{source::document_id{107}, source::document_uri{"untitled:commented-imports"}, {}}, 1,
       "module mission\n"
       "import telemetry.flight as telemetry\n"
       "// This comment belongs to course.\n"
       "import course from guidance\n"
       "fun main(): Int => 0\n"});
  require(language_service::organize_imports(commented_imports).state ==
              language_service::edit_state::unsupported,
          "import organizer moved a comment away from its declaration");
  const source::document_snapshot grouped_imports(
      {{source::document_id{108}, source::document_uri{"untitled:grouped-imports"}, {}}, 1,
       "module mission\nimport telemetry.flight as telemetry\n\n"
       "import course from guidance\nfun main(): Int => 0\n"});
  require(language_service::organize_imports(grouped_imports).state ==
              language_service::edit_state::unsupported,
          "import organizer collapsed a deliberate blank-line grouping");
  const source::disk_source_provider disk;
  const auto graph = modules::resolve("tests/fixtures/modules/module_demo/main.sagan", disk);
  const auto workspace_index = semantic::build_workspace_index(graph, disk);
  const auto exports = workspace_index.exported_symbols();
  const auto course = std::find_if(exports.begin(), exports.end(), [](const auto &entry)
  { return entry.module == "guidance" && entry.public_name == "course"; });
  require(course != exports.end() && course->targets.size() == 1,
          "workspace fixture did not expose one public course symbol");
  const source::document_snapshot needs_import(
      {{source::document_id{112}, source::document_uri{"untitled:needs-import"}, {}}, 1,
       "module scratch\nfun main(): Int => 0\n"});
  const auto added = language_service::add_missing_import(needs_import, workspace_index,
                                                            course->targets.front());
  const auto added_preview = language_service::preview_edits(added.edits, {&needs_import});
  require(added.state == language_service::edit_state::ready &&
              added_preview.state == language_service::edit_state::ready &&
              added_preview.documents.front().text ==
                  "module scratch\nimport course from guidance\nfun main(): Int => 0\n",
          "identity-based missing import did not use the selected public export");
  const source::document_snapshot import_conflict(
      {{source::document_id{113}, source::document_uri{"untitled:import-conflict"}, {}}, 1,
       "module scratch\nfun course(): Int => 0\nfun main(): Int => 0\n"});
  require(language_service::add_missing_import(import_conflict, workspace_index,
                                                course->targets.front()).state ==
              language_service::edit_state::conflict,
          "add-import action accepted a colliding local declaration");
  const source::document_snapshot crlf_imports(
      {{source::document_id{109}, source::document_uri{"untitled:crlf-imports"}, {}}, 1,
       "module mission\r\nimport telemetry.flight as telemetry\r\n"
       "import course from guidance\r\nfun main(): Int => 0\r\n"});
  const auto crlf_organized = language_service::organize_imports(crlf_imports);
  const auto crlf_preview = language_service::preview_edits(crlf_organized.edits, {&crlf_imports});
  require(crlf_organized.state == language_service::edit_state::ready &&
              crlf_preview.state == language_service::edit_state::ready &&
              crlf_preview.documents.front().text.find("import course from guidance\r\n") !=
                  std::string::npos,
          "import organizer did not preserve CRLF line endings");
  std::cout << "Sagan formatting input:\n" << document.text()
            << "\nFormatted preview:\n" << preview.documents.front().text
            << "\nRenamed preview:\n" << renamed_preview.documents.front().text
            << "\nOrganized imports preview:\n" << organized_preview.documents.front().text
            << "\nAdded import preview:\n" << added_preview.documents.front().text
            << "\nMissing brace quick-fix preview:\n" << brace_preview.documents.front().text;
  return 0;
}
