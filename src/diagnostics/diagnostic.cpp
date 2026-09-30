#include "diagnostic.hpp"

#include <algorithm>
#include <sstream>

namespace sagan::diagnostics
{
  namespace
  {
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
    output << phase_name(value.owner) << ' ' << severity_name(value.level) << " at "
           << position.line + 1 << ':' << position.character + 1 << " [" << value.code << "]: "
           << value.message << '\n';
    for (const auto &note : value.notes) output << "note: " << note << '\n';
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

