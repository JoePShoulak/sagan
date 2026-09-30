#include "../src/language_service/workspace.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/source/provider.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

auto main() -> int
{
  using namespace sagan;
  const auto guidance_path = std::filesystem::absolute("examples/module_demo/guidance.sagan").lexically_normal();
  const auto entry_path = std::filesystem::absolute("examples/module_demo/main.sagan").lexically_normal();
  const auto guidance = source::identity_from_path(source::document_id{1}, guidance_path);
  auto documents = std::make_shared<source::document_store>();

  const std::string unsaved =
      "module guidance\n"
      "fun replacement(value: Int): Int => value\n"
      "export replacement\n";
  if (!documents->open(guidance.uri, 1, unsaved)) return 1;

  std::cout << "Opened unsaved overlay: " << guidance.uri.value << " (version 1)\n";
  try
  {
    static_cast<void>(modules::resolve(entry_path, *documents));
    std::cerr << "Expected the unsaved export change to affect the importing module.\n";
    return 1;
  }
  catch (const std::runtime_error &error)
  {
    std::cout << "Importer sees overlay: " << error.what() << '\n';
  }

  language_service::workspace workspace(documents);
  const source::document_uri dependent{"untitled:dependent"};
  if (!workspace.open(dependent, 1, "fun result(): Int => 1\n")) return 1;
  workspace.set_dependencies(dependent, {guidance.uri});
  auto pending = workspace.begin_analysis(dependent);
  if (!pending || !workspace.replace(guidance.uri, 1, 2, unsaved + "// changed\n")) return 1;
  std::cout << "Dependency edit cancelled pending analysis: " << std::boolalpha
            << pending.value->cancellation.is_cancelled() << '\n';
  auto checked = language_service::analyze_document(pending.value->document);
  checked = workspace.finish_analysis(*pending.value, std::move(checked));
  std::cout << "Obsolete result state: " << diagnostics::state_name(checked.state) << '\n';

  if (!workspace.close(guidance.uri)) return 1;
  const auto disk_graph = modules::resolve(entry_path, workspace.documents());
  std::cout << "Closed overlay; disk module graph resolves " << disk_graph.modules.size() << " modules.\n";
  return checked.state == diagnostics::result_state::stale ? 0 : 1;
}
