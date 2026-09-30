#pragma once

#include "../source/source.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace sagan::diagnostics
{
  inline constexpr std::string_view schema_version = "sagan.language-service/1";

  enum class result_state { complete, recovered, incomplete, cancelled, stale };
  enum class severity { error, warning, information, hint };
  enum class phase { lexical, syntax, semantic, type, module, project, build, entry_point, runtime };

  struct related_location
  {
    source::source_range range;
    std::string message;
  };

  struct fix
  {
    std::string title;
    std::vector<source::text_edit> edits;
  };

  struct diagnostic
  {
    std::string code;
    severity level{severity::error};
    phase owner{phase::syntax};
    source::source_range primary;
    std::string message;
    std::vector<related_location> related;
    std::vector<std::string> notes;
    std::vector<fix> fixes;
  };

  template<class T> struct analysis_result
  {
    result_state state{result_state::complete};
    std::optional<T> value;
    std::vector<diagnostic> diagnostics;
    source::document_version analyzed_version{};
  };

  class cancellation_source;

  class cancellation_token
  {
    std::shared_ptr<const std::atomic_bool> state_;
    explicit cancellation_token(std::shared_ptr<const std::atomic_bool> state);
    friend class cancellation_source;

  public:
    cancellation_token() = default;
    auto is_cancelled() const -> bool;
  };

  class cancellation_source
  {
    std::shared_ptr<std::atomic_bool> state_{std::make_shared<std::atomic_bool>(false)};

  public:
    auto token() const -> cancellation_token;
    auto cancel() const -> void;
  };

  auto phase_name(phase value) -> std::string_view;
  auto default_code(phase value) -> std::string_view;
  auto severity_name(severity value) -> std::string_view;
  auto state_name(result_state value) -> std::string_view;
  auto render_terminal(std::ostream &output, const source::document_snapshot &document,
                       const diagnostic &value) -> void;
  auto render_json(const source::document_snapshot &document, result_state state,
                   const std::vector<diagnostic> &values) -> std::string;
}

