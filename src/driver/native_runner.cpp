#include "native_runner.hpp"
#include "process.hpp"
#include "../modules/resolver.hpp"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#else
#include <windows.h>
#endif

namespace driver
{
  namespace
  {
    auto flag_words(const std::string &flags) -> std::vector<std::string>
    {
      std::vector<std::string> result;
      std::string word;
      char quote = '\0';
      bool started = false;
      for (const char character : flags)
      {
        if ((character == '\'' || character == '"') && (quote == '\0' || quote == character))
        { quote = quote == '\0' ? character : '\0'; started = true; continue; }
        if (quote == '\0' && std::isspace(static_cast<unsigned char>(character)))
        {
          if (started) { result.push_back(std::move(word)); word.clear(); started = false; }
          continue;
        }
        word += character;
        started = true;
      }
      if (quote != '\0') throw std::runtime_error("Unclosed quote in SAGAN_CXXFLAGS");
      if (started) result.push_back(std::move(word));
      return result;
    }

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

    auto toolchain_root() -> std::filesystem::path
    {
      if (const char *override_root = std::getenv("SAGAN_TOOLCHAIN_ROOT");
          override_root != nullptr && *override_root != '\0')
        return std::filesystem::absolute(override_root).lexically_normal();
#ifdef _WIN32
      std::vector<wchar_t> buffer(32768);
      const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
      if (length == 0 || length == buffer.size())
        throw std::runtime_error("Could not locate the Sagan toolchain directory");
      return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path().parent_path();
#else
#ifdef __APPLE__
      std::uint32_t size = 0;
      static_cast<void>(_NSGetExecutablePath(nullptr, &size));
      std::vector<char> buffer(size);
      if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw std::runtime_error("Could not locate the Sagan toolchain directory");
      return std::filesystem::weakly_canonical(buffer.data()).parent_path().parent_path();
#else
      std::error_code error;
      const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
      if (error) throw std::runtime_error("Could not locate the Sagan toolchain directory");
      return executable.parent_path().parent_path();
#endif
#endif
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

  auto compilation_inputs_for(const modules::module_graph &graph) -> native_compilation_inputs
  {
    native_compilation_inputs result;
    for (const auto &module : graph.modules)
    {
      const auto root = module.path.parent_path().parent_path();
      if (!std::filesystem::is_regular_file(root / "sagan.toml")) continue;
      const auto manifest = modules::load_package(root);
      if (manifest.name != "sagan-render") continue;
      result.header = root / "native" / "window_bridge.hpp";
      result.source = root / "native" / "window_bridge.cpp";
      if (graph.package) result.working_directory = graph.package->package_root;
      if (!std::filesystem::is_regular_file(*result.header) ||
          !std::filesystem::is_regular_file(*result.source))
        throw std::runtime_error("Installed sagan-render package is missing native/window_bridge.hpp or "
                                 "native/window_bridge.cpp under '" + root.string() + "'");
#ifdef _WIN32
      result.libraries = {"-lgdi32", "-luser32"};
#endif
      break;
    }
    return result;
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

  auto native_icon_resource() -> std::optional<std::filesystem::path>
  {
#ifdef _WIN32
    const auto root = toolchain_root();
    for (const auto &candidate : {root / "assets" / "application" / "windows" / "sagan-resource.o",
                                  root / "assets" / "sagan-resource.o",
                                  root / "obj" / "launcher" / "sagan-resource.o"})
      if (std::filesystem::is_regular_file(candidate)) return candidate;
#endif
    return {};
  }

  auto application_icon_resource(const std::string &platform) -> std::filesystem::path
  {
    const auto root = toolchain_root();
    std::vector<std::filesystem::path> candidates;
    if (platform == "windows")
    {
      candidates = {root / "assets" / "application" / "windows" / "sagan-resource.o",
                    root / "assets" / "sagan-resource.o",
                    root / "obj" / "launcher" / "sagan-resource.o"};
    }
    else if (platform == "linux")
      candidates = {root / "assets" / "application" / "linux" / "sagan.png"};
    else if (platform == "macos")
      candidates = {root / "assets" / "application" / "macos" / "sagan.icns"};
    else
      throw std::runtime_error("Unsupported application-icon platform '" + platform +
                               "'; expected windows, linux, or macos");
    for (const auto &candidate : candidates)
      if (std::filesystem::is_regular_file(candidate)) return candidate;
    throw std::runtime_error("Sagan application icon for " + platform +
                             " is missing; rebuild or reinstall the Sagan toolchain with application assets");
  }

  auto compile_and_run(const std::string &generated_cpp, const native_compilation_inputs &inputs) -> int
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
#ifdef _WIN32
    const auto icon = native_icon_resource();
    if (!icon) throw std::runtime_error("Sagan icon resource is missing; reinstall or rebuild Sagan");
#endif
    if (inputs.source)
    {
      auto arguments = flag_words(flags);
#ifdef _WIN32
      arguments.insert(arguments.end(), {"-static-libgcc", "-static-libstdc++"});
#endif
      arguments.insert(arguments.end(), {"-include", inputs.header->string(), source.string(),
                                          inputs.source->string()});
#ifdef _WIN32
      arguments.push_back(icon->string());
#endif
      arguments.insert(arguments.end(), {"-o", executable.string()});
      arguments.insert(arguments.end(), inputs.libraries.begin(), inputs.libraries.end());
      const auto report = [](const bool error, const std::string_view output)
      { (error ? std::cerr : std::cout) << output; };
      const auto compiled = run_process(compiler, arguments, build.path(), configuration.environment, {}, report);
      if (compiled.exit_status != 0)
        throw std::runtime_error("Native rendering build failed (exit " +
                                 std::to_string(compiled.exit_status) + ")");
      return run_process(executable, {}, inputs.working_directory.value_or(std::filesystem::current_path()),
                         {}, {}, report).exit_status;
    }
    const bool quote_compiler = compiler.find_first_of(" \\/") != std::string::npos;
    std::string compile_command = (quote_compiler ? shell_quote(compiler) : compiler) + " " + flags + " " +
                                  shell_quote(source.string());
#ifdef _WIN32
    compile_command += " " + shell_quote(icon->string());
#endif
    compile_command += " -o " + shell_quote(executable.string());
#ifdef _WIN32
    if (quote_compiler) compile_command = '"' + compile_command + '"';
#endif
    const int compile_status = exit_code(std::system(compile_command.c_str()));
    if (compile_status != 0)
      throw std::runtime_error("Native C++ compilation failed with exit code " + std::to_string(compile_status));
    return exit_code(std::system(shell_quote(executable.string()).c_str()));
  }
}
