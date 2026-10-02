#include "gdb_process.hpp"
#include "framing.hpp"

#include <sstream>
#include <stdexcept>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sagan::dap
{
#ifdef _WIN32
  namespace
  {
    auto close_handle(HANDLE &handle) -> void
    {
      if (handle != INVALID_HANDLE_VALUE && handle != nullptr) CloseHandle(handle);
      handle = INVALID_HANDLE_VALUE;
    }

    auto read_byte(const HANDLE pipe, char &byte) -> bool
    {
      DWORD count = 0;
      return ReadFile(pipe, &byte, 1, &count, nullptr) && count == 1;
    }
  }
#else
  namespace
  {
    auto close_fd(int &fd) -> void
    {
      if (fd >= 0) ::close(fd);
      fd = -1;
    }

    auto read_byte(const int fd, char &byte) -> bool
    {
      return ::read(fd, &byte, 1) == 1;
    }
  }
#endif

  gdb_process::~gdb_process() { stop(); }

  auto gdb_process::start(const std::filesystem::path &gdb) -> void
  {
#ifdef _WIN32
    if (running()) throw std::runtime_error("GDB DAP process is already running");
    if (!std::filesystem::is_regular_file(gdb)) throw std::runtime_error("GDB executable is missing");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE child_input = INVALID_HANDLE_VALUE, parent_input = INVALID_HANDLE_VALUE;
    HANDLE parent_output = INVALID_HANDLE_VALUE, child_output = INVALID_HANDLE_VALUE;
    const auto cleanup = [&]
    {
      close_handle(child_input); close_handle(parent_input);
      close_handle(parent_output); close_handle(child_output);
    };
    if (!CreatePipe(&child_input, &parent_input, &security, 0) ||
        !CreatePipe(&parent_output, &child_output, &security, 0) ||
        !SetHandleInformation(parent_input, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(parent_output, HANDLE_FLAG_INHERIT, 0))
    { cleanup(); throw std::runtime_error("Could not create GDB DAP pipes"); }
    std::wstring command = L"\"" + gdb.wstring() + L"\" --interpreter=dap --quiet";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION created{};
    const auto directory = gdb.parent_path().wstring();
    if (!CreateProcessW(gdb.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                        directory.c_str(), &startup, &created))
    { cleanup(); throw std::runtime_error("Could not start GDB native DAP"); }
    close_handle(child_input); close_handle(child_output);
    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job_ == INVALID_HANDLE_VALUE || job_ == nullptr ||
        !SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job_, created.hProcess))
    {
      TerminateProcess(created.hProcess, 1);
      CloseHandle(created.hThread);
      CloseHandle(created.hProcess);
      cleanup(); close_handle(job_);
      throw std::runtime_error("Could not isolate GDB DAP process tree");
    }
    if (ResumeThread(created.hThread) == static_cast<DWORD>(-1))
    {
      TerminateJobObject(job_, 1);
      CloseHandle(created.hThread);
      CloseHandle(created.hProcess);
      cleanup(); close_handle(job_);
      throw std::runtime_error("Could not resume GDB native DAP");
    }
    CloseHandle(created.hThread);
    process_ = created.hProcess;
    input_ = parent_input;
    output_ = parent_output;
#else
    if (running()) throw std::runtime_error("GDB DAP process is already running");
    if (!std::filesystem::is_regular_file(gdb)) throw std::runtime_error("GDB executable is missing");
    int child_input[2]{-1, -1}, child_output[2]{-1, -1};
    if (::pipe(child_input) != 0 || ::pipe(child_output) != 0)
    {
      for (auto &fd : child_input) close_fd(fd);
      for (auto &fd : child_output) close_fd(fd);
      throw std::runtime_error("Could not create GDB DAP pipes");
    }
    const auto child = ::fork();
    if (child < 0)
    {
      for (auto &fd : child_input) close_fd(fd);
      for (auto &fd : child_output) close_fd(fd);
      throw std::runtime_error("Could not fork GDB DAP");
    }
    if (child == 0)
    {
      ::setpgid(0, 0);
      ::dup2(child_input[0], STDIN_FILENO);
      ::dup2(child_output[1], STDOUT_FILENO);
      for (auto &fd : child_input) close_fd(fd);
      for (auto &fd : child_output) close_fd(fd);
      ::execl(gdb.c_str(), gdb.c_str(), "--interpreter=dap", "--quiet", nullptr);
      ::_exit(127);
    }
    ::setpgid(child, child);
    close_fd(child_input[0]);
    close_fd(child_output[1]);
    process_ = child;
    input_ = child_input[1];
    output_ = child_output[0];
    std::signal(SIGPIPE, SIG_IGN);
#endif
  }

  auto gdb_process::send(const std::string_view json) -> void
  {
#ifdef _WIN32
    if (!running()) throw std::runtime_error("GDB DAP process is not running");
    std::ostringstream framed;
    write_frame(framed, json);
    const auto bytes = framed.str();
    std::size_t written = 0;
    while (written < bytes.size())
    {
      DWORD count = 0;
      if (!WriteFile(input_, bytes.data() + written,
                     static_cast<DWORD>(bytes.size() - written), &count, nullptr) || count == 0)
        throw std::runtime_error("Could not send GDB DAP request");
      written += count;
    }
#else
    if (!running()) throw std::runtime_error("GDB DAP process is not running");
    std::ostringstream framed;
    write_frame(framed, json);
    const auto bytes = framed.str();
    std::size_t written = 0;
    while (written < bytes.size())
    {
      const auto count = ::write(input_, bytes.data() + written, bytes.size() - written);
      if (count <= 0)
      {
        if (errno == EINTR) continue;
        throw std::runtime_error("Could not send GDB DAP request");
      }
      written += static_cast<std::size_t>(count);
    }
#endif
  }

  auto gdb_process::receive() -> std::string
  {
#ifdef _WIN32
    std::string header;
    char byte{};
    while (header.size() < 8192 && read_byte(output_, byte))
    {
      header.push_back(byte);
      if (header.ends_with("\r\n\r\n")) break;
    }
    if (!header.ends_with("\r\n\r\n")) throw std::runtime_error("GDB DAP header ended unexpectedly");
    const auto marker = header.find("Content-Length:");
    if (marker == std::string::npos) throw std::runtime_error("GDB DAP response has no length");
    const auto end = header.find("\r\n", marker);
    const auto length = std::stoull(header.substr(marker + sizeof("Content-Length:") - 1,
                                                  end - marker - sizeof("Content-Length:") + 1));
    if (length > max_frame_bytes) throw std::runtime_error("GDB DAP response is oversized");
    std::string body(length, '\0');
    std::size_t received = 0;
    while (received < length)
    {
      DWORD count = 0;
      if (!ReadFile(output_, body.data() + received,
                    static_cast<DWORD>(length - received), &count, nullptr) || count == 0)
        throw std::runtime_error("GDB DAP response is truncated");
      received += count;
    }
    std::istringstream framed(header + body);
    const auto parsed = read_frame(framed);
    if (!parsed) throw std::runtime_error("GDB DAP response is empty");
    return *parsed;
#else
    std::string header;
    char byte{};
    while (header.size() < 8192 && read_byte(output_, byte))
    {
      header.push_back(byte);
      if (header.ends_with("\r\n\r\n")) break;
    }
    if (!header.ends_with("\r\n\r\n")) throw std::runtime_error("GDB DAP header ended unexpectedly");
    const auto marker = header.find("Content-Length:");
    if (marker == std::string::npos) throw std::runtime_error("GDB DAP response has no length");
    const auto end = header.find("\r\n", marker);
    const auto length = std::stoull(header.substr(marker + sizeof("Content-Length:") - 1,
                                                  end - marker - sizeof("Content-Length:") + 1));
    if (length > max_frame_bytes) throw std::runtime_error("GDB DAP response is oversized");
    std::string body(length, '\0');
    std::size_t received = 0;
    while (received < length)
    {
      const auto count = ::read(output_, body.data() + received, length - received);
      if (count <= 0)
      {
        if (count < 0 && errno == EINTR) continue;
        throw std::runtime_error("GDB DAP response is truncated");
      }
      received += static_cast<std::size_t>(count);
    }
    std::istringstream framed(header + body);
    const auto parsed = read_frame(framed);
    if (!parsed) throw std::runtime_error("GDB DAP response is empty");
    return *parsed;
#endif
  }

  auto gdb_process::stop() -> void
  {
#ifdef _WIN32
    close_handle(input_);
    if (job_ != INVALID_HANDLE_VALUE && job_ != nullptr) TerminateJobObject(job_, 1);
    if (process_ != INVALID_HANDLE_VALUE && process_ != nullptr)
      WaitForSingleObject(process_, 2000);
    close_handle(output_);
    close_handle(process_);
    close_handle(job_);
#endif
#ifndef _WIN32
    close_fd(input_);
    if (process_ > 0)
    {
      ::kill(-process_, SIGKILL);
      ::kill(process_, SIGKILL);
      while (::waitpid(process_, nullptr, 0) < 0 && errno == EINTR) {}
      process_ = -1;
    }
    close_fd(output_);
#endif
  }

  auto gdb_process::running() const -> bool
  {
#ifdef _WIN32
    return process_ != INVALID_HANDLE_VALUE && process_ != nullptr &&
           WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
#else
    return process_ > 0 && ::kill(process_, 0) == 0;
#endif
  }
}
