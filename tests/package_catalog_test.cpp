#include "../src/language_service/package_catalog.hpp"

#include <stdexcept>

namespace
{
  auto require(const bool value, const char *message) -> void
  {
    if (!value) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  using sagan::language_service::query_package_catalog;
  const auto path = "tests/fixtures/catalog/index.tsv";
  const auto catalog = query_package_catalog(path, "2.1.0");
  require(catalog.state == modules::package_index_state::ready &&
              catalog.packages.size() == 3 && !catalog.cancelled,
          "Package catalog lost available or installed versions");
  const auto orbit = query_package_catalog(path, "2.1.0", "orbit", 1);
  require(orbit.packages.size() == 1 && orbit.packages.front().identity == "orbit-tools@0.1.0" &&
              orbit.packages.front().compiler_compatible && orbit.packages.front().error.empty() &&
              orbit.packages.front().modules.size() == 1,
          "Installed package source did not enter the catalog");
  const auto &module = orbit.packages.front().modules.front();
  require(module.name == "main" && module.exports.size() == 2 &&
              module.exports.front().public_name == "orbit_answer" &&
              module.exports.front().kind == "function" &&
              module.exports.front().signature.find("Int") != std::string::npos &&
              !module.exports.front().symbol_id.empty() &&
              module.exports.front().source_uri.value.starts_with("file://"),
          "Installed export lacks authoritative symbol/signature/navigation metadata");
  require(module.exports.back().public_name == "🚀" &&
              module.exports.back().start.character == 0 &&
              module.exports.back().documentation == "Advance by one.",
          "Unicode export lost its UTF-16 position or source documentation");
  require(query_package_catalog(path, "2.1.0", "orbit", 0).packages.empty(),
          "Catalog query ignored its result bound");
  const auto broken_source = query_package_catalog(
      "tests/fixtures/package_index/index.tsv", "2.1.0", "orbit");
  require(broken_source.packages.size() == 1 && !broken_source.packages.front().error.empty() &&
              broken_source.packages.front().modules.empty(),
          "Invalid installed source was silently reported as an empty package API");
  sagan::diagnostics::cancellation_source cancelled;
  cancelled.cancel();
  const auto stopped = query_package_catalog(path, "2.1.0", {}, 100, cancelled.token());
  require(stopped.cancelled && stopped.packages.empty(), "Cancelled catalog query returned packages");
  const auto missing = query_package_catalog("tests/fixtures/package_index/no-index.tsv", "2.1.0");
  require(missing.state == modules::package_index_state::unavailable,
          "Missing catalog index did not report unavailable");
  return 0;
}
