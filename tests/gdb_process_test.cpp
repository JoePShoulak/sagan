#include "../src/dap/gdb_process.hpp"
#include "../src/lsp/json.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
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
  const auto *configured = std::getenv("SAGAN_TEST_GDB");
#ifdef _WIN32
  const std::filesystem::path gdb = configured ? configured : "C:/msys64/ucrt64/bin/gdb.exe";
#else
  const std::filesystem::path gdb = configured ? configured : "/usr/bin/gdb";
#endif
  if (!std::filesystem::is_regular_file(gdb)) return 0; // Integration probe is optional without GDB.
  sagan::dap::gdb_process child;
  child.start(gdb);
  require(child.running(), "GDB native DAP did not start");
  child.send(R"({"seq":1,"type":"request","command":"initialize","arguments":{"adapterID":"sagan"}})");
  auto initialize = std::async(std::launch::async, [&]
  {
    for (int i = 0; i < 8; ++i)
    {
      const auto frame = sagan::lsp::json::parse(child.receive());
      if (frame.get("type") && frame.get("type")->string() == "response" &&
          frame.get("command") && frame.get("command")->string() == "initialize")
        return frame;
    }
    throw std::runtime_error("GDB DAP did not answer initialize");
  });
  if (initialize.wait_for(std::chrono::seconds(15)) != std::future_status::ready)
  { child.stop(); throw std::runtime_error("GDB DAP initialization timed out"); }
  const auto answer = initialize.get();
  require(answer.get("success") && answer.get("success")->boolean() == true,
          "GDB native DAP rejected initialize");
  child.stop();
  require(!child.running(), "Stopping GDB DAP did not reap the process");
#ifndef _WIN32
  // A non-DAP child still proves that the POSIX stderr pipe is drained and
  // never mixed into the framed protocol stream.
  sagan::dap::gdb_process stderr_child;
  stderr_child.start("/bin/sh");
  require(!stderr_child.captured_stderr().empty(), "GDB stderr was not captured");
  stderr_child.stop();
#endif
  std::cout << "GDB native DAP transport passed.\n";
  return 0;
}
