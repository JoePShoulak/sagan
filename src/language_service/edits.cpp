#include "edits.hpp"
#include "../parser/unicode.hpp"

#include <algorithm>
#include <set>

namespace sagan::language_service
{
  auto preview_edits(const workspace_edit &edits,
                     const std::vector<const source::document_snapshot *> &snapshots) -> edit_preview
  {
    edit_preview preview{edit_state::ready, {}, {}};
    std::set<std::string> seen;
    for (const auto &document : edits.documents)
    {
      if (!seen.insert(document.uri.value).second)
        return {edit_state::conflict, "Duplicate document edit group", {}};
      const source::document_snapshot *snapshot = nullptr;
      for (const auto *candidate : snapshots)
        if (candidate && candidate->identity().uri == document.uri)
        {
          if (snapshot) return {edit_state::conflict, "Ambiguous document identity", {}};
          snapshot = candidate;
        }
      if (!snapshot) return {edit_state::invalid, "Document snapshot is missing", {}};
      if (snapshot->version() != document.expected_version)
        return {edit_state::stale, "Document version changed", {}};
      auto ordered = document.edits;
      std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right)
      {
        if (left.range.bytes.begin != right.range.bytes.begin)
          return left.range.bytes.begin < right.range.bytes.begin;
        return left.range.bytes.end < right.range.bytes.end;
      });
      source::byte_offset previous_begin = 0;
      source::byte_offset previous_end = 0;
      bool previous = false;
      for (const auto &edit : ordered)
      {
        if (unicode::first_invalid_utf8(edit.replacement_utf8))
          return {edit_state::invalid, "Replacement is not valid UTF-8", {}};
        if (edit.range.document != snapshot->identity().id ||
            edit.range.bytes.begin > edit.range.bytes.end ||
            !snapshot->to_utf16(edit.range.bytes.begin) ||
            !snapshot->to_utf16(edit.range.bytes.end))
          return {edit_state::invalid, "Edit range is not a valid boundary in its document", {}};
        if (previous && (edit.range.bytes.begin < previous_end ||
                         edit.range.bytes.begin == previous_begin))
          return {edit_state::conflict, "Edits overlap or share an insertion point", {}};
        previous_begin = edit.range.bytes.begin;
        previous_end = edit.range.bytes.end;
        previous = true;
      }
      std::string text(snapshot->text());
      for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
        text.replace(it->range.bytes.begin, it->range.bytes.end - it->range.bytes.begin,
                     it->replacement_utf8);
      preview.documents.push_back({document.uri, document.expected_version, std::move(text)});
    }
    std::sort(preview.documents.begin(), preview.documents.end(), [](const auto &left, const auto &right)
    { return left.uri.value < right.uri.value; });
    return preview;
  }
}
