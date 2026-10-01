#include "../src/lsp/server.hpp"
#include "../src/source/provider.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>

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
              initialized.get("capabilities")->get("positionEncoding")->string() == "utf-16" &&
              initialized.get("experimental")->get("compiler")->get("capabilities")
                  ->get("languageServer")->boolean() == true,
          "initialize did not advertise implemented hover support");
  const std::string uri = "file:///lsp-demo.sagan";
  const std::string text = "fun 🚀(value: Int): Int => value + 2\nfun main(): Int {\n  let answer = 🚀(40)\n  print(answer)\n  return 0\n}\n";
  const auto opened = notify(service, "textDocument/didOpen",
                             J::object{{"textDocument", J::object{{"uri", uri},
                                                                  {"version", 1}, {"text", text}}}});
  require(opened.size() == 1 && opened.front().get("method") &&
              opened.front().get("method")->string() == "textDocument/publishDiagnostics" &&
              opened.front().get("params")->get("diagnostics")->elements()->empty(),
          "didOpen did not publish clean diagnostics");
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
  const auto range_formatted = request(service, "textDocument/rangeFormatting",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"range", J::object{{"start", J::object{{"line", 1}, {"character", 0}}},
                                    {"end", J::object{{"line", 4}, {"character", 10}}}}}});
  require(range_formatted.elements(), "range formatting handler failed");
  const auto on_type = request(service, "textDocument/onTypeFormatting",
      J::object{{"textDocument", J::object{{"uri", uri}}},
                {"position", J::object{{"line", 5}, {"character", 1}}}, {"ch", "}"}});
  require(on_type.elements(), "on-type formatting handler failed");
  const auto renamed = request(service, "textDocument/rename",
                               J::object{{"textDocument", J::object{{"uri", uri}}},
                                         {"position", J::object{{"line", 3}, {"character", 9}}},
                                         {"newName", "result"}});
  require(renamed.get("documentChanges"), "safe local rename was not converted to workspace edits");
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
  const std::string composition_uri = "untitled:composition-lsp";
  const std::string composition =
      "face Readable { fun read(): Int\n  fun describe(): String => \"readable\" }\n"
      "class Probe is Readable { fun read(): Int => 1 }\n"
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
