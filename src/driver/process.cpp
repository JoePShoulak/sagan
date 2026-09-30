#include "process.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace driver
{
  namespace
  {
    constexpr std::size_t max_output_bytes = 8 * 1024 * 1024;

    auto append_output(process_result &result, const bool error, const std::string_view chunk,
                       const output_observer &observer) -> void
    {
      auto &target = error ? result.standard_error : result.standard_output;
      const auto room = target.size() < max_output_bytes ? max_output_bytes - target.size() : 0;
      const auto count = std::min(room, chunk.size());
      target.append(chunk.substr(0, count));
      if (count != chunk.size()) result.output_truncated = true;
      if (observer && count != 0) observer(error, chunk.substr(0, count));
    }

#ifdef _WIN32
    struct owned_handle
    {
      HANDLE value{INVALID_HANDLE_VALUE};
      owned_handle() = default;
      explicit owned_handle(const HANDLE handle) : value(handle) {}
      owned_handle(const owned_handle &) = delete;
      auto operator=(const owned_handle &) -> owned_handle & = delete;
      ~owned_handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
      auto valid() const -> bool { return value != INVALID_HANDLE_VALUE && value != nullptr; }
      auto reset(const HANDLE handle = INVALID_HANDLE_VALUE) -> void
      {
        if (valid()) CloseHandle(value);
        value = handle;
      }
    };

    auto utf16(const std::string &value) -> std::wstring
    {
      const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
      if (size <= 0 && !value.empty()) throw std::runtime_error("Invalid UTF-8 process argument");
      std::wstring result(static_cast<std::size_t>(size), L'\0');
      if (size > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                        static_cast<int>(value.size()), result.data(), size);
      return result;
    }

    auto quote(const std::wstring &value) -> std::wstring
    {
      std::wstring result = L"\"";
      std::size_t slashes = 0;
      for (const wchar_t character : value)
      {
        if (character == L'\\') { ++slashes; continue; }
        if (character == L'"')
        {
          result.append(slashes * 2 + 1, L'\\');
          result += L'"';
          slashes = 0;
          continue;
        }
        result.append(slashes, L'\\');
        slashes = 0;
        result += character;
      }
      result.append(slashes * 2, L'\\');
      return result + L'"';
    }

    auto environment_block(const std::vector<std::pair<std::string, std::string>> &overrides)
      -> std::vector<wchar_t>
    {
      std::vector<std::wstring> entries;
      for (const auto &[name, value] : overrides) entries.push_back(utf16(name) + L"=" + utf16(value));
      wchar_t *existing = GetEnvironmentStringsW();
      if (!existing) throw std::runtime_error("Could not read process environment");
      for (const wchar_t *cursor = existing; *cursor; cursor += wcslen(cursor) + 1)
      {
        const std::wstring entry(cursor);
        const auto separator = entry.find(L'=', entry.starts_with(L'=') ? 1 : 0);
        const auto name = entry.substr(0, separator);
        const bool replaced = std::any_of(overrides.begin(), overrides.end(), [&](const auto &override_value)
        { return _wcsicmp(name.c_str(), utf16(override_value.first).c_str()) == 0; });
        if (!replaced) entries.push_back(entry);
      }
      FreeEnvironmentStringsW(existing);
      std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right)
      { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
      std::vector<wchar_t> block;
      for (const auto &entry : entries)
      {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
      }
      block.push_back(L'\0');
      return block;
    }

    auto drain_pipe(const HANDLE pipe, const bool error, process_result &result,
                    const output_observer &observer) -> void
    {
      DWORD available = 0;
      while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available != 0)
      {
        std::array<char, 4096> buffer{};
        DWORD read = 0;
        if (!ReadFile(pipe, buffer.data(), std::min<DWORD>(available, buffer.size()), &read, nullptr) ||
            read == 0) break;
        append_output(result, error, {buffer.data(), read}, observer);
      }
    }
#else
    auto drain_pipe(const int pipe, const bool error, process_result &result,
                    const output_observer &observer) -> bool
    {
      std::array<char, 4096> buffer{};
      for (;;)
      {
        const auto read_count = read(pipe, buffer.data(), buffer.size());
        if (read_count > 0)
          append_output(result, error, {buffer.data(), static_cast<std::size_t>(read_count)}, observer);
        else return read_count == 0;
      }
    }
#endif
  }

  auto run_process(const std::filesystem::path &executable,
                   const std::vector<std::string> &arguments,
                   const std::filesystem::path &working_directory,
                   const std::vector<std::pair<std::string, std::string>> &environment,
                   const sagan::diagnostics::cancellation_token cancellation,
                   const output_observer &observer) -> process_result
  {
    process_result result;
    if (cancellation.is_cancelled()) { result.cancelled = true; return result; }
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE out_read_raw = INVALID_HANDLE_VALUE, out_write_raw = INVALID_HANDLE_VALUE;
    HANDLE err_read_raw = INVALID_HANDLE_VALUE, err_write_raw = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&out_read_raw, &out_write_raw, &security, 0))
      throw std::runtime_error("Could not create process output pipe");
    owned_handle out_read(out_read_raw), out_write(out_write_raw);
    if (!CreatePipe(&err_read_raw, &err_write_raw, &security, 0))
      throw std::runtime_error("Could not create process error pipe");
    owned_handle err_read(err_read_raw), err_write(err_write_raw);
    if (!SetHandleInformation(out_read.value, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(err_read.value, HANDLE_FLAG_INHERIT, 0))
      throw std::runtime_error("Could not configure process pipes");
    owned_handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &security,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!input.valid()) throw std::runtime_error("Could not open null process input");
    std::wstring command = quote(executable.wstring());
    for (const auto &argument : arguments) command += L" " + quote(utf16(argument));
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    auto block = environment_block(environment);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value;
    startup.hStdOutput = out_write.value;
    startup.hStdError = err_write.value;
    PROCESS_INFORMATION created{};
    const auto directory = working_directory.wstring();
    if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                        block.data(), directory.empty() ? nullptr : directory.c_str(),
                        &startup, &created))
      throw std::runtime_error("Could not start native process (Windows error " +
                               std::to_string(GetLastError()) + ")");
    owned_handle process(created.hProcess), thread(created.hThread);
    owned_handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.valid() ||
        !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job.value, process.value))
    {
      TerminateProcess(process.value, 1);
      throw std::runtime_error("Could not establish cancellable native process job");
    }
    if (ResumeThread(thread.value) == static_cast<DWORD>(-1))
    {
      TerminateJobObject(job.value, 1);
      throw std::runtime_error("Could not resume native process");
    }
    out_write.reset();
    err_write.reset();
    for (;;)
    {
      drain_pipe(out_read.value, false, result, observer);
      drain_pipe(err_read.value, true, result, observer);
      if (cancellation.is_cancelled() && !result.cancelled)
      {
        result.cancelled = true;
        TerminateJobObject(job.value, 1);
      }
      if (WaitForSingleObject(process.value, 20) == WAIT_OBJECT_0) break;
    }
    drain_pipe(out_read.value, false, result, observer);
    drain_pipe(err_read.value, true, result, observer);
    DWORD exit_status = 1;
    if (!GetExitCodeProcess(process.value, &exit_status))
      throw std::runtime_error("Could not read native process status");
    result.exit_status = static_cast<int>(exit_status);
#else
    int out_pipe[2]{}, err_pipe[2]{};
    if (pipe(out_pipe) != 0) throw std::runtime_error("Could not create process output pipe");
    if (pipe(err_pipe) != 0)
    {
      close(out_pipe[0]); close(out_pipe[1]);
      throw std::runtime_error("Could not create process error pipe");
    }
    const pid_t child = fork();
    if (child < 0)
    {
      close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
      throw std::runtime_error("Could not start native process");
    }
    if (child == 0)
    {
      setpgid(0, 0);
      dup2(out_pipe[1], STDOUT_FILENO);
      dup2(err_pipe[1], STDERR_FILENO);
      close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
      if (!working_directory.empty() && chdir(working_directory.c_str()) != 0) _exit(127);
      for (const auto &[name, value] : environment) setenv(name.c_str(), value.c_str(), 1);
      std::vector<std::string> values{executable.string()};
      values.insert(values.end(), arguments.begin(), arguments.end());
      std::vector<char *> pointers;
      for (auto &value : values) pointers.push_back(value.data());
      pointers.push_back(nullptr);
      execvp(pointers.front(), pointers.data());
      _exit(127);
    }
    // Establish the process group in the parent too: cancellation can arrive
    // before the child reaches its own setpgid call.
    if (setpgid(child, child) != 0 && errno != EACCES && errno != ESRCH)
    {
      kill(child, SIGKILL);
      close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
      waitpid(child, nullptr, 0);
      throw std::runtime_error("Could not establish cancellable process group");
    }
    close(out_pipe[1]); close(err_pipe[1]);
    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);
    bool out_open = true, err_open = true, finished = false;
    int status = 0;
    while (out_open || err_open || !finished)
    {
      if (cancellation.is_cancelled() && !result.cancelled)
      {
        result.cancelled = true;
        kill(-child, SIGKILL);
      }
      pollfd fds[]{{out_pipe[0], static_cast<short>(out_open ? POLLIN : 0), 0},
                   {err_pipe[0], static_cast<short>(err_open ? POLLIN : 0), 0}};
      poll(fds, 2, 20);
      if (out_open && (fds[0].revents & (POLLIN | POLLHUP | POLLERR)))
        out_open = !drain_pipe(out_pipe[0], false, result, observer);
      if (err_open && (fds[1].revents & (POLLIN | POLLHUP | POLLERR)))
        err_open = !drain_pipe(err_pipe[0], true, result, observer);
      if (!finished) finished = waitpid(child, &status, WNOHANG) == child;
    }
    close(out_pipe[0]); close(err_pipe[0]);
    result.exit_status = WIFEXITED(status) ? WEXITSTATUS(status) :
                         WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
#endif
    return result;
  }
}
