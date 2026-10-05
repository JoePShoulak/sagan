#include "diagnostic.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace sagan::diagnostics
{
  namespace
  {
    auto terminal_color(const std::ostream &output) -> bool
    {
      if (&output != &std::cerr && &output != &std::cout) return false;
      if (std::getenv("NO_COLOR") != nullptr) return false;
#ifdef _WIN32
      if (_isatty(_fileno(stderr)) == 0) return false;
      const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
      DWORD mode = 0;
      return GetConsoleMode(handle, &mode) != 0 &&
             ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0 ||
              SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0);
#else
      const char *term = std::getenv("TERM");
      return term != nullptr && std::string_view(term) != "dumb" && isatty(STDERR_FILENO) != 0;
#endif
    }

    auto display_path(const source::document_snapshot &document) -> std::string
    {
      if (!document.identity().canonical_path) return document.identity().uri.value;
      const auto &path = *document.identity().canonical_path;
      std::error_code error;
      const auto relative = std::filesystem::relative(path, std::filesystem::current_path(), error);
      if (!error && !relative.empty() && *relative.begin() != "..") return relative.generic_string();
      return path.generic_string();
    }

    auto source_excerpt(std::ostream &output, const source::document_snapshot &document,
                        const source::source_range &range, const std::string_view label) -> void
    {
      if (range.document != document.identity().id || range.bytes.begin > document.text().size()) return;
      const auto position = document.to_utf16(range.bytes.begin);
      if (!position) return;
      const std::string_view text = document.text();
      std::size_t begin = range.bytes.begin;
      while (begin > 0 && text[begin - 1] != '\n' && text[begin - 1] != '\r') --begin;
      std::size_t end = range.bytes.begin;
      while (end < text.size() && text[end] != '\n' && text[end] != '\r') ++end;
      const auto line = std::to_string(position->line + 1);
      output << " " << line << " | " << text.substr(begin, end - begin) << '\n';
      output << " " << std::string(line.size(), ' ') << " | ";
      const auto prefix = document.to_utf16(static_cast<source::byte_offset>(begin));
      const auto indent = prefix ? position->character - prefix->character : 0;
      output << std::string(indent, ' ');
      const auto highlight_end = std::min<std::size_t>(range.bytes.end, end);
      const auto end_position = document.to_utf16(static_cast<source::byte_offset>(highlight_end));
      const auto width = end_position && end_position->line == position->line &&
                         end_position->character > position->character
                             ? end_position->character - position->character : 1;
      output << std::string(width, '^');
      if (!label.empty()) output << ' ' << label;
      output << '\n';
    }

    auto json_string(const std::string_view input) -> std::string
    {
      static constexpr char hex[] = "0123456789abcdef";
      std::string output{"\""};
      for (const unsigned char value : input)
      {
        switch (value)
        {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
          if (value < 0x20)
          {
            output += "\\u00";
            output.push_back(hex[value >> 4]);
            output.push_back(hex[value & 0x0f]);
          }
          else output.push_back(static_cast<char>(value));
        }
      }
      output.push_back('"');
      return output;
    }

    auto position_json(const source::utf16_position position) -> std::string
    {
      return "{\"line\":" + std::to_string(position.line) +
             ",\"character\":" + std::to_string(position.character) + "}";
    }

    auto range_json(const source::document_snapshot &document, const source::source_range &range) -> std::string
    {
      const auto start = document.to_utf16(range.bytes.begin).value_or(source::utf16_position{});
      const auto end = document.to_utf16(range.bytes.end).value_or(start);
      return "{\"bytes\":{\"begin\":" + std::to_string(range.bytes.begin) +
             ",\"end\":" + std::to_string(range.bytes.end) + "},\"utf16\":{\"start\":" +
             position_json(start) + ",\"end\":" + position_json(end) + "}}";
    }
  }

  cancellation_token::cancellation_token(std::shared_ptr<const std::atomic_bool> state) : state_(std::move(state)) {}
  auto cancellation_token::is_cancelled() const -> bool { return state_ && state_->load(); }
  auto cancellation_source::token() const -> cancellation_token { return cancellation_token{state_}; }
  auto cancellation_source::cancel() const -> void { state_->store(true); }

  auto phase_name(const phase value) -> std::string_view
  {
    switch (value)
    {
    case phase::lexical: return "lexical";
    case phase::syntax: return "syntax";
    case phase::semantic: return "semantic";
    case phase::type: return "type";
    case phase::module: return "module";
    case phase::project: return "project";
    case phase::build: return "build";
    case phase::entry_point: return "entry-point";
    case phase::runtime: return "runtime";
    }
    return "unknown";
  }

  auto default_code(const phase value) -> std::string_view
  {
    switch (value)
    {
    case phase::lexical: return "SAG-LEX-0001";
    case phase::syntax: return "SAG-SYN-0001";
    case phase::semantic: return "SAG-SEM-0001";
    case phase::type: return "SAG-TYP-0001";
    case phase::module: return "SAG-MOD-0001";
    case phase::project: return "SAG-PRJ-0001";
    case phase::build: return "SAG-BLD-0001";
    case phase::entry_point: return "SAG-ENT-0001";
    case phase::runtime: return "SAG-RUN-0001";
    }
    return "SAG-UNK-0001";
  }

  auto severity_name(const severity value) -> std::string_view
  {
    switch (value)
    {
    case severity::error: return "error";
    case severity::warning: return "warning";
    case severity::information: return "information";
    case severity::hint: return "hint";
    }
    return "unknown";
  }

  auto state_name(const result_state value) -> std::string_view
  {
    switch (value)
    {
    case result_state::complete: return "complete";
    case result_state::recovered: return "recovered";
    case result_state::incomplete: return "incomplete";
    case result_state::cancelled: return "cancelled";
    case result_state::stale: return "stale";
    }
    return "unknown";
  }

  auto render_terminal(std::ostream &output, const source::document_snapshot &document,
                       const diagnostic &value) -> void
  {
    const auto position = document.to_utf16(value.primary.bytes.begin).value_or(source::utf16_position{});
    const bool color = terminal_color(output);
    if (color) output << (value.level == severity::error ? "\x1b[31m" : "\x1b[33m");
    output << severity_name(value.level) << '[' << value.code << ']';
    if (color) output << "\x1b[0m";
    output << ": " << value.message << '\n';
    const bool located = !(value.primary.bytes == source::byte_range{} &&
                           (value.owner == phase::build || value.owner == phase::project ||
                            value.owner == phase::runtime));
    if (located)
    {
      output << " --> " << display_path(document) << ':' << position.line + 1
             << ':' << position.character + 1 << '\n';
      source_excerpt(output, document, value.primary, {});
    }
    for (const auto &related : value.related)
    {
      if (related.range.document == document.identity().id)
      {
        const auto where = document.to_utf16(related.range.bytes.begin);
        if (where) output << " note: " << related.message << " at " << display_path(document)
                          << ':' << where->line + 1 << ':' << where->character + 1 << '\n';
        source_excerpt(output, document, related.range, related.message);
      }
      else output << " note: " << related.message << '\n';
    }
    for (const auto &note : value.notes) output << "note: " << note << '\n';
    for (const auto &fix : value.fixes) output << "help: " << fix.title << '\n';
  }

  auto render_json(const source::document_snapshot &document, const result_state state,
                   const std::vector<diagnostic> &values) -> std::string
  {
    std::ostringstream output;
    output << "{\"schema\":" << json_string(schema_version)
           << ",\"document\":{\"uri\":" << json_string(document.identity().uri.value)
           << ",\"canonicalPath\":";
    if (document.identity().canonical_path)
      output << json_string(document.identity().canonical_path->generic_string());
    else output << "null";
    output << ",\"version\":" << document.version() << "},\"state\":" << json_string(state_name(state))
           << ",\"diagnostics\":[";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
      if (index != 0) output << ',';
      const auto &value = values[index];
      output << "{\"code\":" << json_string(value.code)
             << ",\"severity\":" << json_string(severity_name(value.level))
             << ",\"phase\":" << json_string(phase_name(value.owner))
             << ",\"range\":" << range_json(document, value.primary)
             << ",\"message\":" << json_string(value.message) << ",\"related\":[";
      for (std::size_t related_index = 0; related_index < value.related.size(); ++related_index)
      {
        if (related_index != 0) output << ',';
        const auto &related = value.related[related_index];
        output << "{\"range\":" << range_json(document, related.range)
               << ",\"message\":" << json_string(related.message) << '}';
      }
      output << "],\"notes\":[";
      for (std::size_t note_index = 0; note_index < value.notes.size(); ++note_index)
      {
        if (note_index != 0) output << ',';
        output << json_string(value.notes[note_index]);
      }
      output << "],\"fixes\":[";
      for (std::size_t fix_index = 0; fix_index < value.fixes.size(); ++fix_index)
      {
        if (fix_index != 0) output << ',';
        output << "{\"title\":" << json_string(value.fixes[fix_index].title) << ",\"edits\":[";
        const auto &edits = value.fixes[fix_index].edits;
        for (std::size_t edit_index = 0; edit_index < edits.size(); ++edit_index)
        {
          if (edit_index != 0) output << ',';
          output << "{\"range\":" << range_json(document, edits[edit_index].range)
                 << ",\"replacement\":" << json_string(edits[edit_index].replacement_utf8) << '}';
        }
        output << "]}";
      }
      output << "]}";
    }
    output << "]}\n";
    return output.str();
  }
}

