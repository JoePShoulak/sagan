#pragma once

#include "../source/source.hpp"

#include <string>
#include <vector>

namespace sagan::language_service
{
  enum class edit_state { ready, stale, invalid, conflict, unsupported };

  struct versioned_document_edits
  {
    source::document_uri uri;
    source::document_version expected_version{};
    std::vector<source::text_edit> edits;
  };

  struct workspace_edit
  {
    std::vector<versioned_document_edits> documents;
  };

  struct edit_plan
  {
    edit_state state{edit_state::unsupported};
    std::string reason;
    workspace_edit edits;
  };

  struct preview_document
  {
    source::document_uri uri;
    source::document_version expected_version{};
    std::string text;
  };

  struct edit_preview
  {
    edit_state state{edit_state::invalid};
    std::string reason;
    std::vector<preview_document> documents;
  };

  // Validate the exact snapshot versions, UTF-8 boundaries, and non-overlap,
  // then preview changes without writing to disk or editor buffers.
  auto preview_edits(const workspace_edit &edits,
                     const std::vector<const source::document_snapshot *> &snapshots) -> edit_preview;
}
