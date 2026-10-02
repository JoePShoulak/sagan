#include "../src/driver/native_runner.hpp"
#include "../src/language_service/operations.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/source/provider.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
  const auto orbit_graph = modules::resolve_package("examples/two_body_demo");
  const auto render = driver::compilation_inputs_for(orbit_graph);
  if (!render.header || !render.source || !render.working_directory ||
      !std::filesystem::is_regular_file(*render.header) ||
      !std::filesystem::is_regular_file(*render.source) ||
      *render.working_directory != orbit_graph.package->package_root)
    throw std::runtime_error("Render dependency did not supply its native bridge");

  const auto numeric_graph = modules::resolve_package("examples/orbit_numeric_demo");
  const auto numeric = driver::compilation_inputs_for(numeric_graph);
  if (numeric.header || numeric.source || !numeric.libraries.empty())
    throw std::runtime_error("Headless physics unexpectedly links rendering");

  modules::module_graph missing;
  missing.modules.push_back(modules::module_info{
      "render.window", "tests/fixtures/render_missing_bridge/src/window.sagan", {}, {}});
  bool actionable = false;
  try { static_cast<void>(driver::compilation_inputs_for(missing)); }
  catch (const std::runtime_error &error)
  { actionable = std::string(error.what()).find("missing native/window_bridge.hpp") != std::string::npos; }
  if (!actionable) throw std::runtime_error("Missing native bridge was not diagnosed");
  const sagan::source::disk_source_provider disk;
  const auto built = sagan::language_service::build_project(
      "examples/two_body_demo", disk, "build/native-bridge-test");
  if (built.state != sagan::diagnostics::result_state::complete || !built.executable ||
      !std::filesystem::is_regular_file(*built.executable) || !built.working_directory ||
      *built.working_directory != orbit_graph.package->package_root)
    throw std::runtime_error("Reusable project build did not link the window bridge: " +
                             (built.diagnostics.empty() ? built.standard_error : built.diagnostics.front().message));
  std::cout << "Native render bridge discovery and missing-resource tests passed.\n";
}
