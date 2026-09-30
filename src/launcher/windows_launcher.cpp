#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  enum class launch_mode { console, windowed };

  auto trim(std::string value) -> std::string
  {
    const auto whitespace = [](const unsigned char character) { return std::isspace(character) != 0; };
    while (!value.empty() && whitespace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && whitespace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
  }

  auto manifest_for(const std::filesystem::path &target) -> std::optional<std::filesystem::path>
  {
    std::filesystem::path current;
    if (std::filesystem::is_directory(target)) current = target;
    else if (target.filename() == "sagan.toml") return target;
    else current = target.parent_path();
    current = std::filesystem::absolute(current).lexically_normal();
    while (!current.empty())
    {
      const auto candidate = current / "sagan.toml";
      if (std::filesystem::is_regular_file(candidate)) return candidate;
      const auto parent = current.parent_path();
      if (parent == current) break;
      current = parent;
    }
    return {};
  }

  auto configured_mode(const std::filesystem::path &target) -> launch_mode
  {
    const auto manifest = manifest_for(target);
    if (!manifest) return launch_mode::console;
    std::ifstream input(*manifest);
    if (!input) return launch_mode::console;
    std::string section;
    std::string line;
    while (std::getline(input, line))
    {
      if (const auto comment = line.find('#'); comment != std::string::npos) line.erase(comment);
      line = trim(std::move(line));
      if (line.empty()) continue;
      if (line.front() == '[' && line.back() == ']')
      {
        section = trim(line.substr(1, line.size() - 2));
        continue;
      }
      if (section != "application") continue;
      const auto equals = line.find('=');
      if (equals == std::string::npos || trim(line.substr(0, equals)) != "mode") continue;
      const std::string value = trim(line.substr(equals + 1));
      if (value == "\"windowed\"") return launch_mode::windowed;
      if (value == "\"console\"") return launch_mode::console;
    }
    return launch_mode::console;
  }

  auto quote(const std::wstring &value) -> std::wstring
  {
    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : value)
    {
      if (character == L'\\')
      {
        ++backslashes;
        continue;
      }
      if (character == L'\"')
      {
        result.append(backslashes * 2 + 1, L'\\');
        result.push_back(L'\"');
        backslashes = 0;
        continue;
      }
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(character);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
  }

  auto executable_directory() -> std::filesystem::path
  {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) throw std::runtime_error("Could not locate the Sagan launcher");
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
  }

  auto diagnostic_log() -> std::filesystem::path
  {
    const wchar_t *local = _wgetenv(L"LOCALAPPDATA");
    const auto root = local == nullptr ? std::filesystem::temp_directory_path() : std::filesystem::path(local);
    const auto logs = root / "Sagan" / "logs";
    std::filesystem::create_directories(logs);
    return logs / "latest-launch.log";
  }

  auto show_error(const std::wstring &message) -> int
  {
    MessageBoxW(nullptr, message.c_str(), L"Sagan Launcher", MB_OK | MB_ICONERROR);
    return 1;
  }

  auto start(const std::filesystem::path &target, const launch_mode mode) -> int
  {
    const auto sagan = executable_directory() / "sagan.exe";
    if (!std::filesystem::is_regular_file(sagan))
      return show_error(L"The Sagan compiler could not be found beside sagan-launch.exe.");

    const std::wstring option = mode == launch_mode::console ? L"--launch-console" : L"--launch-windowed";
    std::wstring command = quote(sagan.wstring()) + L" " + option + L" " + quote(target.wstring());
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    HANDLE log = INVALID_HANDLE_VALUE;
    HANDLE null_input = INVALID_HANDLE_VALUE;
    std::filesystem::path log_path;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    if (mode == launch_mode::console) flags |= CREATE_NEW_CONSOLE;
    else
    {
      log_path = diagnostic_log();
      SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
      log = CreateFileW(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inheritable, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
      if (log == INVALID_HANDLE_VALUE) return show_error(L"Sagan could not create its launch diagnostic log.");
      null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (null_input == INVALID_HANDLE_VALUE)
      {
        CloseHandle(log);
        return show_error(L"Sagan could not prepare windowed input handling.");
      }
      startup.dwFlags |= STARTF_USESTDHANDLES;
      startup.hStdInput = null_input;
      startup.hStdOutput = log;
      startup.hStdError = log;
      flags |= CREATE_NO_WINDOW;
    }

    const BOOL created = CreateProcessW(sagan.c_str(), mutable_command.data(), nullptr, nullptr,
                                        mode == launch_mode::windowed, flags, nullptr,
                                        target.parent_path().c_str(), &startup, &process);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (null_input != INVALID_HANDLE_VALUE) CloseHandle(null_input);
    if (!created) return show_error(L"Windows could not start the Sagan compiler.");

    if (mode == launch_mode::console)
    {
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
      return 0;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 1;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (status != 0)
      return show_error(L"The Sagan program failed to start or exited with an error.\n\nDiagnostic log:\n" +
                        log_path.wstring());
    return 0;
  }
}

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int
{
  int argc = 0;
  wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) return show_error(L"Windows could not read the Sagan launch command.");
  launch_mode mode = launch_mode::console;
  std::filesystem::path target;
  if (argc == 2)
  {
    target = argv[1];
    mode = configured_mode(target);
  }
  else if (argc == 3 && (std::wstring(argv[1]) == L"--console" || std::wstring(argv[1]) == L"--windowed"))
  {
    mode = std::wstring(argv[1]) == L"--windowed" ? launch_mode::windowed : launch_mode::console;
    target = argv[2];
  }
  else
  {
    LocalFree(argv);
    return show_error(L"Usage: sagan-launch.exe [--console | --windowed] FILE");
  }
  LocalFree(argv);
  if (!std::filesystem::is_regular_file(target)) return show_error(L"The selected Sagan source file does not exist.");
  try
  {
    return start(std::filesystem::absolute(target).lexically_normal(), mode);
  }
  catch (const std::exception &)
  {
    return show_error(L"Sagan could not prepare the selected program for launch.");
  }
}

#endif
