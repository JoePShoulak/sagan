#include "tokenizer.hpp"

#include <algorithm>

namespace parser {

auto tokenizer::next() -> std::optional<token> {
	token_pulled = true;
	if (tok) {
		prev_index = tok.value().range.end;
	}
	tok = z::core::generator<token, programText>::next();
	return tok;
}

auto tokenizer::get_span() -> span {
	return {
		prev_index,
		state.index,
	};
}

auto tokenizer::existing_token() -> std::optional<token> {
	return tok;
}

auto tokenizer::get_token() -> std::optional<token> {
	return token_pulled ? tok : next();
}

auto tokenizer::started() const -> bool {
	return token_pulled;
}

auto tokenizer::empty() const -> bool {
	return token_pulled && !tok;
}

auto tokenizer::recover_after_error(const span error_range) -> void {
	const int next = error_range.end > error_range.begin ? error_range.end : error_range.begin + 1;
	state.index = std::min(next, static_cast<int>(state.text.size()));
	state.pending.clear();
	state.in_string = false;
	state.string_raw = false;
	state.string_multiline = false;
	state.string_interpolated = false;
	state.string_quote = '\0';
	state.interpolation_depth = 0;
	state.string_stack.clear();
	tok.reset();
	token_pulled = true;
}

} // namespace parser
