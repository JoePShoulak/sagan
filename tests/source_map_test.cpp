#include "../src/codegen/cpp_generator.hpp"
#include "../src/language_service/operations.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/semantic/analyzer.hpp"
#include "../src/semantic/type_checker.hpp"
#include "../src/source/source.hpp"
#include "../src/source/provider.hpp"
#include "../src/syntax/syntax.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
  auto require(const bool condition, const char *message) -> void
  {
    if (!condition) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  const std::string text = "fun main(): Int {\n  return 42\n}\n";
  const sagan::source::document_snapshot document(
      {{sagan::source::document_id{1}, sagan::source::document_uri{"untitled:source-map"}, {}},
       1, text});
  const auto parsed = sagan::syntax::analyze(document, {.recover = false});
  require(parsed.value && parsed.value->strict_ast, "source-map fixture did not parse");
  const auto &tree = *parsed.value->strict_ast;
  const auto model = semantic::analyze(tree);
  const auto types = semantic::check_types(tree);
  const auto generated = codegen::generate_cpp_mapped(tree, types, "demo.sagan");
  require(generated.text.find("sagan_source_scope") != std::string::npos &&
              codegen::generate_cpp(tree, types).find("sagan_source_scope") == std::string::npos &&
              !generated.mappings.empty() &&
              codegen::source_map_schema_version == std::string_view{"sagan-cpp-source-map-v1"},
          "mapped generation changed the generated C++ or lost its schema");
  const auto position = generated.text.rfind("return 42;");
  require(position != std::string::npos, "generated return was not found");
  const auto *mapped = codegen::source_for_generated_offset(generated, position);
  const auto metadata = sagan::language_service::derive_debug_metadata(document, model, types, generated);
  require(mapped && mapped->breakpoint && mapped->generated_function == "main" &&
              mapped->source_path && mapped->source_path->filename() == "demo.sagan" &&
              mapped->source.begin <= static_cast<int>(text.find("return 42")) &&
              static_cast<int>(text.find("return 42")) < mapped->source.end,
          "return statement did not round-trip to the Sagan source range");
  const auto line = 1 + std::count(generated.text.begin(),
                                   generated.text.begin() + static_cast<std::ptrdiff_t>(position), '\n');
  const auto line_begin = generated.text.rfind('\n', position);
  const auto column = position - (line_begin == std::string::npos ? 0 : line_begin + 1) + 1;
  require(codegen::generated_offset(generated.text, static_cast<std::size_t>(line), column) == position &&
              !codegen::generated_offset(generated.text, 0, 1) &&
              !codegen::generated_offset(generated.text, 1, generated.text.size() + 1) &&
              codegen::source_for_generated_offset(generated, generated.text.size()) == nullptr,
          "generated line/column and offset conversion failed bounds checks");
  const auto frame = sagan::language_service::source_for_stack_frame(
      metadata, generated, "main", static_cast<std::size_t>(line), column);
  require(frame && frame->document == document.identity().id &&
              frame->bytes.begin == text.find("return 42") &&
              !sagan::language_service::source_for_stack_frame(metadata, generated, "missing",
                                                                 static_cast<std::size_t>(line), column),
          "generated stack-frame location did not map to a Sagan breakpoint");
  const auto toolchain = sagan::language_service::map_toolchain_errors(
      document, generated, "program.cpp:" + std::to_string(line) + ":" +
                           std::to_string(column) + ": error: synthetic compiler failure\n");
  require(toolchain.size() == 1 && toolchain.front().owner == sagan::diagnostics::phase::build &&
              toolchain.front().primary.document == document.identity().id &&
              toolchain.front().primary.bytes.begin == text.find("return 42") &&
              toolchain.front().message == "synthetic compiler failure" &&
              !toolchain.front().notes.empty(),
          "native toolchain error did not map to the Sagan source range");

  const auto linked = modules::link("tests/fixtures/modules/module_demo/main.sagan");
  static_cast<void>(semantic::analyze(linked));
  const auto linked_types = semantic::check_types(linked);
  const auto linked_cpp = codegen::generate_cpp_mapped(linked, linked_types);
  require(std::any_of(linked_cpp.mappings.begin(), linked_cpp.mappings.end(), [](const auto &entry)
          {
            return entry.source_path && entry.source_path->filename() == "guidance.sagan" &&
                   !entry.generated_function.empty();
          }) &&
              std::any_of(linked_cpp.mappings.begin(), linked_cpp.mappings.end(), [](const auto &entry)
          {
            return entry.source_path && entry.source_path->filename() == "main.sagan" &&
                   entry.generated_function == "main";
          }),
          "linked declarations lost their original module paths");
  const auto guidance_mapping = std::find_if(linked_cpp.mappings.begin(), linked_cpp.mappings.end(),
                                              [](const auto &entry)
  { return entry.breakpoint && entry.source_path &&
           entry.source_path->filename() == "guidance.sagan"; });
  require(guidance_mapping != linked_cpp.mappings.end(),
          "linked guidance had no mapped breakpoint");
  const auto linked_position = guidance_mapping->generated_begin;
  const auto linked_line = 1 + std::count(linked_cpp.text.begin(),
                                         linked_cpp.text.begin() +
                                             static_cast<std::ptrdiff_t>(linked_position), '\n');
  const auto linked_line_begin = linked_cpp.text.rfind('\n', linked_position);
  const auto linked_column = linked_position -
      (linked_line_begin == std::string::npos ? 0 : linked_line_begin + 1) + 1;
  const sagan::source::disk_source_provider disk;
  const auto main_document = disk.read_path("tests/fixtures/modules/module_demo/main.sagan");
  const auto guidance_document = disk.read_path("tests/fixtures/modules/module_demo/guidance.sagan");
  require(main_document && guidance_document, "linked source documents were unavailable");
  const auto linked_issues = sagan::language_service::map_toolchain_errors(
      *main_document.value, linked_cpp,
      "program.cpp:" + std::to_string(linked_line) + ":" +
          std::to_string(linked_column) + ": error: linked compiler failure\n", &disk);
  require(linked_issues.size() == 1 &&
              linked_issues.front().primary.document == guidance_document.value->identity().id,
          "linked compiler error did not map to the imported module identity");
  return 0;
}
