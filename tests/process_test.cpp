#include "../src/driver/process.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace
{
  auto require(const bool condition, const char *message) -> void
  {
    if (!condition) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  const auto version = driver::run_process("g++", {"--version"}, std::filesystem::current_path(), {});
  require(!version.cancelled && version.exit_status == 0 &&
              version.standard_output.find("g++") != std::string::npos,
          "captured native process did not return GCC output");
  sagan::diagnostics::cancellation_source prior;
  prior.cancel();
  const auto already_cancelled = driver::run_process("g++", {"--version"}, {}, {}, prior.token());
  require(already_cancelled.cancelled && already_cancelled.standard_output.empty(),
          "pre-cancelled native process launched anyway");
  sagan::diagnostics::cancellation_source running;
  std::thread stop([&]
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    running.cancel();
  });
  const auto interrupted = driver::run_process("sleep", {"5"}, {}, {}, running.token());
  stop.join();
  require(interrupted.cancelled, "running native process ignored cancellation");
  return 0;
}
