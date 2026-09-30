#include "native_runner.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#else
#include <windows.h>
#endif

namespace driver
{
  namespace
  {
    auto shell_quote(const std::string &value) -> std::string
    {
#ifdef _WIN32
      if (value.find('"') != std::string::npos)
        throw std::runtime_error("Native compiler paths cannot contain a quote");
      return '"' + value + '"';
#else
      std::string result = "'";
      for (const char character : value)
      {
        if (character == '\'') result += "'\\''";
        else result += character;
      }
      return result + "'";
#endif
    }

    auto exit_code(const int status) -> int
    {
      if (status == -1) return 1;
#ifdef _WIN32
      return status;
#else
      if (WIFEXITED(status)) return WEXITSTATUS(status);
      if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
      return 1;
#endif
    }

    class temporary_directory
    {
      std::filesystem::path value;

    public:
      temporary_directory()
      {
        const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::vector<std::filesystem::path> roots;
#ifdef _WIN32
        const char *local_value = std::getenv("LOCALAPPDATA");
        const std::string local_app_data = local_value == nullptr ? std::string{} : std::string{local_value};
        if (!local_app_data.empty()) roots.emplace_back(std::filesystem::path(local_app_data) / "Temp");
#endif
        std::error_code ignored;
        const auto system_temporary = std::filesystem::temp_directory_path(ignored);
        if (!ignored) roots.push_back(system_temporary);
        roots.push_back(std::filesystem::current_path());
        for (const auto &root : roots)
        {
          for (int attempt = 0; attempt < 100; ++attempt)
          {
            value = root / ("sagan-run-" + std::to_string(seed) + "-" + std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(value, error)) return;
            if (error && error != std::errc::file_exists) break;
          }
        }
        throw std::runtime_error("Could not create a temporary Sagan build directory");
      }

      temporary_directory(const temporary_directory &) = delete;
      auto operator=(const temporary_directory &) -> temporary_directory & = delete;

      ~temporary_directory()
      {
        std::error_code ignored;
        std::filesystem::remove_all(value, ignored);
      }

      auto path() const -> const std::filesystem::path & { return value; }
    };

    auto environment_value(const char *name) -> std::string
    {
      const char *value = std::getenv(name);
      return value == nullptr ? std::string{} : std::string{value};
    }

    class environment_override
    {
      std::string name;
      std::optional<std::string> previous;

      auto set(const std::string &value) const -> void
      {
#ifdef _WIN32
        if (_putenv_s(name.c_str(), value.c_str()) != 0)
          throw std::runtime_error("Could not configure the native compiler temporary directory");
#else
        if (setenv(name.c_str(), value.c_str(), 1) != 0)
          throw std::runtime_error("Could not configure the native compiler temporary directory");
#endif
      }

    public:
      environment_override(std::string variable, const std::string &value) : name(std::move(variable))
      {
        if (const char *existing = std::getenv(name.c_str())) previous = existing;
        set(value);
      }

      environment_override(const environment_override &) = delete;
      auto operator=(const environment_override &) -> environment_override & = delete;

      ~environment_override()
      {
#ifdef _WIN32
        static_cast<void>(_putenv_s(name.c_str(), previous ? previous->c_str() : ""));
#else
        if (previous) static_cast<void>(setenv(name.c_str(), previous->c_str(), 1));
        else static_cast<void>(unsetenv(name.c_str()));
#endif
      }
    };

#ifdef _WIN32
    auto installed_compiler() -> std::optional<std::filesystem::path>
    {
      std::vector<wchar_t> buffer(32768);
      const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
      if (length == 0 || length == buffer.size()) return {};
      const auto executable = std::filesystem::path(std::wstring(buffer.data(), length));
      const auto candidate = executable.parent_path().parent_path() / "toolchain" / "ucrt64" / "bin" / "g++.exe";
      if (std::filesystem::is_regular_file(candidate)) return candidate;
      return {};
    }
#endif
  }

  auto configured_compiler() -> native_compiler_configuration
  {
    native_compiler_configuration result;
    result.executable = environment_value("CXX");
    if (result.executable.empty())
    {
#ifdef _WIN32
      if (const auto bundled = installed_compiler())
      {
        result.executable = bundled->string();
        result.environment.emplace_back("PATH", bundled->parent_path().string() + ";" +
                                                 environment_value("PATH"));
      }
      else
#endif
        result.executable = "g++";
    }
    result.flags = environment_value("SAGAN_CXXFLAGS").empty()
                       ? "-std=c++23 -Wall -Wextra -Wpedantic -Werror"
                       : environment_value("SAGAN_CXXFLAGS");
    return result;
  }

  auto compile_and_run(const std::string &generated_cpp) -> int
  {
    temporary_directory build;
    const auto source = build.path() / "program.cpp";
#ifdef _WIN32
    const auto executable = build.path() / "program.exe";
#else
    const auto executable = build.path() / "program";
#endif
    std::ofstream output(source, std::ios::binary);
    if (!output) throw std::runtime_error("Could not create temporary generated C++");
    output << generated_cpp;
    output.close();
    if (!output) throw std::runtime_error("Could not write temporary generated C++");

    const std::string temporary_path = build.path().string();
    const environment_override tmpdir("TMPDIR", temporary_path);
    const environment_override tmp("TMP", temporary_path);
    const environment_override temp("TEMP", temporary_path);

    const auto configuration = configured_compiler();
    const std::string &compiler = configuration.executable;
    std::optional<environment_override> bundled_path;
    if (!configuration.environment.empty())
      bundled_path.emplace(configuration.environment.front().first,
                           configuration.environment.front().second);
    const std::string &flags = configuration.flags;
    const bool quote_compiler = compiler.find_first_of(" \\/") != std::string::npos;
    std::string compile_command = (quote_compiler ? shell_quote(compiler) : compiler) + " " + flags + " " +
                                  shell_quote(source.string()) + " -o " + shell_quote(executable.string());
#ifdef _WIN32
    if (quote_compiler) compile_command = '"' + compile_command + '"';
#endif
    const int compile_status = exit_code(std::system(compile_command.c_str()));
    if (compile_status != 0)
      throw std::runtime_error("Native C++ compilation failed with exit code " + std::to_string(compile_status));
    return exit_code(std::system(shell_quote(executable.string()).c_str()));
  }
}
