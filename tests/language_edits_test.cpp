#include "../src/language_service/edits.hpp"
#include "../src/language_service/formatter.hpp"
#include "../src/language_service/refactor.hpp"
#include "../src/language_service/language_service.hpp"

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
                  "fun add(left: Int, right: Int): Int => left+right\n"
                  "fun main(): Int{\n  let answer = add(1, 2)\n  return answer\n}\n",
          "formatter did not apply its canonical unambiguous token gaps");
  const source::document_snapshot spacing_again(spacing.identity(), 2,
                                                 spacing_preview.documents.front().text);
  require(language_service::format_document(spacing_again).edits.documents.front().edits.empty(),
          "spacing formatter was not idempotent");
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
  const auto use = static_cast<source::byte_offset>(document.text().find("return value") + 7);
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
          "rename offered a public function without a workspace proof");
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
            << "\nOrganized imports preview:\n" << organized_preview.documents.front().text;
  return 0;
}
