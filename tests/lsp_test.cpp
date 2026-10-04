#include "../src/lsp/server.hpp"
#include "../src/source/provider.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
  using J = sagan::lsp::json::value;
  auto require(const bool condition, const char *message) -> void
  { if (!condition) throw std::runtime_error(message); }
  auto request(sagan::lsp::server &service, const std::string &method, J params = J::object{}) -> J
  {
    const auto answers = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 7},
                                                   {"method", method}, {"params", std::move(params)}});
    require(answers.size() == 1, "request did not produce one response");
    require(!answers.front().get("error"), "LSP request returned an error");
    return *answers.front().get("result");
  }
  auto notify(sagan::lsp::server &service, const std::string &method, J params = J::object{}) -> J::array
  {
    return service.handle(J::object{{"jsonrpc", "2.0"}, {"method", method},
                                     {"params", std::move(params)}});
  }
  auto at(const std::string &uri, const int line, const int character) -> J
  {
    return J::object{{"textDocument", J::object{{"uri", uri}}},
                     {"position", J::object{{"line", line}, {"character", character}}}};
  }
  auto frame(const J &message) -> std::string
  {
    const auto encoded = sagan::lsp::json::serialize(message);
    return "Content-Length: " + std::to_string(encoded.size()) + "\r\n\r\n" + encoded;
  }
}

auto main() -> int
{
  using namespace sagan::lsp;
  const auto emoji = json::parse(R"({"text":"\ud83d\ude80"})");
  require(emoji.get("text") && emoji.get("text")->string() == "🚀",
          "JSON surrogate pair did not decode to UTF-8");
  require(json::parse(json::serialize(emoji)).get("text")->string() == "🚀",
          "JSON round trip changed an emoji");
  server service;
  const auto initialized = request(service, "initialize");
  require(initialized.get("capabilities") &&
              initialized.get("capabilities")->get("hoverProvider") &&
              initialized.get("capabilities")->get("hoverProvider")->boolean() == true &&
              initialized.get("capabilities")->get("renameProvider")->get("prepareProvider") &&
              initialized.get("capabilities")->get("renameProvider")->get("prepareProvider")->boolean() == true &&
              initialized.get("capabilities")->get("positionEncoding")->string() == "utf-16" &&
              initialized.get("experimental")->get("compiler")->get("capabilities")
                  ->get("languageServer")->boolean() == true,
          "initialize did not advertise implemented hover support");
  const auto catalog_uri = sagan::source::identity_from_path(
      sagan::source::document_id{}, "tests/fixtures/package_index/index.tsv").uri.value;
  const auto package_query = request(service, "sagan/packages/query",
      J::object{{"indexUri", catalog_uri}, {"prefix", "physics"}});
  require(package_query.get("schema")->string() == "sagan-package-index-v1" &&
              package_query.get("state")->string() == "ready" &&
              package_query.get("packages")->elements()->size() == 2,
          "LSP package index query did not expose compiler-owned local catalog results");
  const auto catalog = request(service, "sagan/packages/catalog",
      J::object{{"indexUri", sagan::source::identity_from_path(
          sagan::source::document_id{}, "tests/fixtures/catalog/index.tsv").uri.value},
          {"prefix", "orbit"}, {"limit", 1}});
  const auto &catalog_package = catalog.get("packages")->elements()->front();
  const auto &catalog_exports = *catalog_package.get("modules")->elements()->front()
      .get("exports")->elements();
  require(catalog.get("schema")->string() == "sagan-package-catalog-v1" &&
              catalog.get("state")->string() == "ready" &&
              catalog_package.get("modules")->elements()->size() == 1 &&
              std::any_of(catalog_exports.begin(), catalog_exports.end(), [](const auto &item)
                { return item.get("name")->string() == "orbit_answer"; }),
          "LSP package catalog omitted installed exported declarations");
  const std::string uri = "file:///lsp-demo.sagan";
  const std::string text = "fun 🚀(value: Int): Int => value + 2\nfun main(): Int {\n  let answer = 🚀(40)\n  print(answer)\n  return 0\n}\n";
  const auto opened = notify(service, "textDocument/didOpen",
                             J::object{{"textDocument", J::object{{"uri", uri},
                                                                  {"version", 1}, {"text", text}}}});
  require(opened.size() == 1 && opened.front().get("method") &&
              opened.front().get("method")->string() == "textDocument/publishDiagnostics" &&
              opened.front().get("params")->get("diagnostics")->elements()->empty(),
          "didOpen did not publish clean diagnostics");
  const std::string invalid_default_uri = "file:///default-argument-invalid.sagan";
  const auto invalid_default = notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", invalid_default_uri}, {"version", 1},
          {"text", "fun bad(value: Int = \"wrong\"): Int => value\nprint(bad())\n"}}}});
  require(invalid_default.size() == 2 &&
              invalid_default.back().get("params")->get("uri")->string() == invalid_default_uri &&
              !invalid_default.back().get("params")->get("diagnostics")->elements()->empty() &&
              invalid_default.back().get("params")->get("diagnostics")->elements()->front()
                  .get("message")->string()->find("Default argument") != std::string::npos,
          "default-argument type mismatch did not produce an LSP diagnostic");
  static_cast<void>(notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", invalid_default_uri}}}}));
  J::array operation_notifications;
  service.set_notification_sink([&](const J &message) { operation_notifications.push_back(message); });
  const auto operation = request(service, "sagan/operation",
      J::object{{"kind", "check"}, {"scope", "document"},
                {"textDocument", J::object{{"uri", uri}, {"version", 1}}},
                {"workDoneToken", "check-demo"}});
  require(operation.get("schema") && operation.get("schema")->string() == "sagan-operations-v2" &&
              operation.get("state") && operation.get("state")->string() == "completed" &&
              operation.get("diagnostics") && operation.get("diagnostics")->elements()->empty() &&
              operation.get("exitStatus") && operation.get("exitStatus")->integer() == 0 &&
              !operation_notifications.empty(), "LSP check operation contract failed");
  bool saw_progress = false;
  bool saw_operation_event = false;
  for (const auto &notice : operation_notifications)
  {
    saw_progress |= notice.get("method") && notice.get("method")->string() == "$/progress";
    saw_operation_event |= notice.get("method") && notice.get("method")->string() == "sagan/operationEvent";
  }
  require(saw_progress && saw_operation_event, "LSP operation omitted progress notifications");
  service.set_notification_sink({});
  const auto stale_operation = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 41},
      {"method", "sagan/operation"},
      {"params", J::object{{"kind", "check"}, {"scope", "document"},
                            {"textDocument", J::object{{"uri", uri}, {"version", 0}}}}}});
  require(stale_operation.size() == 1 && stale_operation.front().get("error") &&
              stale_operation.front().get("error")->get("code")->integer() == -32602,
          "LSP operation accepted stale document version");
  const J cancelled_id{42};
  service.register_request(cancelled_id);
  service.cancel_request(cancelled_id);
  service.begin_request(cancelled_id);
  const auto cancelled_operation = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", cancelled_id},
      {"method", "sagan/operation"},
      {"params", J::object{{"kind", "check"}, {"scope", "document"},
                            {"textDocument", J::object{{"uri", uri}, {"version", 1}}}}}});
  service.end_request(cancelled_id);
  require(cancelled_operation.size() == 1 && cancelled_operation.front().get("result") &&
              cancelled_operation.front().get("result")->get("state") &&
              cancelled_operation.front().get("result")->get("state")->string() == "cancelled" &&
              cancelled_operation.front().get("result")->get("diagnostics")->elements()->empty(),
          "LSP operation cancellation did not return a structured empty result");
  const std::string native_uri = "untitled:sagan-native-operation";
  const auto native_opened = notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", native_uri}, {"version", 1},
                                            {"text", "print(42)\n"}}}});
  require(native_opened.size() == 2, "second operation document did not open");
  J::array native_events;
  service.set_notification_sink([&](const J &message) { native_events.push_back(message); });
  const auto native_run = request(service, "sagan/operation",
      J::object{{"kind", "run"}, {"scope", "document"},
                {"textDocument", J::object{{"uri", native_uri}, {"version", 1}}}});
  service.set_notification_sink({});
  require(native_run.get("state") && native_run.get("state")->string() == "completed" &&
              native_run.get("exitStatus") && native_run.get("exitStatus")->integer() == 0 &&
              native_run.get("stdout") &&
              (native_run.get("stdout")->string() == "42\n" ||
               native_run.get("stdout")->string() == "42\r\n") &&
              native_run.get("executable") && native_run.get("executable")->string() &&
              !native_events.empty(),
          "LSP native run did not report structured output and artifact");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", native_uri}}}});
  const std::string test_uri = "untitled:sagan-test-discovery";
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 1},
                                              {"text", "test \"orbit 🚀\" { print(1) }\n"}}}});
  const auto discovered = request(service, "sagan/tests/discover",
      J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 1}}}});
  require(discovered.get("schema") && discovered.get("schema")->string() == "sagan-tests-v1" &&
              discovered.get("state")->string() == "complete" &&
              discovered.get("tests")->elements()->size() == 1 &&
              discovered.get("tests")->elements()->front().get("name")->string() == "orbit 🚀" &&
              discovered.get("tests")->elements()->front().get("suites")->elements()->empty() &&
              discovered.get("tests")->elements()->front().get("range")->get("end")
                  ->get("character")->integer() == 15,
          "LSP test discovery did not expose stable UTF-16 test metadata");
  J::array selected_tests;
  selected_tests.push_back(*discovered.get("tests")->elements()->front().get("id"));
  J::array test_events;
  service.set_notification_sink([&](const J &message) { test_events.push_back(message); });
  const auto test_run = request(service, "sagan/tests/run",
      J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 1}}},
                {"testIds", std::move(selected_tests)}});
  service.set_notification_sink({});
  require(test_run.get("schema")->string() == "sagan-tests-v1" &&
              test_run.get("state")->string() == "complete" &&
              test_run.get("tests")->elements()->size() == 1 &&
              test_run.get("tests")->elements()->front().get("state")->string() == "passed" &&
              test_events.size() >= 3,
          "LSP selected-test run did not stream structured states");
  const auto project_path = std::filesystem::absolute(
      "tests/fixtures/modules/tests_project/main.sagan").lexically_normal();
  const auto project_uri = sagan::source::identity_from_path(
      sagan::source::document_id{}, project_path).uri.value;
  notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", project_uri}, {"version", 1},
          {"text", "module main\nimport tools\ntest \"root/🚀\" { print(1) }\n"}}}});
  const auto project_discovery = request(service, "sagan/tests/discover",
      J::object{{"scope", "project"}, {"projectUri", project_uri},
                {"textDocument", J::object{{"uri", project_uri}, {"version", 1}}}});
  require(project_discovery.get("state")->string() == "complete" &&
              project_discovery.get("tests")->elements()->size() == 2,
          "LSP project discovery did not include imported-module tests");
  J::array imported_selection;
  imported_selection.push_back(*project_discovery.get("tests")->elements()->front().get("id"));
  const auto project_run = request(service, "sagan/tests/run",
      J::object{{"scope", "project"}, {"projectUri", project_uri},
                {"textDocument", J::object{{"uri", project_uri}, {"version", 1}}},
                {"testIds", std::move(imported_selection)}});
  require(project_run.get("state")->string() == "complete" &&
              project_run.get("tests")->elements()->size() == 1 &&
              project_run.get("tests")->elements()->front().get("state")->string() == "passed",
          "LSP project test request did not execute a selected linked-module test");
  const auto package_uri = sagan::source::identity_from_path(sagan::source::document_id{},
      project_path.parent_path()).uri.value;
  const auto package_run = request(service, "sagan/tests/run",
      J::object{{"scope", "project"}, {"projectUri", package_uri},
                {"textDocument", J::object{{"uri", project_uri}, {"version", 1}}}});
  require(package_run.get("state")->string() == "complete" &&
              package_run.get("tests")->elements()->size() == 2 &&
              package_run.get("tests")->elements()->front().get("package")->string() ==
                  "tests-project",
          "LSP package-root test request lost package identity");
  J::array unknown_project_selection;
  unknown_project_selection.push_back("sagan-test-v1:unknown");
  const auto unknown_project_run = request(service, "sagan/tests/run",
      J::object{{"scope", "project"}, {"projectUri", package_uri},
                {"textDocument", J::object{{"uri", project_uri}, {"version", 1}}},
                {"testIds", std::move(unknown_project_selection)}});
  require(unknown_project_run.get("state")->string() == "incomplete" &&
              unknown_project_run.get("tests")->elements()->empty() &&
              !unknown_project_run.get("diagnostics")->elements()->empty(),
          "LSP project test run accepted an unknown stable ID");
  notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", project_uri}}}});
  const auto stale_discovery = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 43},
      {"method", "sagan/tests/discover"},
      {"params", J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 0}}}}}});
  require(stale_discovery.size() == 1 && stale_discovery.front().get("error") &&
              stale_discovery.front().get("error")->get("code")->integer() == -32602,
          "LSP test discovery accepted a stale document version");
  J::array incomplete_changes;
  incomplete_changes.push_back(J::object{{"text", "test \"orbit\" { print(\n"}});
  notify(service, "textDocument/didChange",
      J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 2}}},
                {"contentChanges", std::move(incomplete_changes)}});
  const auto incomplete_discovery = request(service, "sagan/tests/discover",
      J::object{{"textDocument", J::object{{"uri", test_uri}, {"version", 2}}}});
  require(incomplete_discovery.get("diagnostics") &&
              !incomplete_discovery.get("diagnostics")->elements()->empty() &&
              incomplete_discovery.get("tests"),
          "LSP test discovery did not return structured incomplete-source diagnostics");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", test_uri}}}});
  const auto hover = request(service, "textDocument/hover", at(uri, 2, 15));
  require(hover.get("contents"), "emoji function hover failed");
  const auto split_emoji = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 10},
                                                     {"method", "textDocument/hover"},
                                                     {"params", at(uri, 2, 16)}});
  require(split_emoji.size() == 1 && split_emoji.front().get("error") &&
              split_emoji.front().get("error")->get("code")->integer() == -32602,
          "LSP accepted a cursor in the middle of an emoji surrogate pair");
  const auto definition = request(service, "textDocument/definition", at(uri, 2, 15));
  require(definition.elements() && definition.elements()->size() == 1 &&
              definition.elements()->front().get("uri")->string() == uri,
          "definition did not resolve emoji call");
  const auto completion = request(service, "textDocument/completion", at(uri, 3, 8));
  require(completion.elements(), "completion did not return candidates");
  const auto symbols = request(service, "textDocument/documentSymbol",
                               J::object{{"textDocument", J::object{{"uri", uri}}}});
  require(symbols.elements() && symbols.elements()->size() >= 2,
          "document symbols did not expose both functions");
  const auto tokens = request(service, "textDocument/semanticTokens/full",
                              J::object{{"textDocument", J::object{{"uri", uri}}}});
  require(tokens.get("data") && tokens.get("data")->elements() &&
              !tokens.get("data")->elements()->empty(), "semantic tokens were empty");
  const auto unknown = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 8},
                                                 {"method", "textDocument/notAFeature"},
                                                 {"params", J::object{{"textDocument", J::object{{"uri", uri}}}}}});
  require(unknown.size() == 1 && unknown.front().get("error") &&
              unknown.front().get("error")->get("code")->integer() == -32601,
          "unknown LSP request did not return MethodNotFound");
  const auto references = request(service, "textDocument/references",
                                  J::object{{"textDocument", J::object{{"uri", uri}}},
                                            {"position", J::object{{"line", 2}, {"character", 15}}},
                                            {"context", J::object{{"includeDeclaration", true}}}});
  require(references.elements() && references.elements()->size() >= 2,
          "reference query lost the emoji declaration or call");
  const auto signature = request(service, "textDocument/signatureHelp", at(uri, 2, 18));
  require(signature.get("signatures"), "signature help did not return signatures");
  const auto highlights = request(service, "textDocument/documentHighlight", at(uri, 3, 9));
  require(highlights.elements() && highlights.elements()->size() >= 2,
          "document highlights did not include answer declaration and use");
  const auto selections = request(service, "textDocument/selectionRange",
                                  J::object{{"textDocument", J::object{{"uri", uri}}},
                                            {"positions", J::array{J::object{{"line", 2}, {"character", 15}}}}});
  require(selections.elements() && selections.elements()->size() == 1 &&
              selections.elements()->front().get("range"), "selection range did not cover emoji call");
  const auto folding = request(service, "textDocument/foldingRange",
                               J::object{{"textDocument", J::object{{"uri", uri}}}});
  require(folding.elements() && !folding.elements()->empty(), "folding query omitted main block");
  const auto inlays = request(service, "textDocument/inlayHint",
                              J::object{{"textDocument", J::object{{"uri", uri}}},
                                        {"range", J::object{{"start", J::object{{"line", 0}, {"character", 0}}},
                                                            {"end", J::object{{"line", 5}, {"character", 1}}}}}});
  require(inlays.elements(), "inlay hint query failed");
  const auto prepared_call = request(service, "textDocument/prepareCallHierarchy", at(uri, 2, 15));
  require(prepared_call.elements() && prepared_call.elements()->size() == 1,
          "call hierarchy preparation failed");
  const auto incoming = request(service, "callHierarchy/incomingCalls",
                                J::object{{"item", prepared_call.elements()->front()}});
  require(incoming.elements() && !incoming.elements()->empty(),
          "call hierarchy did not connect main to emoji function");
  const auto formatted = request(service, "textDocument/formatting",
                                 J::object{{"textDocument", J::object{{"uri", uri}}}});
  require(formatted.elements(), "formatting handler failed");
  const auto manifest_uri = sagan::source::identity_from_path(
      sagan::source::document_id{}, "tests/fixtures/catalog/consumer-alias/sagan.toml").uri.value;
  const auto manifest_source = sagan::source::disk_source_provider{}.read_path(
      "tests/fixtures/catalog/consumer-alias/sagan.toml");
  require(manifest_source.value.has_value(), "LSP manifest fixture was not readable");
  auto manifest_text = std::string(manifest_source.value->text());
  const auto manifest_name = manifest_text.find("name = \"consumer-alias\"");
  require(manifest_name != std::string::npos, "LSP manifest fixture has no package name");
  manifest_text.replace(manifest_name, std::string_view{"name = \"consumer-alias\""}.size(),
                        "  name  =  \"consumer-alias\"  ");
  static_cast<void>(notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", manifest_uri}, {"version", 31},
                                           {"text", manifest_text}}}}));
  const auto manifest_formatted = request(service, "textDocument/formatting",
      J::object{{"textDocument", J::object{{"uri", manifest_uri}}}});
  require(manifest_formatted.elements() && manifest_formatted.elements()->size() == 1 &&
              manifest_formatted.elements()->front().get("newText") &&
              manifest_formatted.elements()->front().get("newText")->string() ==
                  "name = \"consumer-alias\"",
          "LSP did not format a valid unsaved manifest");
  static_cast<void>(notify(service, "textDocument/didChange",
      J::object{{"textDocument", J::object{{"uri", manifest_uri}, {"version", 32}}},
                {"contentChanges", J::array{J::object{{"text", "[package]\nname = \"broken\"\nunknown = 1\n"}}}}}));
  const auto invalid_manifest_format = request(service, "textDocument/formatting",
      J::object{{"textDocument", J::object{{"uri", manifest_uri}}}});
  require(invalid_manifest_format.elements() && invalid_manifest_format.elements()->empty(),
          "LSP formatted an invalid manifest instead of refusing safely");
  static_cast<void>(notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", manifest_uri}}}}));
  const auto range_formatted = request(service, "textDocument/rangeFormatting",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"range", J::object{{"start", J::object{{"line", 1}, {"character", 0}}},
                                    {"end", J::object{{"line", 4}, {"character", 10}}}}}});
  require(range_formatted.elements(), "range formatting handler failed");
  const auto on_type = request(service, "textDocument/onTypeFormatting",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"position", J::object{{"line", 5}, {"character", 1}}}, {"ch", "}"}});
  require(on_type.elements(), "on-type formatting handler failed");
  const auto prepared_rename = request(service, "textDocument/prepareRename", at(uri, 2, 8));
  require(prepared_rename.get("range") && prepared_rename.get("placeholder") &&
              prepared_rename.get("placeholder")->string() == "answer",
          "F2 preparation did not select the local declaration name");
  const auto prepared_main = request(service, "textDocument/prepareRename", at(uri, 1, 4));
  require(prepared_main.get("range") && prepared_main.get("placeholder") &&
              prepared_main.get("placeholder")->string() == "main",
          "F2 preparation rejected an ordinary function named main");
  const auto renamed = request(service, "textDocument/rename",
                               J::object{{"textDocument", J::object{{"uri", uri}}},
                                         {"position", J::object{{"line", 3}, {"character", 9}}},
                                         {"newName", "result"}});
  require(renamed.get("documentChanges"), "safe local rename was not converted to workspace edits");
  const auto renamed_declaration = request(service, "textDocument/rename",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"position", J::object{{"line", 2}, {"character", 8}}},
                {"newName", "altitude"}});
  require(renamed_declaration.get("documentChanges") &&
              renamed_declaration.get("documentChanges")->elements()->size() == 1 &&
              renamed_declaration.get("documentChanges")->elements()->front().get("edits")->elements()->size() == 2,
          "F2 local rename from the declaration did not include the declaration and use");
  const auto renamed_function = request(service, "textDocument/rename",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"position", J::object{{"line", 2}, {"character", 15}}},
                {"newName", "launch"}});
  require(renamed_function.get("documentChanges") &&
              renamed_function.get("documentChanges")->elements()->size() == 1 &&
              renamed_function.get("documentChanges")->elements()->front().get("edits")->elements()->size() == 2,
          "F2 function rename did not include the declaration and call");
  const auto invalid_rename = service.handle(J::object{
      {"jsonrpc", "2.0"}, {"id", 11}, {"method", "textDocument/rename"},
      {"params", J::object{{"textDocument", J::object{{"uri", uri}}},
                            {"position", J::object{{"line", 2}, {"character", 8}}},
                            {"newName", "return"}}}});
  require(invalid_rename.size() == 1 && invalid_rename.front().get("error") &&
              invalid_rename.front().get("error")->get("code")->integer() == -32803 &&
              invalid_rename.front().get("error")->get("message")->string() ==
                  "Proposed name is not a valid binding identifier",
          "F2 invalid-name refusal did not preserve the compiler-owned explanation");
  const auto conflicting_rename = service.handle(J::object{
      {"jsonrpc", "2.0"}, {"id", 12}, {"method", "textDocument/rename"},
      {"params", J::object{{"textDocument", J::object{{"uri", uri}}},
                            {"position", J::object{{"line", 2}, {"character", 8}}},
                            {"newName", "main"}}}});
  require(conflicting_rename.size() == 1 && conflicting_rename.front().get("error") &&
              conflicting_rename.front().get("error")->get("code")->integer() == -32803 &&
              conflicting_rename.front().get("error")->get("message")->string() ==
                  "Proposed name already exists in the semantic scope set",
          "F2 collision refusal did not preserve the compiler-owned explanation");
  const std::string independent_uri = "untitled:independent-f2-scopes";
  static_cast<void>(notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", independent_uri}, {"version", 1},
          {"text", "fun first(): Int { let value = 1\n return value }\n"
                   "fun second(): Int { let other = 2\n return other }\n"}}}}));
  const auto independent_f2 = request(service, "textDocument/rename",
      J::object{{"textDocument", J::object{{"uri", independent_uri}}},
                {"position", J::object{{"line", 2}, {"character", 24}}},
                {"newName", "value"}});
  require(independent_f2.get("documentChanges") &&
              independent_f2.get("documentChanges")->elements()->size() == 1 &&
              independent_f2.get("documentChanges")->elements()->front().get("edits")->elements()->size() == 2,
          "F2 rejected an independent-scope name through the LSP transport");
  static_cast<void>(notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", independent_uri}}}}));
  const std::string local_type_uri = "untitled:local-type-f2";
  static_cast<void>(notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", local_type_uri}, {"version", 1},
          {"text", "class Probe { new() {} }\nlet probe = Probe()\nprint(probe)\n"}}}}));
  const auto prepared_local_type = request(service, "textDocument/prepareRename",
      at(local_type_uri, 1, 13));
  require(prepared_local_type.get("placeholder") &&
              prepared_local_type.get("placeholder")->string() == "Probe",
          "F2 preparation rejected an unexported constructor reference");
  const auto renamed_type = request(service, "textDocument/rename",
      J::object{{"textDocument", J::object{{"uri", local_type_uri}}},
                {"position", J::object{{"line", 1}, {"character", 13}}},
                {"newName", "Sensor"}});
  require(renamed_type.get("documentChanges") &&
              renamed_type.get("documentChanges")->elements()->size() == 1 &&
              renamed_type.get("documentChanges")->elements()->front().get("edits")->elements()->size() == 2,
          "F2 did not rename an unexported class from its constructor reference");
  static_cast<void>(notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", local_type_uri}}}}));
  const auto actions = request(service, "textDocument/codeAction",
                               J::object{{"textDocument", J::object{{"uri", uri}}},
                                         {"context", J::object{{"diagnostics", J::array{}}}},
                                         {"range", J::object{{"start", J::object{{"line", 0}, {"character", 0}}},
                                                             {"end", J::object{{"line", 0}, {"character", 0}}}}}});
  require(actions.elements(), "code actions handler failed");
  service.register_request(99);
  service.cancel_request(99);
  service.begin_request(99);
  const auto cancelled = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 99},
                                                  {"method", "textDocument/hover"},
                                                  {"params", at(uri, 2, 15)}});
  service.end_request(99);
  require(cancelled.size() == 1 && cancelled.front().get("error") &&
              cancelled.front().get("error")->get("code")->integer() == -32800,
          "cancelled query did not return LSP RequestCancelled");
  const auto changed_line = notify(service, "textDocument/didChange",
                                   J::object{{"textDocument", J::object{{"uri", uri}, {"version", 2}}},
                                             {"contentChanges", J::array{J::object{
                                                 {"range", J::object{{"start", J::object{{"line", 4}, {"character", 9}}},
                                                                     {"end", J::object{{"line", 4}, {"character", 10}}}}},
                                                 {"text", "1"}}}}});
  require(changed_line.size() == 1 &&
              changed_line.front().get("params")->get("diagnostics")->elements()->empty() &&
              service.documents().read(sagan::source::document_uri{uri}).value->version() == 2,
          "incremental UTF-16 edit did not update the document version");
  const auto broken = notify(service, "textDocument/didChange",
                             J::object{{"textDocument", J::object{{"uri", uri}, {"version", 3}}},
                                       {"contentChanges", J::array{J::object{{"text", "fun main(): Int => missing\n"}}}}});
  require(broken.size() == 1 &&
              !broken.front().get("params")->get("diagnostics")->elements()->empty(),
          "didChange did not publish a compiler diagnostic");
  const auto repaired = notify(service, "textDocument/didChange",
                               J::object{{"textDocument", J::object{{"uri", uri}, {"version", 4}}},
                                         {"contentChanges", J::array{J::object{{"text", text}}}}});
  require(repaired.size() == 1 &&
              repaired.front().get("params")->get("diagnostics")->elements()->empty(),
          "repaired source retained stale diagnostics");
  const auto closed = notify(service, "textDocument/didClose",
                             J::object{{"textDocument", J::object{{"uri", uri}}}});
  require(closed.size() == 1 && closed.front().get("params")->get("diagnostics")->elements()->empty(),
          "didClose did not clear diagnostics");
  const std::string sugar_uri = "file:///lsp-sugar.sagan";
  const std::string sugar_text =
      "fun main(): Int {\n"
      "  let a = 0\n"
      "  let b = 1\n"
      "  let indices = 5.times\n"
      "  let phi = (1 + 5 ^ 0.5) / 2\n"
      "  let raised = phi ^ b\n"
      "  let rounded = Int.round(raised)\n"
      "  let direction = <3.0, 4.0>\n"
      "  let heading = direction.normalized()\n"
      "  a, b = b, a + b\n"
      "  return indices[0] + a + b + rounded\n"
      "}\n";
  const auto sugar_opened = notify(service, "textDocument/didOpen",
                                   J::object{{"textDocument", J::object{{"uri", sugar_uri},
                                                                        {"version", 1}, {"text", sugar_text}}}});
  require(sugar_opened.size() == 1 && sugar_opened.front().get("params")->get("diagnostics") &&
              sugar_opened.front().get("params")->get("diagnostics")->elements()->empty(),
          "LSP rejected parallel reassignment, times, or mixed exponentiation");
  const auto sugar_completion = request(service, "textDocument/completion", at(sugar_uri, 3, 18));
  require(sugar_completion.elements() && sugar_completion.elements()->size() == 1 &&
              sugar_completion.elements()->front().get("label")->string() == "times",
          "LSP did not offer the integer times member");
  const auto sugar_hover = request(service, "textDocument/hover", at(sugar_uri, 3, 18));
  require(sugar_hover.get("contents") &&
              sagan::lsp::json::serialize(sugar_hover).find("Array<Int64>") != std::string::npos,
          "LSP hover did not describe the built-in times array");
  const auto round_completion = request(service, "textDocument/completion", at(sugar_uri, 6, 20));
  require(round_completion.elements() && round_completion.elements()->size() == 1 &&
              round_completion.elements()->front().get("label")->string() == "round",
          "LSP did not offer Int.round completion");
  const auto vector_completion = request(service, "textDocument/completion", at(sugar_uri, 8, 29));
  require(vector_completion.elements() &&
              std::any_of(vector_completion.elements()->begin(), vector_completion.elements()->end(),
                          [](const auto &entry)
                          {
                            return entry.get("label") && entry.get("label")->string() == "normalized";
                          }),
          "LSP did not offer Vector.normalized completion");
  const auto round_hover = request(service, "textDocument/hover", at(sugar_uri, 6, 20));
  require(round_hover.get("contents") &&
              sagan::lsp::json::serialize(round_hover).find("Int64") != std::string::npos,
          "LSP hover did not describe Int.round");
  const auto round_signature = request(service, "textDocument/signatureHelp", at(sugar_uri, 6, 26));
  require(round_signature.get("signatures") &&
              sagan::lsp::json::serialize(round_signature).find("Int64") != std::string::npos,
          "LSP signature help did not describe Int.round");
  const auto sugar_inlay = request(service, "textDocument/inlayHint",
      J::object{{"textDocument", J::object{{"uri", sugar_uri}}},
                {"range", J::object{{"start", J::object{{"line", 0}, {"character", 0}}},
                                    {"end", J::object{{"line", 9}, {"character", 1}}}}}});
  require(sugar_inlay.elements(), "LSP inlay query failed for mixed exponentiation");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", sugar_uri}}}});
  const std::string private_uri = "file:///lsp-private-member.sagan";
  const std::string private_text =
      "class Counter {\n"
      "  let .value: Int\n"
      "  new(start: Int) { self.value = start }\n"
      "  fun .advance!(): Int {\n"
      "    self.value += 1\n"
      "    return self.value\n"
      "  }\n"
      "  fun run!(): Int => self.advance!()\n"
      "}\n"
      "fun main(): Int {\n"
      "  let counter = Counter(0)\n"
      "  return counter.run!()\n"
      "}\n";
  const auto private_opened = notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", private_uri},
                                             {"version", 1}, {"text", private_text}}}});
  require(private_opened.size() == 1 &&
              private_opened.front().get("params")->get("diagnostics")->elements()->empty(),
          "private-member LSP fixture produced diagnostics");
  const auto prepared_private = request(service, "textDocument/prepareRename", at(private_uri, 3, 7));
  require(prepared_private.get("placeholder") &&
              prepared_private.get("placeholder")->string() == "advance!",
          "F2 preparation did not select the private mutating method");
  const auto renamed_private = request(service, "textDocument/rename",
      J::object{{"textDocument", J::object{{"uri", private_uri}}},
                {"position", J::object{{"line", 3}, {"character", 7}}},
                {"newName", "step!"}});
  require(renamed_private.get("documentChanges") &&
              renamed_private.get("documentChanges")->elements()->front().get("edits")->elements()->size() == 2,
          "F2 private mutating-method rename did not include its declaration and reference");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", private_uri}}}});
  const sagan::source::disk_source_provider disk;
  const auto module = disk.read_path("tests/fixtures/modules/module_demo/main.sagan");
  require(static_cast<bool>(module), "module fixture missing");
  const auto module_uri = module.value->identity().uri.value;
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", module_uri}, {"version", 1},
                                              {"text", std::string(module.value->text())}}}});
  const auto imported_definition = request(service, "textDocument/definition", at(module_uri, 6, 18));
  require(imported_definition.elements() && imported_definition.elements()->size() == 1 &&
              imported_definition.elements()->front().get("uri") &&
              imported_definition.elements()->front().get("uri")->string()->find("guidance.sagan") !=
                  std::string_view::npos,
          "cross-module definition did not navigate to guidance");
  const auto links = request(service, "textDocument/documentLink",
                             J::object{{"textDocument", J::object{{"uri", module_uri}}}});
  require(links.elements() && links.elements()->size() == 2,
          "module imports did not become document links");
  const auto workspace_symbols = request(service, "workspace/symbol", J::object{{"query", "calculate"}});
  require(workspace_symbols.elements() && !workspace_symbols.elements()->empty(),
          "workspace symbol search did not find exported course");
  const auto watched = notify(service, "workspace/didChangeWatchedFiles",
                              J::object{{"changes", J::array{}}});
  require(watched.size() == 1 && watched.front().get("method")->string() ==
              "textDocument/publishDiagnostics", "file notification did not refresh open diagnostics");
  const auto guidance = disk.read_path("tests/fixtures/modules/module_demo/guidance.sagan");
  require(static_cast<bool>(guidance), "guidance fixture missing");
  std::string altered_guidance(guidance.value->text());
  const auto export_at = altered_guidance.find("export calculate as course");
  require(export_at != std::string::npos, "guidance export fixture changed");
  altered_guidance.replace(export_at, std::string("export calculate as course").size(),
                           "export calculate as orbit");
  const auto overlay_updates = notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", guidance.value->identity().uri.value},
                                           {"version", 1}, {"text", altered_guidance}}}});
  require(overlay_updates.size() == 2 &&
              !overlay_updates.front().get("params")->get("diagnostics")->elements()->empty(),
          "unsaved imported overlay did not invalidate the importing document");
  const auto restored_updates = notify(service, "textDocument/didClose",
      J::object{{"textDocument", J::object{{"uri", guidance.value->identity().uri.value}}}});
  require(restored_updates.size() == 2 &&
              restored_updates.back().get("params")->get("diagnostics")->elements()->empty(),
          "closing imported overlay did not restore importer diagnostics");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", module_uri}}}});
  const auto collision = disk.read_path("tests/fixtures/modules/completion_collision/main.sagan");
  require(static_cast<bool>(collision), "completion collision fixture missing");
  const auto collision_uri = collision.value->identity().uri.value;
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", collision_uri}, {"version", 1},
                                              {"text", std::string(collision.value->text())}}}});
  const auto collision_completion = request(service, "textDocument/completion",
                                            at(collision_uri, 6, 2));
  require(collision_completion.elements(), "colliding export completion returned no candidates");
  std::vector<std::string> collision_details;
  for (const auto &item : *collision_completion.elements())
    if (item.get("label") && item.get("label")->string() == "answer")
    {
      const auto *edits = item.get("additionalTextEdits");
      require(edits && edits->elements() && edits->elements()->size() == 1 &&
                  item.get("detail") && item.get("detail")->string(),
              "colliding export completion omitted its module-specific edit or detail");
      collision_details.push_back(std::string(*item.get("detail")->string()));
    }
  require(collision_details == std::vector<std::string>{"(): Int (from alpha)",
                                                       "(): Int (from beta)"},
          "standard LSP completion did not distinguish colliding module exports");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", collision_uri}}}});
  const std::string composition_uri = "untitled:composition-lsp";
  const std::string composition =
      "face Readable { fun read(): Int\n  fun describe(): String => \"readable\" }\n"
      "class Probe has Readable { fun read(): Int => 1 }\n"
      "fun main(): Int {\n  let probe = Probe()\n  return probe.read()\n}\n";
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", composition_uri}, {"version", 1},
                                              {"text", composition}}}});
  const auto implementations = request(service, "textDocument/implementation", at(composition_uri, 0, 6));
  require(implementations.elements() && implementations.elements()->size() == 1,
          "face implementation request missed Probe");
  const auto type_definition = request(service, "textDocument/typeDefinition", at(composition_uri, 5, 9));
  require(type_definition.elements() && type_definition.elements()->size() == 1,
          "inferred Probe type definition was not navigable");
  const auto prepared_type = request(service, "textDocument/prepareTypeHierarchy", at(composition_uri, 0, 6));
  require(prepared_type.elements() && prepared_type.elements()->size() == 1,
          "type hierarchy preparation missed Readable");
  const auto subtypes = request(service, "typeHierarchy/subtypes",
                                J::object{{"item", prepared_type.elements()->front()}});
  require(subtypes.elements() && subtypes.elements()->size() == 1 &&
              subtypes.elements()->front().get("name")->string() == "Probe",
          "type hierarchy did not return Probe");
  const auto supertypes = request(service, "typeHierarchy/supertypes",
                                  J::object{{"item", subtypes.elements()->front()}});
  require(supertypes.elements() && supertypes.elements()->size() == 1 &&
              supertypes.elements()->front().get("name")->string() == "Readable",
          "hierarchy item did not retain an identity-based follow-up position");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", composition_uri}}}});
  const std::string face_field_uri = "untitled:face-field-lsp";
  const std::string face_field_valid =
      "face Massive { let .mass: Float<kilogram>\n"
      "  fun getMass(): Float<kilogram> => self.mass }\n"
      "class Body has Massive { new() { self.mass = 1.0 kilogram } }\n"
      "let body = Body()\nassert(body.getMass() == 1.0 kilogram)\n";
  const auto face_field_opened = notify(service, "textDocument/didOpen",
      J::object{{"textDocument", J::object{{"uri", face_field_uri}, {"version", 1},
                                           {"text", face_field_valid}}}});
  require(face_field_opened.size() == 1 &&
              face_field_opened.front().get("params")->get("diagnostics")->elements()->empty(),
          "LSP rejected a valid face field promise");
  const auto face_field_definition = request(service, "textDocument/definition",
                                             at(face_field_uri, 2, 39));
  require(face_field_definition.elements() && face_field_definition.elements()->size() == 1 &&
              face_field_definition.elements()->front().get("range")->get("start")
                  ->get("line")->integer() == 0,
          "LSP did not navigate synthesized face storage to its promise");
  const std::string face_field_invalid =
      "face Massive { let .mass: Float<kilogram>\n"
      "  fun getMass(): Float<kilogram> => self.mass }\n"
      "class Body has Massive { new() { } }\n";
  const auto face_field_changed = notify(service, "textDocument/didChange",
      J::object{{"textDocument", J::object{{"uri", face_field_uri}, {"version", 2}}},
                {"contentChanges", J::array{J::object{{"text", face_field_invalid}}}}});
  require(face_field_changed.size() == 1 &&
              !face_field_changed.front().get("params")->get("diagnostics")->elements()->empty(),
          "LSP missed a missing face field promise");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", face_field_uri}}}});
  const std::string incomplete_uri = "untitled:incomplete-lsp";
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", incomplete_uri}, {"version", 1},
                                              {"text", "fun main(): Int {\n  return 0\n"}}}});
  const auto fixes = request(service, "textDocument/codeAction",
                             J::object{{"textDocument", J::object{{"uri", incomplete_uri}}},
                                       {"context", J::object{{"diagnostics", J::array{}},
                                                              {"only", J::array{"quickfix"}}}},
                                       {"range", J::object{{"start", J::object{{"line", 1}, {"character", 0}}},
                                                           {"end", J::object{{"line", 1}, {"character", 8}}}}}});
  require(fixes.elements() && !fixes.elements()->empty() &&
              fixes.elements()->front().get("edit"),
          "incomplete document lost its compiler-issued quick fix");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", incomplete_uri}}}});
  // Repeated incomplete edits must not leak an older diagnostic or overwrite a
  // newer overlay, even while another document remains open in the workspace.
  const std::string rapid_uri = "untitled:rapid-lsp";
  const std::string neighbor_uri = "untitled:neighbor-lsp";
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", rapid_uri}, {"version", 1},
                                              {"text", "fun main(): Int => 0\n"}}}});
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", neighbor_uri}, {"version", 1},
                                              {"text", "fun neighbor(): Int => 1\n"}}}});
  for (int version = 2; version <= 25; ++version)
  {
    const std::string source = version % 2 == 0 ? "fun main(): Int => missing\n"
                                                : "fun main(): Int => 0\n";
    const auto updates = notify(service, "textDocument/didChange",
        J::object{{"textDocument", J::object{{"uri", rapid_uri}, {"version", version}}},
                  {"contentChanges", J::array{J::object{{"text", source}}}}});
    require(updates.size() == 2 &&
                updates.front().get("params")->get("version")->integer() == version &&
                (updates.front().get("params")->get("diagnostics")->elements()->empty() ==
                 (version % 2 != 0)) &&
                updates.back().get("params")->get("version")->integer() == 1 &&
                updates.back().get("params")->get("diagnostics")->elements()->empty(),
            "rapid edit published stale or cross-document diagnostics");
    request(service, "textDocument/semanticTokens/full",
            J::object{{"textDocument", J::object{{"uri", rapid_uri}}}});
  }
  const auto stale = notify(service, "textDocument/didChange",
      J::object{{"textDocument", J::object{{"uri", rapid_uri}, {"version", 24}}},
                {"contentChanges", J::array{J::object{{"text", "fun main(): Int => 99\n"}}}}});
  require(stale.empty() &&
              service.documents().read(sagan::source::document_uri{rapid_uri}).value->version() == 25,
          "out-of-order edit changed the newer document version");
  const auto malformed_updates = notify(service, "textDocument/didChange",
      J::object{{"textDocument", J::object{{"uri", rapid_uri}, {"version", 26}}},
                {"contentChanges", J::array{J::object{{"text", "fun main( {\n"}}}}});
  require(malformed_updates.size() == 2 &&
              !malformed_updates.front().get("params")->get("diagnostics")->elements()->empty(),
          "malformed partial source did not produce a bounded diagnostic response");
  const auto rapid_closed = notify(service, "textDocument/didClose",
                                   J::object{{"textDocument", J::object{{"uri", rapid_uri}}}});
  require(rapid_closed.size() == 2 &&
              rapid_closed.front().get("params")->get("diagnostics")->elements()->empty() &&
              !service.documents().is_open(sagan::source::document_uri{rapid_uri}),
          "closing the rapidly edited document did not clean up its overlay");
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", neighbor_uri}}}});
  for (int iteration = 0; iteration < 40; ++iteration)
  {
    const auto recycled_uri = "untitled:recycled-" + std::to_string(iteration);
    const auto recycled_text = iteration % 5 == 0 ? "fun main( {\n" : "fun main(): Int => 0\n";
    const auto recycled_opened = notify(service, "textDocument/didOpen",
           J::object{{"textDocument", J::object{{"uri", recycled_uri}, {"version", 1},
                                                {"text", recycled_text}}}});
    require(recycled_opened.size() == 1 &&
                recycled_opened.front().get("params")->get("diagnostics")->elements()->size() <= 32,
            "short partial source produced unbounded diagnostics");
    for (int character = -1; character <= 25; ++character)
    {
      const auto answer = service.handle(J::object{{"jsonrpc", "2.0"}, {"id", 1000 + character},
          {"method", "textDocument/hover"}, {"params", at(recycled_uri, 0, character)}});
      require(answer.size() == 1 && (answer.front().get("result") || answer.front().get("error")),
              "cursor-boundary query did not return a bounded result");
    }
    notify(service, "textDocument/didClose",
           J::object{{"textDocument", J::object{{"uri", recycled_uri}}}});
    require(!service.documents().is_open(sagan::source::document_uri{recycled_uri}),
            "repeated close retained an overlay");
  }
  const std::string edit_uri = "untitled:edit-boundaries";
  notify(service, "textDocument/didOpen",
         J::object{{"textDocument", J::object{{"uri", edit_uri}, {"version", 1},
                                              {"text", "fun 🚀(): Int => 0\n"}}}});
  int edit_version = 1;
  for (int character = -2; character <= 40; ++character)
  {
    const auto edited = notify(service, "textDocument/didChange",
        J::object{{"textDocument", J::object{{"uri", edit_uri}, {"version", edit_version + 1}}},
                  {"contentChanges", J::array{J::object{
                      {"range", J::object{{"start", J::object{{"line", 0}, {"character", character}}},
                                          {"end", J::object{{"line", 0}, {"character", character}}}}},
                      {"text", ""}}}}});
    if (!edited.empty()) ++edit_version;
    require(service.documents().read(sagan::source::document_uri{edit_uri}).value->version() ==
                edit_version &&
                (character != 5 || edited.empty()),
            "UTF-16 edit boundary corrupted the document or split an emoji");
  }
  notify(service, "textDocument/didClose",
         J::object{{"textDocument", J::object{{"uri", edit_uri}}}});
  request(service, "shutdown");
  notify(service, "exit");
  require(service.should_exit(), "exit did not stop the server");

  server framed;
  const auto encoded = json::serialize(J::object{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}});
  std::istringstream input("Content-Length: " + std::to_string(encoded.size()) + "\r\n\r\n" + encoded);
  std::ostringstream output;
  require(run(input, output, framed) == 0 && output.str().starts_with("Content-Length: ") &&
              output.str().find("\r\n\r\n{") != std::string::npos,
          "stdio framing did not keep protocol output clean");
  server malformed;
  const std::string invalid = "{oops";
  std::istringstream malformed_input("Content-Length: " + std::to_string(invalid.size()) +
                                     "\r\n\r\n" + invalid);
  std::ostringstream malformed_output;
  require(run(malformed_input, malformed_output, malformed) == 0 &&
              malformed_output.str().find("-32700") != std::string::npos,
          "invalid JSON did not receive a parse-error response");
  server oversized;
  std::istringstream oversized_input("Content-Length: 16777217\r\n\r\n");
  std::ostringstream oversized_output;
  require(run(oversized_input, oversized_output, oversized) == 1 &&
              oversized_output.str().empty(),
          "oversized protocol frame was not rejected before allocation");
  server truncated;
  std::istringstream truncated_input("Content-Length: 20\r\n\r\n{}");
  std::ostringstream truncated_output;
  require(run(truncated_input, truncated_output, truncated) == 1 &&
              truncated_output.str().empty(),
          "truncated protocol frame was not rejected");
  server lifecycle;
  const auto lifecycle_input =
      frame(J::object{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}) +
      frame(J::object{{"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
                      {"params", J::object{{"textDocument", J::object{{"uri", "untitled:framed"},
                                                                        {"version", 1},
                                                                        {"text", "fun main(): Int => 0\n"}}}}}}) +
      frame(J::object{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "textDocument/documentSymbol"},
                      {"params", J::object{{"textDocument", J::object{{"uri", "untitled:framed"}}}}}}) +
      frame(J::object{{"jsonrpc", "2.0"}, {"id", 3}, {"method", "shutdown"}}) +
      frame(J::object{{"jsonrpc", "2.0"}, {"method", "exit"}});
  std::istringstream lifecycle_stream(lifecycle_input);
  std::ostringstream lifecycle_output;
  require(run(lifecycle_stream, lifecycle_output, lifecycle) == 0 && lifecycle.should_exit() &&
              lifecycle_output.str().find("textDocument/publishDiagnostics") != std::string::npos &&
              lifecycle_output.str().find("\"id\":2") != std::string::npos &&
              lifecycle_output.str().find("\"id\":3") != std::string::npos,
          "framed lifecycle did not dispatch notifications, queries, and shutdown");
  std::cout << "Sagan input:\n" << text
            << "Hover: " << json::serialize(hover) << '\n'
            << "Definition: " << json::serialize(definition) << '\n'
            << "Diagnostics after incomplete edit: "
            << json::serialize(*broken.front().get("params")->get("diagnostics")) << '\n'
            << "Sagan LSP lifecycle, Unicode, queries, diagnostics, and framing passed.\n";
  return 0;
}
