#include "refactor.hpp"

namespace sagan::language_service
{
  auto source_edit_capabilities() -> std::vector<source_edit_capability>
  {
    return {
        {"format.document", true, "Strict source only; ambiguous spacing and existing line breaks are preserved"},
        {"format.range", true, "Strict source only; complete intersecting lines are formatted"},
        {"format.onType", true, "Only closing brace and newline triggers"},
        {"rename.local", true, "Only one proven local symbol and its indexed references"},
        {"rename.function", true, "Only non-exported, non-overloaded, non-entry functions in one document"},
        {"rename.workspace", false, "Cross-module rebinding and visibility proof is not available"},
        {"imports.organize", true, "Only one uninterrupted comment-free block"},
        {"imports.add", true, "Requires one selected public workspace symbol and a module header"},
        {"imports.removeUnused", false, "Module initialization effects are not proven absent"},
        {"face.generateRequired", false, "Method-body and conformance synthesis is not proven safe"},
        {"diagnostic.fix", true, "Only fixes emitted and rechecked by the compiler"},
        {"extract.variable", false, "Evaluation order, lifetime, and effect proof is not available"},
        {"extract.function", false, "Capture, lifetime, and effect proof is not available"},
    };
  }
}
