#pragma once

#include "tokenizer.hpp"
#include "parse_error.hpp"
#include "tokens.hpp"

#include <string>
#include <unordered_map>

auto get_token(parser::programText &state) -> std::optional<parser::token>;
auto language_keywords() -> const std::unordered_map<std::string, int> &;
