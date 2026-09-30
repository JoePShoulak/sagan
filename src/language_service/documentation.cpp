#include "documentation.hpp"

namespace sagan::language_service
{
  namespace
  {
    auto trim(std::string_view text) -> std::string_view
    {
      while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
      while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
      return text;
    }

    auto parameter(std::string_view text) -> documentation_parameter
    {
      text = trim(text);
      const auto separator = text.find(' ');
      if (separator == std::string_view::npos) return {std::string(text), {}};
      return {std::string(text.substr(0, separator)), std::string(trim(text.substr(separator + 1)))};
    }
  }

  auto documentation_for(const semantic::indexed_symbol &symbol, std::string module)
    -> documentation_entry
  {
    documentation_entry entry{symbol.id, {}, {}, {}, {}, {}, {}, false, {}, std::move(module), {}};
    if (symbol.origin == semantic::symbol_origin::source) entry.source = symbol.declaration;
    for (const auto &line : symbol.documentation)
    {
      const auto text = trim(line);
      if (text.starts_with("@param ")) entry.parameters.push_back(parameter(text.substr(7)));
      else if (text.starts_with("@typeparam "))
        entry.generic_parameters.push_back(parameter(text.substr(11)));
      else if (text.starts_with("@return ")) entry.returns = std::string(trim(text.substr(8)));
      else if (text.starts_with("@example ")) entry.examples.emplace_back(trim(text.substr(9)));
      else if (text == "@deprecated" || text.starts_with("@deprecated ")) entry.deprecated = true;
      else if (text.starts_with("@since ")) entry.availability = std::string(trim(text.substr(7)));
      else if (entry.summary.empty()) entry.summary = text;
      else
      {
        if (!entry.detail.empty()) entry.detail += '\n';
        entry.detail += text;
      }
    }
    return entry;
  }
}
