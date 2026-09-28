#pragma once

#include "ast_node.hpp"

#include <string>
#include <string_view>

namespace parser
{
  auto render_ast_dot(const program &tree) -> std::string;
  auto render_ast_svg(const program &tree) -> std::string;
  auto render_ast_html(std::string_view source, const program &tree, std::string_view title) -> std::string;
}
