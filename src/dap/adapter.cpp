#include "adapter.hpp"

#include "breakpoints.hpp"
#include "framing.hpp"
#include "gdb_process.hpp"
#ifdef interface
#undef interface
#endif
#include "../language_service/operations.hpp"
#include "../lsp/json.hpp"
#include "../parser/ast_node.hpp"
#include "../source/provider.hpp"
#include "../syntax/syntax.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <istream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sagan::dap
{
  namespace
  {
    using J = lsp::json::value;

    auto field(const J &object, const std::string_view key) -> const J &
    {
      static const J empty;
      const auto *value = object.get(key);
      return value ? *value : empty;
    }

    auto string_field(const J &object, const std::string_view key) -> std::string
    {
      if (const auto value = field(object, key).string()) return std::string(*value);
      return {};
    }

    auto integer_field(const J &object, const std::string_view key) -> std::int64_t
    {
      return field(object, key).integer().value_or(0);
    }

    auto object_copy(const J &value) -> J::object
    {
      return value.fields() ? *value.fields() : J::object{};
    }

    auto path_from_utf8(const std::string &text) -> std::filesystem::path
    {
      return std::filesystem::path(std::u8string(
          reinterpret_cast<const char8_t *>(text.data()),
          reinterpret_cast<const char8_t *>(text.data() + text.size())));
    }

    auto path_utf8(const std::filesystem::path &path) -> std::string
    {
      const auto text = path.u8string();
      return {reinterpret_cast<const char *>(text.data()), text.size()};
    }

    auto debugger_path() -> std::filesystem::path
    {
      if (const auto *testing = std::getenv("SAGAN_GDB")) return path_from_utf8(testing);
#ifdef _WIN32
      std::wstring module(32768, L'\0');
      const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
      if (length == 0 || length >= module.size()) throw std::runtime_error("Could not locate sagan-dap");
      module.resize(length);
      const auto beside = std::filesystem::path(module).parent_path();
      const auto installed = beside.parent_path() / "toolchain" / "ucrt64" / "bin" / "gdb.exe";
      if (std::filesystem::is_regular_file(installed)) return installed;
      const auto bundled = beside / "toolchain" / "bin" / "gdb.exe";
      if (std::filesystem::is_regular_file(bundled)) return bundled;
      const auto adjacent = beside / "gdb.exe";
      if (std::filesystem::is_regular_file(adjacent)) return adjacent;
#elif defined(__linux__)
      const auto beside = std::filesystem::read_symlink("/proc/self/exe").parent_path();
      const auto bundled = beside / "toolchain" / "bin" / "gdb";
      if (std::filesystem::is_regular_file(bundled)) return bundled;
      if (std::filesystem::is_regular_file(beside / "gdb")) return beside / "gdb";
      if (std::filesystem::is_regular_file("/usr/bin/gdb")) return "/usr/bin/gdb";
#endif
      throw std::runtime_error("Bundled GDB native DAP executable is unavailable");
    }

    auto sagan_function_name(std::string name, const std::filesystem::path &source_path,
                             const source::source_provider &provider) -> std::string
    {
      if (name == "main" || name.starts_with("main(")) return "<top level>";
      if (const auto paren = name.find('('); paren != std::string::npos) name.resize(paren);
      if (!name.starts_with("sagan_") || (name.size() - 6) % 2 != 0)
        return "<Sagan function>";
      std::string result;
      for (std::size_t offset = 6; offset < name.size(); offset += 2)
      {
        const auto hex = [](const char byte) -> int
        {
          if (byte >= '0' && byte <= '9') return byte - '0';
          if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
          return -1;
        };
        const auto high = hex(name[offset]), low = hex(name[offset + 1]);
        if (high < 0 || low < 0) return "<Sagan function>";
        result.push_back(static_cast<char>((high << 4) | low));
      }
      try
      {
        const auto loaded = provider.read_path(source_path);
        if (loaded)
        {
          const auto parsed = syntax::analyze(*loaded.value);
          if (parsed.value && parsed.value->strict_ast)
            for (const auto &statement : parsed.value->strict_ast->statements)
              if (const auto *module = dynamic_cast<const parser::module_declaration *>(statement.get()))
              {
                const auto prefix = module->name + "__";
                if (result.starts_with(prefix)) result.erase(0, prefix.size());
                break;
              }
        }
      }
      catch (const std::exception &) {}
      return result;
    }

    struct requested_breakpoint
    {
      std::int64_t line{};
      std::int64_t column{1};
    };

    struct pending_breakpoints
    {
      std::int64_t client_seq{};
      std::string source_path;
      std::vector<requested_breakpoint> requested;
      std::vector<std::optional<std::size_t>> generated_indices;
      std::vector<breakpoint_result> mappings;
    };

    struct step_state
    {
      std::int64_t thread_id{};
      std::string source_path;
      std::int64_t source_line{};
      std::string function_name;
      std::string command;
      std::size_t stack_depth{};
      std::size_t attempts{};
      J stopped_event;
    };

    struct source_location
    {
      std::string path;
      std::int64_t line{};
      std::string function_name;
      std::size_t stack_depth{};
    };

    class session
    {
      std::istream &input_;
      std::ostream &output_;
      std::ostream &errors_;
      std::filesystem::path gdb_path_;
      std::filesystem::path artifact_root_;
      std::mutex output_mutex_;
      std::mutex state_mutex_;
      std::mutex gdb_mutex_;
      std::unique_ptr<gdb_process> gdb_;
      std::thread gdb_reader_;
      std::thread build_thread_;
      diagnostics::cancellation_source build_cancel_;
      source::disk_source_provider source_;
      std::shared_ptr<language_service::native_operation_result> build_;
      std::map<std::string, std::vector<requested_breakpoint>> requested_by_file_;
      std::map<std::int64_t, pending_breakpoints> pending_breakpoints_;
      std::map<std::int64_t, std::string> forwarded_;
      std::map<std::int64_t, std::int64_t> stack_threads_;
      std::map<std::int64_t, source_location> last_location_;
      std::optional<step_state> stepping_;
      std::int64_t stopped_thread_{};
      std::vector<J> queued_;
      std::atomic_bool shutting_down_{false};
      std::atomic_bool terminated_{false};
      std::atomic<std::int64_t> next_seq_{1000000};
      std::optional<std::int64_t> launch_seq_;
      bool launch_pending_{};

      auto send(const J &message) -> void
      {
        std::scoped_lock lock(output_mutex_);
        write_frame(output_, lsp::json::serialize(message));
      }

      auto response(const std::int64_t request_seq, const std::string &command,
                    const bool success, J body = J::object{}, std::string message = {}) -> void
      {
        J::object result{{"seq", next_seq_++}, {"type", "response"},
                         {"request_seq", request_seq}, {"command", command},
                         {"success", success}};
        if (success) result.emplace("body", std::move(body));
        else result.emplace("message", std::move(message));
        send(result);
      }

      auto event(const std::string &name, J body = J::object{}) -> void
      {
        send(J::object{{"seq", next_seq_++}, {"type", "event"},
                       {"event", name}, {"body", std::move(body)}});
      }

      auto terminate_event() -> void
      {
        if (!terminated_.exchange(true)) event("terminated");
      }

      auto gdb_send(const J &request) -> void
      {
        std::scoped_lock lock(gdb_mutex_);
        if (!gdb_ || !gdb_->running()) throw std::runtime_error("GDB DAP is not running");
        gdb_->send(lsp::json::serialize(request));
      }

      auto forward(const J &request) -> void
      {
        const auto seq = integer_field(request, "seq");
        const auto command = string_field(request, "command");
        {
          std::scoped_lock lock(state_mutex_);
          forwarded_.insert_or_assign(seq, command);
        }
        gdb_send(request);
      }

      auto initialize(const J &request) -> void
      {
        const auto seq = integer_field(request, "seq");
        try
        {
          gdb_ = std::make_unique<gdb_process>();
          gdb_->start(gdb_path_.empty() ? debugger_path() : gdb_path_);
          auto native = object_copy(request);
          native.insert_or_assign("seq", next_seq_++);
          const auto native_seq = integer_field(J(native), "seq");
          {
            std::scoped_lock lock(state_mutex_);
            forwarded_.insert_or_assign(native_seq, "internalInitialize");
          }
          gdb_send(native);
          response(seq, "initialize", true, J::object{
              {"supportsConfigurationDoneRequest", true},
              {"supportsTerminateRequest", true},
              {"supportsCancelRequest", true},
              {"supportsEvaluateForHovers", false},
              {"supportsSetVariable", false},
              {"supportsStepBack", false},
              {"supportsRestartRequest", false}});
          gdb_reader_ = std::thread([this] { read_gdb(); });
        }
        catch (const std::exception &error)
        { response(seq, "initialize", false, {}, error.what()); }
      }

      auto launch(const J &request) -> void;
      auto set_breakpoints(const J &request) -> void;
      auto flush_queued() -> void;
      auto read_gdb() -> void;
      auto handle_gdb_response(const J &message) -> void;
      auto map_stack_trace(const J &message) -> J;
      auto process_step_stack(const J &message) -> void;
      auto map_generated_location(const J &native) -> std::optional<J::object>;
      auto handle(const J &request) -> bool;

    public:
      session(std::istream &input, std::ostream &output, std::ostream &errors,
              std::filesystem::path gdb)
          : input_(input), output_(output), errors_(errors), gdb_path_(std::move(gdb)),
            artifact_root_(std::filesystem::temp_directory_path() / "sagan-dap" /
                           ("session-" + std::to_string(
                                std::chrono::steady_clock::now().time_since_epoch().count()))) {}
      auto run() -> int;
    };

    auto session::launch(const J &request) -> void
    {
      const auto seq = integer_field(request, "seq");
      const auto &arguments = field(request, "arguments");
      const auto raw_path = string_field(arguments, "program").empty()
          ? string_field(arguments, "packageRoot") : string_field(arguments, "program");
      if (raw_path.empty())
      { response(seq, "launch", false, {}, "Supply program or packageRoot"); return; }
      if (build_thread_.joinable())
      { response(seq, "launch", false, {}, "A debug build is already active"); return; }
      const auto path = path_from_utf8(raw_path);
      if (!std::filesystem::exists(path))
      { response(seq, "launch", false, {}, "Sagan source or package does not exist"); return; }
      if (string_field(arguments, "profile") == "optimized")
      { response(seq, "launch", false, {}, "Optimized Sagan debugging is not supported yet"); return; }
      if (!string_field(arguments, "profile").empty() &&
          string_field(arguments, "profile") != "debug")
      { response(seq, "launch", false, {}, "Unknown Sagan debug profile"); return; }
      if (field(arguments, "stopOnEntry").boolean().value_or(false))
      { response(seq, "launch", false, {}, "Sagan source-level stopOnEntry is not supported yet"); return; }
      if (field(arguments, "env").fields())
      { response(seq, "launch", false, {}, "Debug environment overrides are not supported yet"); return; }
      if (const auto *args = field(arguments, "args").elements())
        for (const auto &arg : *args)
          if (!arg.string())
          { response(seq, "launch", false, {}, "Every debug argument must be a string"); return; }
      if (!string_field(arguments, "cwd").empty() &&
          !std::filesystem::is_directory(path_from_utf8(string_field(arguments, "cwd"))))
      { response(seq, "launch", false, {}, "Debug working directory does not exist"); return; }
      {
        std::scoped_lock lock(state_mutex_);
        launch_pending_ = true;
        launch_seq_ = seq;
      }
      build_thread_ = std::thread([this, path, arguments, seq]
      {
        try
        {
          auto result = std::make_shared<language_service::native_operation_result>(
              language_service::build_project(path, source_, artifact_root_,
                                              language_service::native_build_profile::debug,
                                              build_cancel_.token()));
          const auto plan = language_service::plan_debug_launch(*result);
          if (build_cancel_.token().is_cancelled())
            throw std::runtime_error("Debug build cancelled");
          if (!plan || !result->generated_source)
          {
            const auto message = !result->diagnostics.empty()
                ? result->diagnostics.front().message
                : result->standard_error.empty() ? "Sagan debug build failed" : result->standard_error;
            throw std::runtime_error(message);
          }
          {
            std::scoped_lock lock(state_mutex_);
            build_ = result;
            launch_pending_ = false;
          }
          J::object native_args{{"program", path_utf8(plan->executable)},
                                {"cwd", string_field(arguments, "cwd").empty()
                                    ? path_utf8(plan->working_directory)
                                    : string_field(arguments, "cwd")},
                                {"stopOnEntry", field(arguments, "stopOnEntry").boolean().value_or(false)}};
          if (const auto *args = field(arguments, "args").elements())
            native_args.emplace("args", *args);
          forward(J::object{{"seq", seq}, {"type", "request"},
                            {"command", "launch"}, {"arguments", native_args}});
          flush_queued();
        }
        catch (const std::exception &error)
        {
          if (!shutting_down_)
          {
            try { response(seq, "launch", false, {}, error.what()); }
            catch (const std::exception &) { shutting_down_.store(true); }
          }
          std::vector<J> pending;
          {
            std::scoped_lock lock(state_mutex_);
            pending.swap(queued_);
          }
          if (!shutting_down_)
            for (const auto &item : pending)
              try { response(integer_field(item, "seq"), string_field(item, "command"), false, {},
                             "Debug launch did not complete"); }
              catch (const std::exception &) { shutting_down_.store(true); break; }
        }
        {
          std::scoped_lock lock(state_mutex_);
          launch_pending_ = false;
          launch_seq_.reset();
        }
      });
    }

    auto session::set_breakpoints(const J &request) -> void
    {
      const auto seq = integer_field(request, "seq");
      std::shared_ptr<language_service::native_operation_result> build;
      {
        std::scoped_lock lock(state_mutex_);
        build = build_;
      }
      if (!build || !build->debug || !build->generated || !build->generated_source)
      { response(seq, "setBreakpoints", false, {}, "Debug build is not ready"); return; }
      const auto &arguments = field(request, "arguments");
      const auto source_path = string_field(field(arguments, "source"), "path");
      if (source_path.empty())
      { response(seq, "setBreakpoints", false, {}, "Breakpoint source path is missing"); return; }
      std::vector<requested_breakpoint> requested;
      if (const auto *items = field(arguments, "breakpoints").elements())
        for (const auto &item : *items)
        {
          if (field(item, "condition").string() || field(item, "hitCondition").string() ||
              field(item, "logMessage").string())
          {
            response(seq, "setBreakpoints", false, {},
                     "Conditional breakpoints, hit counts, and logpoints are not supported");
            return;
          }
          requested.push_back({integer_field(item, "line"),
                               std::max<std::int64_t>(1, integer_field(item, "column"))});
        }
      const auto canonical = path_utf8(std::filesystem::absolute(path_from_utf8(source_path)).lexically_normal());
      requested_by_file_.insert_or_assign(canonical, requested);
      J::array native_breakpoints;
      pending_breakpoints pending{seq, canonical, requested, {}, {}};
      for (const auto &[file, points] : requested_by_file_)
      {
        const auto loaded = source_.read_path(path_from_utf8(file));
        for (std::size_t index = 0; index < points.size(); ++index)
        {
          breakpoint_result mapped{{}, "Breakpoint source is unavailable"};
          if (loaded && points[index].line > 0 && points[index].column > 0)
            mapped = map_breakpoint(*build->debug, *build->generated, *loaded.value,
                                    {static_cast<std::uint32_t>(points[index].line - 1),
                                     static_cast<std::uint32_t>(points[index].column - 1)});
          if (file == canonical) pending.mappings.push_back(mapped);
          if (mapped.resolved)
          {
            if (file == canonical) pending.generated_indices.push_back(native_breakpoints.size());
            native_breakpoints.emplace_back(J::object{{"line", static_cast<std::int64_t>(mapped.resolved->generated_line)},
                                                      {"column", static_cast<std::int64_t>(mapped.resolved->generated_column)}});
          }
          else if (file == canonical) pending.generated_indices.push_back({});
        }
      }
      const auto native_seq = next_seq_++;
      {
        std::scoped_lock lock(state_mutex_);
        pending_breakpoints_.emplace(native_seq, std::move(pending));
      }
      gdb_send(J::object{{"seq", native_seq}, {"type", "request"},
                         {"command", "setBreakpoints"},
                         {"arguments", J::object{{"source", J::object{{"path", path_utf8(*build->generated_source)}}},
                                                  {"breakpoints", native_breakpoints}}}});
    }

    auto session::flush_queued() -> void
    {
      std::vector<J> queued;
      {
        std::scoped_lock lock(state_mutex_);
        queued.swap(queued_);
      }
      for (const auto &request : queued) handle(request);
    }

    auto session::map_generated_location(const J &native) -> std::optional<J::object>
    {
      std::shared_ptr<language_service::native_operation_result> build;
      {
        std::scoped_lock lock(state_mutex_);
        build = build_;
      }
      if (!build || !build->generated || !build->generated_source) return {};
      const auto path = string_field(field(native, "source"), "path");
      if (path.empty() || path_from_utf8(path).lexically_normal() !=
                              build->generated_source->lexically_normal()) return {};
      const auto offset = codegen::generated_offset(build->generated->text,
          static_cast<std::size_t>(integer_field(native, "line")),
          static_cast<std::size_t>(std::max<std::int64_t>(1, integer_field(native, "column"))));
      if (!offset) return {};
      const auto *source_entry = codegen::source_for_generated_offset(*build->generated, *offset);
      if (!source_entry || !source_entry->source_path) return {};
      const auto loaded = source_.read_path(*source_entry->source_path);
      if (!loaded) return {};
      const auto position = loaded.value->to_utf16(source_entry->source.begin);
      if (!position) return {};
      return J::object{{"source", J::object{{"name", path_utf8(source_entry->source_path->filename())},
                                            {"path", path_utf8(*source_entry->source_path)}}},
                       {"line", static_cast<std::int64_t>(position->line + 1)},
                       {"column", static_cast<std::int64_t>(position->character + 1)}};
    }

    auto session::map_stack_trace(const J &message) -> J
    {
      auto mapped = object_copy(message);
      auto body = object_copy(field(message, "body"));
      const auto *frames = field(field(message, "body"), "stackFrames").elements();
      if (!frames) return mapped;
      J::array visible;
      for (const auto &frame : *frames)
      {
        const auto location = map_generated_location(frame);
        if (!location) continue;
        auto result = object_copy(frame);
        for (const auto &[key, value] : *location) result.insert_or_assign(key, value);
        result.erase("moduleId");
        result.erase("instructionPointerReference");
        result.insert_or_assign("name", sagan_function_name(string_field(frame, "name"),
            path_from_utf8(string_field(field(J(*location), "source"), "path")), source_));
        visible.emplace_back(std::move(result));
      }
      body.insert_or_assign("stackFrames", visible);
      body.insert_or_assign("totalFrames", static_cast<std::int64_t>(visible.size()));
      mapped.insert_or_assign("body", body);
      return mapped;
    }

    auto session::process_step_stack(const J &message) -> void
    {
      const auto mapped = map_stack_trace(message);
      const auto *frames = field(field(mapped, "body"), "stackFrames").elements();
      std::optional<step_state> current;
      {
        std::scoped_lock lock(state_mutex_);
        current = stepping_;
      }
      if (!current) return;
      const auto *frame = frames && !frames->empty() ? &frames->front() : nullptr;
      const auto path = frame ? string_field(field(*frame, "source"), "path") : std::string{};
      const auto line = frame ? integer_field(*frame, "line") : 0;
      const auto function_name = frame ? string_field(*frame, "name") : std::string{};
      const bool changed_location = !path.empty() &&
          (path != current->source_path || line != current->source_line);
      const bool changed_function = !path.empty() && function_name != current->function_name;
      const auto depth = frames ? frames->size() : 0;
      const bool complete = current->command == "stepOut"
          ? changed_function || (depth != 0 && depth < current->stack_depth)
          : current->command == "stepIn"
              ? changed_location || changed_function || depth > current->stack_depth
              : changed_location;
      if (complete || current->attempts >= 100)
      {
        {
          std::scoped_lock lock(state_mutex_);
          stepping_.reset();
        }
        auto output = object_copy(current->stopped_event);
        if (!complete)
        {
          auto body = object_copy(field(current->stopped_event, "body"));
          body.insert_or_assign("reason", "pause");
          body.insert_or_assign("description", "Sagan source-level step could not find a mapped location");
          output.insert_or_assign("body", body);
        }
        output.insert_or_assign("seq", next_seq_++);
        send(output);
        return;
      }
      {
        std::scoped_lock lock(state_mutex_);
        if (stepping_) ++stepping_->attempts;
      }
      const auto native_seq = next_seq_++;
      {
        std::scoped_lock lock(state_mutex_);
        forwarded_.insert_or_assign(native_seq, "internalStepNext");
      }
      gdb_send(J::object{{"seq", native_seq}, {"type", "request"},
                         {"command", current->command},
                         {"arguments", J::object{{"threadId", current->thread_id}}}});
    }

    auto session::handle_gdb_response(const J &message) -> void
    {
      const auto native_seq = integer_field(message, "request_seq");
      std::optional<pending_breakpoints> pending;
      std::string forwarded_command;
      std::int64_t stack_thread{};
      {
        std::scoped_lock lock(state_mutex_);
        if (const auto it = pending_breakpoints_.find(native_seq); it != pending_breakpoints_.end())
        { pending = std::move(it->second); pending_breakpoints_.erase(it); }
        if (const auto it = forwarded_.find(native_seq); it != forwarded_.end())
        { forwarded_command = it->second; forwarded_.erase(it); }
        if (const auto it = stack_threads_.find(native_seq); it != stack_threads_.end())
        { stack_thread = it->second; stack_threads_.erase(it); }
      }
      if (forwarded_command == "internalInitialize") return;
      if (forwarded_command == "internalStepNext")
      {
        if (!field(message, "success").boolean().value_or(false))
        {
          std::optional<step_state> failed;
          {
            std::scoped_lock lock(state_mutex_);
            failed = std::move(stepping_);
            stepping_.reset();
          }
          if (failed)
          {
            auto stopped = object_copy(failed->stopped_event);
            auto body = object_copy(field(failed->stopped_event, "body"));
            body.insert_or_assign("reason", "pause");
            body.insert_or_assign("description", "Native debugger could not complete the Sagan source step");
            stopped.insert_or_assign("seq", next_seq_++);
            stopped.insert_or_assign("body", body);
            send(stopped);
          }
        }
        return;
      }
      if (forwarded_command == "internalStepStack")
      { process_step_stack(message); return; }
      if (pending)
      {
        J::array results;
        const auto *native = field(field(message, "body"), "breakpoints").elements();
        for (std::size_t index = 0; index < pending->requested.size(); ++index)
        {
          const auto &mapped = pending->mappings[index];
          auto answer = J::object{{"verified", false},
                                  {"line", pending->requested[index].line},
                                  {"column", pending->requested[index].column}};
          if (mapped.resolved && pending->generated_indices[index] && native &&
              *pending->generated_indices[index] < native->size())
          {
            const auto &gdb_answer = (*native)[*pending->generated_indices[index]];
            answer.insert_or_assign("verified", field(gdb_answer, "verified").boolean().value_or(false));
            answer.insert_or_assign("line", static_cast<std::int64_t>(mapped.resolved->position.line + 1));
            answer.insert_or_assign("column", static_cast<std::int64_t>(mapped.resolved->position.character + 1));
            if (!field(gdb_answer, "verified").boolean().value_or(false))
              answer.insert_or_assign("message", string_field(gdb_answer, "message"));
          }
          else answer.insert_or_assign("message", mapped.message);
          results.emplace_back(std::move(answer));
        }
        response(pending->client_seq, "setBreakpoints", true,
                 J::object{{"breakpoints", results}});
        return;
      }
      if (forwarded_command.empty()) return;
      if ((forwarded_command == "next" || forwarded_command == "stepIn" ||
           forwarded_command == "stepOut") &&
          !field(message, "success").boolean().value_or(false))
      {
        std::scoped_lock lock(state_mutex_);
        stepping_.reset();
      }
      auto output = forwarded_command == "stackTrace" ? map_stack_trace(message) : message;
      if (forwarded_command == "stackTrace" && stack_thread)
      {
        const auto *frames = field(field(output, "body"), "stackFrames").elements();
        if (frames && !frames->empty())
        {
          const auto &frame = frames->front();
          std::scoped_lock lock(state_mutex_);
          last_location_.insert_or_assign(stack_thread,
              source_location{string_field(field(frame, "source"), "path"),
                              integer_field(frame, "line"), string_field(frame, "name"),
                              frames->size()});
        }
      }
      auto object = object_copy(output);
      if (forwarded_command == "scopes")
      {
        auto body = object_copy(field(output, "body"));
        J::array visible;
        if (const auto *scopes = field(field(output, "body"), "scopes").elements())
          for (const auto &scope : *scopes)
          {
            if (string_field(scope, "name") == "Registers") continue;
            auto item = object_copy(scope);
            item.erase("source");
            item.erase("line");
            item.erase("namedVariables");
            visible.emplace_back(std::move(item));
          }
        body.insert_or_assign("scopes", visible);
        object.insert_or_assign("body", body);
      }
      if (forwarded_command == "variables")
      {
        auto body = object_copy(field(output, "body"));
        J::array visible;
        std::shared_ptr<language_service::native_operation_result> build;
        {
          std::scoped_lock lock(state_mutex_);
          build = build_;
        }
        if (build && build->debug)
          if (const auto *variables = field(field(output, "body"), "variables").elements())
            for (const auto &item : *variables)
              for (const auto &known : build->debug->variables)
                if (string_field(item, "name") == known.generated_name)
                {
                  auto variable = object_copy(item);
                  variable.insert_or_assign("name", known.name);
                  variable.insert_or_assign("type", known.type);
                  if (known.representation == language_service::debug_value_representation::shared_value)
                  {
                    variable.insert_or_assign("value", "<value unavailable>");
                    variable.insert_or_assign("variablesReference", 0);
                    variable.erase("namedVariables");
                  }
                  visible.emplace_back(std::move(variable));
                  break;
                }
        body.insert_or_assign("variables", visible);
        object.insert_or_assign("body", body);
      }
      object.insert_or_assign("seq", next_seq_++);
      send(object);
    }

    auto session::read_gdb() -> void
    {
      try
      {
        while (!shutting_down_)
        {
          const auto message = lsp::json::parse(gdb_->receive());
          const auto type = string_field(message, "type");
          if (type == "response") { handle_gdb_response(message); continue; }
          if (type != "event") continue;
          const auto name = string_field(message, "event");
          if (name == "terminated" || name == "exited")
          {
            if (name == "terminated") terminated_.store(true);
            std::scoped_lock lock(state_mutex_);
            stepping_.reset();
          }
          if (name == "stopped")
          {
            const auto thread_id = integer_field(field(message, "body"), "threadId");
            const auto reason = string_field(field(message, "body"), "reason");
            bool stepping = false;
            {
              std::scoped_lock lock(state_mutex_);
              stopped_thread_ = thread_id;
              if (stepping_ && stepping_->thread_id == thread_id && reason == "step")
              {
                stepping_->stopped_event = message;
                stepping = true;
              }
              else if (reason != "step") stepping_.reset();
            }
            if (stepping)
            {
              const auto native_seq = next_seq_++;
              {
                std::scoped_lock lock(state_mutex_);
                forwarded_.insert_or_assign(native_seq, "internalStepStack");
              }
              gdb_send(J::object{{"seq", native_seq}, {"type", "request"},
                                 {"command", "stackTrace"},
                                 {"arguments", J::object{{"threadId", thread_id}}}});
              continue;
            }
          }
          if (name == "continued")
          {
            std::scoped_lock lock(state_mutex_);
            if (stepping_) continue;
          }
          if (name == "module") continue; // Native DLLs are not Sagan modules.
          if (name == "breakpoint")
          {
            const auto location = map_generated_location(field(field(message, "body"), "breakpoint"));
            if (!location) continue;
            auto body = object_copy(field(message, "body"));
            auto breakpoint = object_copy(field(field(message, "body"), "breakpoint"));
            for (const auto &[key, value] : *location) breakpoint.insert_or_assign(key, value);
            breakpoint.erase("instructionReference");
            body.insert_or_assign("breakpoint", breakpoint);
            auto output = object_copy(message);
            output.insert_or_assign("seq", next_seq_++);
            output.insert_or_assign("body", body);
            send(output);
            continue;
          }
          if (name == "output")
          {
            const auto printed = string_field(field(message, "body"), "output");
            std::shared_ptr<language_service::native_operation_result> build;
            {
              std::scoped_lock lock(state_mutex_);
              build = build_;
            }
            if (build && build->generated_source &&
                printed.find(path_utf8(*build->generated_source)) != std::string::npos)
              continue;
          }
          auto output = object_copy(message);
          if (name == "process")
          {
            auto body = object_copy(field(message, "body"));
            std::shared_ptr<language_service::native_operation_result> build;
            {
              std::scoped_lock lock(state_mutex_);
              build = build_;
            }
            if (build && build->document.canonical_path)
              body.insert_or_assign("name", path_utf8(build->document.canonical_path->filename()));
            output.insert_or_assign("body", body);
          }
          output.insert_or_assign("seq", next_seq_++);
          send(output);
        }
      }
      catch (const std::exception &error)
      {
        if (!shutting_down_)
        {
          event("output", J::object{{"category", "stderr"}, {"output", std::string(error.what()) + "\n"}});
          terminate_event();
        }
      }
    }

    auto session::handle(const J &request) -> bool
    {
      if (string_field(request, "type") != "request") return true;
      const auto command = string_field(request, "command");
      const auto seq = integer_field(request, "seq");
      if (command == "initialize") { initialize(request); return true; }
      if (command == "launch") { launch(request); return true; }
      if (command == "cancel")
      {
        bool matching = false;
        {
          std::scoped_lock lock(state_mutex_);
          matching = launch_pending_ && launch_seq_ &&
                     *launch_seq_ == integer_field(field(request, "arguments"), "requestId");
        }
        if (matching) build_cancel_.cancel();
        response(seq, command, matching, {}, matching ? "" : "No matching debug build is active");
        return true;
      }
      if (command == "disconnect" || command == "terminate")
      {
        build_cancel_.cancel();
        shutting_down_.store(true);
        if (gdb_) gdb_->stop();
        response(seq, command, true);
        terminate_event();
        return false;
      }
      {
        std::scoped_lock lock(state_mutex_);
        if (launch_pending_ && (command == "setBreakpoints" || command == "configurationDone"))
        { queued_.push_back(request); return true; }
      }
      try
      {
        if (command == "setBreakpoints") set_breakpoints(request);
        else if (command == "attach") response(seq, command, false, {}, "Attach is not supported");
        else if (command == "evaluate") response(seq, command, false, {}, "Sagan expression evaluation is not supported");
        else if (command == "configurationDone" || command == "threads" ||
                 command == "stackTrace" || command == "scopes" || command == "variables" ||
                 command == "continue" || command == "pause" || command == "next" ||
                 command == "stepIn" || command == "stepOut")
        {
          if (command == "stackTrace")
          {
            std::scoped_lock lock(state_mutex_);
            stack_threads_.insert_or_assign(seq, integer_field(field(request, "arguments"), "threadId"));
          }
          if (command == "pause" || command == "continue")
          {
            std::scoped_lock lock(state_mutex_);
            stepping_.reset();
          }
          if (command == "next" || command == "stepIn" || command == "stepOut")
          {
            const auto thread_id = integer_field(field(request, "arguments"), "threadId");
            std::scoped_lock lock(state_mutex_);
            if (!last_location_.contains(thread_id))
            { response(seq, command, false, {}, "Request a Sagan stack trace before stepping"); return true; }
            const auto &[path, line, name, depth] = last_location_.at(thread_id);
            stepping_ = step_state{thread_id, path, line, name, command, depth, 0, {}};
          }
          forward(request);
        }
        else response(seq, command, false, {}, "Unsupported Sagan DAP request");
      }
      catch (const std::exception &error)
      { response(seq, command, false, {}, error.what()); }
      return true;
    }

    auto session::run() -> int
    {
      try
      {
        while (!shutting_down_)
        {
          const auto frame = read_frame(input_);
          if (!frame) break;
          try { if (!handle(lsp::json::parse(*frame))) break; }
          catch (const std::exception &error)
          { errors_ << "Invalid DAP request: " << error.what() << '\n'; }
        }
      }
      catch (const std::exception &error)
      { errors_ << "DAP transport error: " << error.what() << '\n'; }
      shutting_down_.store(true);
      build_cancel_.cancel();
      if (gdb_) gdb_->stop();
      if (build_thread_.joinable()) build_thread_.join();
      if (gdb_reader_.joinable()) gdb_reader_.join();
      const auto base = (std::filesystem::temp_directory_path() / "sagan-dap").lexically_normal();
      if (artifact_root_.parent_path().lexically_normal() == base &&
          artifact_root_.filename().string().starts_with("session-"))
      {
        std::error_code error;
        std::filesystem::remove_all(artifact_root_, error);
        if (error) errors_ << "Could not remove Sagan debug artifacts: " << error.message() << '\n';
      }
      return 0;
    }
  }

  auto run_adapter(std::istream &input, std::ostream &output, std::ostream &errors,
                   std::filesystem::path debugger) -> int
  {
    return session(input, output, errors, std::move(debugger)).run();
  }
}
