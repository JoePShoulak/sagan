#pragma once

#include "tokenizer.hpp"
#include "parse_error.hpp"
#include "tokens.hpp"

auto get_token(parser::programText &state) -> std::optional<parser::token>;
