#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace sagan::dap
{
  // Interactive native-GDB-DAP child. Windows uses a kill-on-close job so
  // adapter failure cannot strand the debugger or debuggee process tree.
  class gdb_process
  {
#ifdef _WIN32
    HANDLE process_{INVALID_HANDLE_VALUE};
    HANDLE job_{INVALID_HANDLE_VALUE};
    HANDLE input_{INVALID_HANDLE_VALUE};
    HANDLE output_{INVALID_HANDLE_VALUE};
#else
    pid_t process_{-1};
    int input_{-1};
    int output_{-1};
#endif

  public:
    gdb_process() = default;
    gdb_process(const gdb_process &) = delete;
    auto operator=(const gdb_process &) -> gdb_process & = delete;
    ~gdb_process();

    auto start(const std::filesystem::path &gdb) -> void;
    auto send(std::string_view json) -> void;
    auto receive() -> std::string;
    auto stop() -> void;
    auto running() const -> bool;
  };
}
